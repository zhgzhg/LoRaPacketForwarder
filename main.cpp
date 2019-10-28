#ifndef LINUX
        #define LINUX
#endif

#include <cstdio>
#include <ctime>
#include <csignal>
#include <functional>
#include <map>
#include <string>

#include "smtUdpPacketForwarder/ConfigFileParser.h"
#include "smtUdpPacketForwarder/UdpUtils.h"

#include <LoRaLib.h>


static volatile sig_atomic_t keepRunning = 1;

static LoRaPacketTrafficStats_t loraPacketStats;
static SX127x *lora;

bool receiveData(LoRaDataPkt_t &pkt, uint8_t msg[]) {

  int state = lora->receive(msg, SX127X_MAX_PACKET_LENGTH);

  time_t timestamp{std::time(nullptr)};

  if (state == ERR_NONE) { // packet was successfully received
    int msg_length = lora->getPacketLength(false);

    ++loraPacketStats.recv_packets;
    ++loraPacketStats.recv_packets_crc_good;

    // print data of the packet

    printf("\nReceived packet (%.24s):\n",
        std::asctime(std::localtime(&timestamp)));
    printf(" Data (%d bytes):\t\t\t%.*s\n", msg_length, msg_length, msg);

    // print RSSI (Received Signal Strength Indicator)
    // of the last received packet
    printf(" RSSI:\t\t\t%d dBm\n", lora->getRSSI());

    // print SNR (Signal-to-Noise Ratio)
    // of the last received packet
    printf(" SNR:\t\t\t%f dB\n", lora->getSNR());

    // print frequency error
    // of the last received packet
    printf(" Frequency error:\t%f Hz\n", lora->getFrequencyError());

    pkt.RSSI = lora->getRSSI();
    pkt.SNR = lora->getSNR();
    pkt.msg = static_cast<const uint8_t*> (msg);
    pkt.msg_sz = msg_length;
    return true;

  } /*else if (state == ERR_RX_TIMEOUT) {
    // timeout occurred while waiting for a packet
    //printf("timeout!\n");
    return false;
  }*/ else if (state == ERR_CRC_MISMATCH) {
    // packet was received, but is malformed
    ++loraPacketStats.recv_packets;
    printf("Received packet CRC error - ignored (%.24s)!\n",
        std::asctime(std::localtime(&timestamp)));
  }

  return false;
}

uint16_t restartLoRaChip(PlatformInfo_t &cfg) {

  if (cfg.lora_chip_settings.pin_rest > -1) {
    digitalWrite(cfg.lora_chip_settings.pin_rest, 0);
    delay(2);
    digitalWrite(cfg.lora_chip_settings.pin_rest, 1);
    delay(12);
  }

  int8_t power = 17, currentLimit_ma = 100, gain = 0;

  return lora->begin(
    cfg.lora_chip_settings.carrier_frequency_mhz,
    cfg.lora_chip_settings.bandwidth_khz,
    cfg.lora_chip_settings.spreading_factor,
    cfg.lora_chip_settings.coding_rate,
    cfg.lora_chip_settings.sync_word,
    power,
    currentLimit_ma,
    cfg.lora_chip_settings.preamble_length,
    gain
  );
}

SX127x* instantiateLoRa(LoRaChipSettings_t& lora_chip_settings) {
  static const std::map<std::string, std::function<SX127x*(LoRa*)> > LORA_CHIPS {
    { "SX1272", [](LoRa* lora_module_settings) { return new SX1272(lora_module_settings); } },
    { "SX1273", [](LoRa* lora_module_settings) { return new SX1273(lora_module_settings); } },
    { "SX1276", [](LoRa* lora_module_settings) { return new SX1276(lora_module_settings); } },
    { "SX1277", [](LoRa* lora_module_settings) { return new SX1277(lora_module_settings); } },
    { "SX1278", [](LoRa* lora_module_settings) { return new SX1278(lora_module_settings); } },
    { "SX1279", [](LoRa* lora_module_settings) { return new SX1279(lora_module_settings); } },
    { "RFM95", [](LoRa* lora_module_settings) { return new RFM95(lora_module_settings); } },
    { "RFM96", [](LoRa* lora_module_settings) { return new RFM96(lora_module_settings); } },
    { "RFM97", [](LoRa* lora_module_settings) { return new RFM97(lora_module_settings); } },
    { "RFM98", [](LoRa* lora_module_settings) { return new RFM96(lora_module_settings); } } // LIKE RFM96
  };

  LoRa* module_settings = new LoRa(
    lora_chip_settings.pin_nss_cs,
    lora_chip_settings.pin_dio0,
    lora_chip_settings.pin_dio1
  );

  return LORA_CHIPS.at(lora_chip_settings.ic_model)(module_settings);
}

