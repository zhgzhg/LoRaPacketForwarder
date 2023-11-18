/**
 * A tiny temperature monitor program. In response to a certain numeric
 * (temperature) level the output GPIO pins can be set or cleared.
 */

#include <iostream>
#include <ctime>
#include <chrono>
#include <thread>
#include <csignal>

#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include <functional>

#include "../../smtUdpPacketForwarder/rapidjson/document.h"
#include "../../smtUdpPacketForwarder/rapidjson/istreamwrapper.h"
#include "../../smtUdpPacketForwarder/rapidjson/stringbuffer.h"
#include "../../smtUdpPacketForwarder/rapidjson/writer.h"

#include <wiringPi.h>

static volatile sig_atomic_t keep_running = 1;

static const std::map<std::string, std::function<bool(long double, long double)> > TEMPR_COMP_OPS {
	{ "=", [](auto curr, auto target) { return curr == target; } },
	{ "<", [](auto curr, auto target) { return curr < target; } },
	{ ">", [](auto curr, auto target) { return curr > target; } },
	{ "<=", [](auto curr, auto target) { return curr <= target; } },
	{ ">=", [](auto curr, auto target) { return curr >= target; } }
};

struct GpioPin
{
	int wpi_pin_number;
	bool output_value;
	std::string condition;
	bool active_on_terminate;
	long double condition_temp_degC;
	std::string temp_degC_source;
	long double prev_read_temp_degC;

	std::string duration_condition;
	std::chrono::seconds duration_seconds;
	std::chrono::seconds curr_duration_seconds;
	bool duration_condition_already_matched;
	time_t last_duration_condition_already_matched_flag_set_time;
};

std::vector<GpioPin> parse_config(std::ifstream& fs)
{
	rapidjson::IStreamWrapper isw_config_file{fs};
	rapidjson::Document d;

	d.ParseStream(isw_config_file);

	std::vector<GpioPin> result;

	for (auto &o : d.GetArray()) {
		GpioPin p;
		p.wpi_pin_number = o["wpi_pin"].GetInt();
		p.output_value = o["output_val"].GetBool();
		p.condition = o["condition"].GetString();
		p.condition_temp_degC = o["temperature_degC"].GetDouble();
		p.active_on_terminate = o.HasMember("match_on_terminate") && o["match_on_terminate"].GetBool();
		p.temp_degC_source = o["temperature_src"].GetString();
		p.prev_read_temp_degC = 0.0;
		p.duration_condition =
			(o.HasMember("duration_condition") ? o["duration_condition"].GetString() : ">=");
		p.duration_seconds = (o.HasMember("duration_seconds") ?
			std::chrono::seconds{o["duration_seconds"].GetInt64()} : std::chrono::seconds::zero());
		p.curr_duration_seconds = std::chrono::seconds::zero();
		p.duration_condition_already_matched = false;
		result.push_back(p);
	}

	return result;
}

long double read_temp_degC(std::string& src_file_path)
{
	std::ifstream in{src_file_path, std::ifstream::in};
	if (!in.is_open())
	{ return std::nan(""); }

	int raw_temp = 0;
       	in >> raw_temp;
	in.close();

	return (raw_temp / 1000.0);
}