int main(int argc, char **argv) {
  char networkIfaceName[64] = "eth0";
  char gatewayId[25];
  memset(gatewayId, 0, sizeof(gatewayId));

  if (argc > 1) {
    strcpy(networkIfaceName, (const char*) argv[1]);
  }

  NetworkConf_t netCfg = PrepareNetworking(networkIfaceName, gatewayId);
  PlatformInfo_t cfg = LoadConfiguration("./config.json", gatewayId);

  PrintConfiguration(cfg);

  SPISettings spiSettings{cfg.lora_chip_settings.spi_speed_hz, MSBFIRST,
    SPI_MODE0, cfg.lora_chip_settings.spi_channel};
  SPI.beginTransaction(spiSettings);

  lora = instantiateLoRa(cfg.lora_chip_settings);

  uint16_t state = ERR_NONE + 1;

  for (uint8_t c = 0; state != ERR_NONE && c < 200; ++c) {
    state = restartLoRaChip(cfg);

    if (state == ERR_NONE) printf("LoRa chip setup success!\n");
    else printf("LoRa chip setup failed, code %d\n", state);
  }

  if (state != ERR_NONE) {
    printf("Giving up due to failing LoRa chip setup!\nExiting!\n");
    SPI.endTransaction();
    return 1;
  }


  auto signalHandler = [](int sigNum) { keepRunning = 0; };

  signal(SIGHUP, signalHandler);  // Process' terminal is closed, the
                                  // user has logger out, etc.
  signal(SIGINT, signalHandler);  // Interrupted process (Ctrl + c)
  signal(SIGQUIT, signalHandler); // Quit request (via console Ctrl + \)
  signal(SIGTERM, signalHandler); // Termination request
  signal(SIGXFSZ, signalHandler); // Creation of a file so large that
                                  // it's not allowed anymore to grow

  const uint16_t delayIntervalMs = 50;
  const uint32_t sendStatPktIntervalMs = 80000;
  const uint32_t loraChipRestIntervalMs = 80000;

  uint32_t accumPktStats = 0;
  uint32_t accumRestStats = delayIntervalMs;

  LoRaDataPkt_t loraDataPacket;
  uint8_t msg[SX127X_MAX_PACKET_LENGTH];

  while (keepRunning) {
    if (keepRunning && (accumPktStats % sendStatPktIntervalMs) == 0) {
      printf("Sending stat update to server(s)... ");
      PublishStatProtocolPacket(netCfg, cfg, loraPacketStats);
      ++loraPacketStats.forw_packets_crc_good;
      ++loraPacketStats.forw_packets;
      printf("done\n");
    }

    if (keepRunning && receiveData(loraDataPacket, msg)) {
      PublishLoRaProtocolPacket(netCfg, cfg, loraDataPacket);
    } else if (keepRunning) {

      if (cfg.lora_chip_settings.pin_rest > -1 && (accumRestStats % loraChipRestIntervalMs) == 0) {
        do {
          state = restartLoRaChip(cfg);
          printf("Regular LoRa chip reset done - code %d, %s success\n", state,
              (state == ERR_NONE ? "with" : "WITHOUT"));
          delay(delayIntervalMs);
        } while (state != ERR_NONE);
      } else {
        delay(delayIntervalMs);
      }

    }

    accumPktStats += delayIntervalMs;
    accumRestStats += delayIntervalMs;
  }

  printf("\nShutting down...\n");
  SPI.endTransaction();
}