int main(int argc, char* argv[])
{
	std::cout << '[' << std::time(nullptr) << "] Started " << argv[0];

	std::string config_file_path = "./config.json";
	if (argc > 1)
	{ config_file_path = argv[1]; }

	std::cout << " config file " << config_file_path << std::endl;

	std::ifstream config_file{config_file_path, std::ifstream::in};

	if (!config_file.is_open())
	{
		std::cerr << "Cannot open configuration file config.json!" << std::endl;
		return 1;
	}

	std::vector<GpioPin> pin_outputs = parse_config(config_file);
	config_file.close();

	if (pin_outputs.size() == 0)
	{
		std::cerr << "No conditions supplied in '"
			<< config_file_path << "' !" << std::endl;
		return 2;
	}

	wiringPiSetup();

	for (GpioPin& p : pin_outputs)
	{ pinMode(p.wpi_pin_number, OUTPUT); }

	auto signal_handler = [](int sig_num) { keep_running = 0; };

	signal(SIGHUP, signal_handler);  // Process' terminal is closed, the
					 // user has logger out, etc.
	signal(SIGINT, signal_handler);  // Interrupted process (Ctrl + c)
	signal(SIGQUIT, signal_handler); // Quit request (via console Ctrl + \)
	signal(SIGTERM, signal_handler); // Termination request
	signal(SIGXFSZ, signal_handler); // Creation of a file so large that
					 // it's not allowed anymore to grow

	bool break_loop = false;

	do
	{
		if (keep_running == 0)
		{ break_loop = true; }


		for (GpioPin& p : pin_outputs)
		{
			long double temp_degC = read_temp_degC(p.temp_degC_source);

			if (p.prev_read_temp_degC != temp_degC && std::isnan(temp_degC))
			{
				std::cerr << '[' << std::time(nullptr)
					<< "] Cannot read current temperature for wPi pin "
					<< p.wpi_pin_number << " " << p.temp_degC_source
					<< std::endl;
				continue;
			}

			if (TEMPR_COMP_OPS.count(p.condition) < 1
				|| TEMPR_COMP_OPS.count(p.duration_condition) < 1)
			{
				std::cerr << '[' << std::time(nullptr)
					<< "] Not supported comparison operation for wPi pin "
					 << p.wpi_pin_number << " " << p.condition
					 << std::endl;
				continue;
			}
			if (p.duration_seconds.count() < 0)
			{
				std::cerr << '[' << std::time(nullptr)
					<< "] Negative duration_seconds value for wPi pin "
					<< p.wpi_pin_number << " of " << p.duration_seconds.count()
					<< std::endl;
			}

			bool terminate_match = (p.active_on_terminate && keep_running == 0);
			bool inst_val_match =
				TEMPR_COMP_OPS.at(p.condition)(temp_degC, p.condition_temp_degC);
			if (!inst_val_match)
			{ p.curr_duration_seconds = decltype(p.curr_duration_seconds)::zero(); }

			bool dura_val_match = TEMPR_COMP_OPS.at(p.duration_condition)(
				p.curr_duration_seconds.count(), p.duration_seconds.count());

			if (!dura_val_match)
			{
				p.duration_condition_already_matched = false;
				p.last_duration_condition_already_matched_flag_set_time = std::time(nullptr);
			}
			else
			{
				double diff = std::difftime(std::time(nullptr),
					p.last_duration_condition_already_matched_flag_set_time);
				p.duration_condition_already_matched = (fabs(diff) < 5);
			}

			if (terminate_match || (!p.duration_condition_already_matched
					&& inst_val_match && dura_val_match))
			{
				p.duration_condition_already_matched = true;

				int outp_val = static_cast<int>(p.output_value);
				pinMode(p.wpi_pin_number, OUTPUT);
				int curr_outp_val = digitalRead(p.wpi_pin_number);

				if (curr_outp_val != outp_val)
				{
					digitalWrite(p.wpi_pin_number, outp_val);
					std::cout << '[' << std::time(nullptr) << "] "
						<< temp_degC << (terminate_match ? " !" : " ")
						<< p.condition << ' ' << p.condition_temp_degC
						<< ", for " << p.curr_duration_seconds.count()
						<< "s" << (terminate_match ? " !" : " ")
						<< p.duration_condition << ' '
						<< p.duration_seconds.count()
						<< "s :: wPi pin " << p.wpi_pin_number
						<< " = " <<  outp_val
						<< (terminate_match ? " :: TERMINATION TRIGGERED" : "")
						<< std::endl;
				}
			}

			p.prev_read_temp_degC = temp_degC;
			++p.curr_duration_seconds;
		}

		for (unsigned char c = 0; keep_running == 1 && c < 5; ++c)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}

	} while (!break_loop);

	std::cout << '[' << std::time(nullptr) << "] Stopped " << argv[0] << std::endl;
	return 0;
}
