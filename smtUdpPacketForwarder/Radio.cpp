#include "Radio.h"
#include "TimeUtils.h"

#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <typeinfo>
#include <cstdarg>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "gpsTimestampUtils/GpsTimestampUtils.h"
#include "base64/base64.h"

static uint8_t gfskSyncWord[] = { 0xC1, 0x94, 0xC1 };

void hexPrint(uint8_t data[], int length, FILE *dest) { // {{{
  if (length < 1) {
    fprintf(dest, "\n");
    fflush(dest);
    return;
  }

  for (int i = 0; i < length; i += 16) {
    fprintf(dest, "  %04x  ", (i * 16));
    int j = i;
    for (int limit = i + 16 ; j < limit && j < length; ++j) {
      if (j % 8 == 0) fprintf(dest, "  ");
      fprintf(dest, "%02x ", data[j]);
    }

    for (int k = j; k % 16 != 0; ++k) {
      if (k % 8 == 0) fprintf(dest, "  ");
      fprintf(dest, "   ");
    }

    fprintf(dest, "  ");

    for (int k = i; k < j; ++k)
      fprintf(dest, "%c", isprint(data[k]) ? data[k] : '.');

    fprintf(dest, "\n");
  }
  fprintf(dest, "\n");
  fflush(dest);
} // }}}

#define MODULE_RESET(chip_class, origin, is_reset) if (!is_reset) { \
	  chip_class* module = dynamic_cast<chip_class*>(origin); \
	  if (module != nullptr) { \
	    is_reset = true; \
	    module->reset(); \
	    delay(10); \
	  } \
	}

#define MODULE_REINIT(chip_class, origin, is_reinitted, result, pi_cfg, power, current_lim_ma, gain) \
	if (!is_reinitted && origin != nullptr && typeid(*origin) == typeid(chip_class)) { \
	  is_reinitted = true; \
	  chip_class* chip = static_cast<chip_class*>(origin); \
	  result = chip->begin( \
	    pi_cfg.lora_chip_settings.carrier_frequency_mhz, \
	    pi_cfg.lora_chip_settings.bandwidth_khz, \
	    pi_cfg.lora_chip_settings.spreading_factor, \
	    pi_cfg.lora_chip_settings.coding_rate, \
	    pi_cfg.lora_chip_settings.sync_word, \
	    power, \
	    pi_cfg.lora_chip_settings.preamble_length, \
	    gain \
	  ); \
	  if (result == ERR_NONE) result = chip->setCurrentLimit(current_lim_ma); \
	  if (result == ERR_NONE) { \
	    SX127x* sx127x_chip = dynamic_cast<SX127x*>(origin); \
	    if (sx127x_chip != nullptr) { \
	      result = sx127x_chip->invertIQ(false); \
	      if (result == ERR_NONE) { \
	        result = sx127x_chip->setCRC(true); \
	      } \
	    } else { \
	      SX126x* sx126x_chip = dynamic_cast<SX126x*>(origin); \
	      if (sx126x_chip != nullptr) { \
	        result = sx126x_chip->invertIQ(false); \
	        if (result == ERR_NONE) { \
	          result = sx126x_chip->setCRC((uint8_t) 1); \
	        } \
	      } \
	    } \
	  } \
	}

#define MODULE_REINIT_FOR_TX(chip_class, origin, is_reinitted, result, pi_cfg, downlink_pkt, current_lim_ma, gain) \
	if (!is_reinitted && origin != nullptr && typeid(*origin) == typeid(chip_class)) { \
	  is_reinitted = true; \
	  chip_class* chip = static_cast<chip_class*>(origin); \
	  if (downlink_pkt.fsk_datarate_bps == 0) { \
	    result = chip->begin( \
	      downlink_pkt.carrier_frequency_mhz, \
	      downlink_pkt.bandwidth_khz, \
	      downlink_pkt.spreading_factor, \
	      downlink_pkt.coding_rate, \
	      pi_cfg.lora_chip_settings.sync_word, \
	      (int8_t) downlink_pkt.output_power_dbm, \
	      downlink_pkt.preamble_length, \
	      gain \
	    ); \
	    if (result == ERR_NONE) { \
	      SX127x* sx127x_chip = dynamic_cast<SX127x*>(origin); \
	      if (sx127x_chip != nullptr) { \
	        result = sx127x_chip->invertIQ(downlink_pkt.iq_polatization_inversion); \
	        if (result == ERR_NONE) { \
	          result = sx127x_chip->setCRC(!downlink_pkt.disable_crc); \
	        } \
	      } else { \
	        SX126x* sx126x_chip = dynamic_cast<SX126x*>(origin); \
	        if (sx126x_chip != nullptr) { \
	          result = sx126x_chip->invertIQ(downlink_pkt.iq_polatization_inversion); \
	          if (result == ERR_NONE) { \
	            result = sx126x_chip->setCRC(!downlink_pkt.disable_crc ? (uint8_t) 0 : (uint8_t) 1); \
	          } \
	        } \
	      } \
	    } \
	  } else { \
	    result = chip->beginFSK( \
	      downlink_pkt.carrier_frequency_mhz, \
	      downlink_pkt.fsk_datarate_bps / 1000.0, \
	      downlink_pkt.fsk_freq_deviation_hz / 1000.0, \
	      downlink_pkt.bandwidth_khz, \
	      (int8_t) downlink_pkt.output_power_dbm, \
	      downlink_pkt.preamble_length, \
	      false /* don't use OOK */ \
	    ); \
      if (result == ERR_NONE) result = chip->setSyncWord(gfskSyncWord, ((uint8_t) sizeof(gfskSyncWord))); \
	  } \
	  if (result == ERR_NONE) result = chip->setCurrentLimit(current_lim_ma); \
	}

void doRestartLoRaChip(PhysicalLayer *lora, PlatformInfo_t &cfg) { // {{{
  if (cfg.lora_chip_settings.pin_rest > -1) {
    bool is_reset = false;
    MODULE_RESET(SX1261, lora, is_reset); MODULE_RESET(SX1262, lora, is_reset);  MODULE_RESET(SX1268, lora, is_reset); 
    MODULE_RESET(SX1272, lora, is_reset); MODULE_RESET(SX1273, lora, is_reset);  MODULE_RESET(SX1276, lora, is_reset); 
    MODULE_RESET(SX1277, lora, is_reset); MODULE_RESET(SX1278, lora, is_reset);  MODULE_RESET(SX1279, lora, is_reset); 
    MODULE_RESET(RFM95, lora, is_reset); MODULE_RESET(RFM96, lora, is_reset); MODULE_RESET(RFM97, lora, is_reset); 
  }
} // }}}

uint16_t restartLoRaChip(PhysicalLayer *lora, PlatformInfo_t &cfg) { // {{{
  doRestartLoRaChip(lora, cfg);

  int8_t power = 17, currentLimit_ma = 100, gain = 0;
  bool is_reinitted = false;
  uint16_t result = ERR_NONE + 1;

  MODULE_REINIT(SX1261, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);
  MODULE_REINIT(SX1262, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);
  MODULE_REINIT(SX1268, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);
  MODULE_REINIT(SX1272, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);
  MODULE_REINIT(SX1273, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);
  MODULE_REINIT(SX1276, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);
  MODULE_REINIT(SX1277, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);
  MODULE_REINIT(SX1278, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);
  MODULE_REINIT(SX1279, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);
  MODULE_REINIT(RFM95, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);
  MODULE_REINIT(RFM96, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);
  MODULE_REINIT(RFM97, lora, is_reinitted, result, cfg, power, currentLimit_ma, gain);

  return result;
} // }}}

#define MODULE_INIT_PAIR(origin_class_name, origin_class) { #origin_class_name, [](Module* lora_module_settings) { return new origin_class(lora_module_settings); } }

PhysicalLayer* instantiateLoRaChip(LoRaChipSettings_t& lora_chip_settings, SPIClass &spiClass, SPISettings &spiSettings) { // {{{
  static const std::map<std::string, std::function<PhysicalLayer*(Module*)> > LORA_CHIPS {
    MODULE_INIT_PAIR(SX1261, SX1261), MODULE_INIT_PAIR(SX1262, SX1262), MODULE_INIT_PAIR(SX1268, SX1268),
    MODULE_INIT_PAIR(SX1272, SX1272), MODULE_INIT_PAIR(SX1273, SX1273), MODULE_INIT_PAIR(SX1276, SX1276),
    MODULE_INIT_PAIR(SX1277, SX1277), MODULE_INIT_PAIR(SX1278, SX1278), MODULE_INIT_PAIR(SX1279, SX1279),
    MODULE_INIT_PAIR(RFM95, RFM95), MODULE_INIT_PAIR(RFM96, RFM96), MODULE_INIT_PAIR(RFM97, RFM97),
    MODULE_INIT_PAIR(RFM98, RFM96) // like RFM96
 };

  Module *module_settings = new Module(
    lora_chip_settings.pin_nss_cs,
    lora_chip_settings.pin_dio0,
    (lora_chip_settings.pin_rest > -1 ? lora_chip_settings.pin_rest : RADIOLIB_NC),
    lora_chip_settings.pin_dio1,
    spiClass,
    spiSettings
  );

  return LORA_CHIPS.at(lora_chip_settings.ic_model)(module_settings);
} // }}}

#define PKT_ENRICHER(origin_class, instance_ptr, inst_type_info, lora_packet, freq_err) \
	if (inst_type_info == typeid(origin_class)) { \
	  origin_class *inst = static_cast<origin_class*>(instance_ptr); \
	  lora_packet.RSSI = inst->getRSSI(true); \
	  lora_packet.SNR = inst->getSNR(); \
	  freq_err = inst->getFrequencyError(); \
	  return; \
	}
#define PKT_ENRICHER_NO_FREQ_ERR(origin_class, instance_ptr, inst_type_info, lora_packet, freq_err) \
	if (inst_type_info == typeid(origin_class)) { \
	  origin_class *inst = static_cast<origin_class*>(instance_ptr); \
	  lora_packet.RSSI = inst->getRSSI(); \
	  lora_packet.SNR = inst->getSNR(); \
	  freq_err = 0.0f; \
	  return; \
	}

void enrichWithRadioStats(PhysicalLayer *lora, LoRaDataPkt_t &pkt, float &freqErr) { // {{{
  const std::type_info &loraInstTypeInfo = typeid(*lora);

  PKT_ENRICHER_NO_FREQ_ERR(SX1261, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER_NO_FREQ_ERR(SX1262, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER_NO_FREQ_ERR(SX1268, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER(SX1272, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER(SX1273, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER(SX1276, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER(SX1277, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER(SX1278, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER(SX1279, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER(RFM95, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER(RFM96, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER(RFM97, lora, loraInstTypeInfo, pkt, freqErr);

  // else:
  pkt.RSSI = 1.0f;
  pkt.SNR = 1.0f;
  freqErr = 0.0f;
} // }}}

#define ITER_ALL_SF(origin_class, lora, lora_type, is_matched, state, insist_data_recv_fail, sf_min, sf_max, curr_sf) \
	if (!is_matched && lora_type == typeid(origin_class)) { \
	  is_matched = true; \
	  origin_class* inst = static_cast<origin_class*>(lora); \
	  for (unsigned i = sf_min; i <= sf_max; ++i) {\
	    inst->setSpreadingFactor(i); \
	    curr_sf = decltype(curr_sf)(i); \
	    state = inst->scanChannel(); \
	    if (state == PREAMBLE_DETECTED) /*&& lora->getRSSI() > -124.0) */{ \
	      state = inst->receive(msg, SX127X_MAX_PACKET_LENGTH); \
	      insist_data_recv_fail = (state != ERR_NONE); \
	      printf("Got preamble at SF%d, RSSI %f!\n", i, inst->getRSSI()); \
	      break; \
	    } \
	  } \
	}

static void logMessage(const char *format, ...) {
  time_t timestamp{std::time(nullptr)};
  char asciiTime[25];
  printf("(%s) ", ts_asciitime(timestamp, asciiTime, sizeof(asciiTime)));

  va_list argptr;
  va_start(argptr, format);
  vprintf(format, argptr);
  va_end(argptr);

  fflush(stdout);
}

LoRaRecvStat recvLoRaUplinkData(PhysicalLayer *lora, PlatformInfo_t &cfg, LoRaDataPkt_t &pkt,
                                uint8_t msg[], LoRaPacketTrafficStats_t &loraPacketStats) { // {{{

  int state = ERR_RX_TIMEOUT;
  bool insistDataReceiveFailure = false;

  SpreadingFactor_t usedSF;

  if (!cfg.lora_chip_settings.all_spreading_factors) {
    state = lora->receive(msg, SX127X_MAX_PACKET_LENGTH);
    usedSF = cfg.lora_chip_settings.spreading_factor;
  } else {
    const std::type_info &loraTypeInfo = typeid(*lora);
    bool is_matched = false;
    ITER_ALL_SF(SX1261, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX, usedSF);
    ITER_ALL_SF(SX1262, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX, usedSF);
    ITER_ALL_SF(SX1268, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX, usedSF);
    ITER_ALL_SF(SX1273, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX, usedSF);
    ITER_ALL_SF(SX1276, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX, usedSF);
    ITER_ALL_SF(SX1277, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX, usedSF);
    ITER_ALL_SF(SX1278, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX, usedSF);
    ITER_ALL_SF(SX1279, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX, usedSF);
    ITER_ALL_SF(RFM95, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX, usedSF);
    ITER_ALL_SF(RFM96, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX, usedSF);
    ITER_ALL_SF(RFM97, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX, usedSF);
  }

  if (state == ERR_NONE) { // packet was successfully received

    int msg_length = lora->getPacketLength(false);
    float freqErr = 0.0f;

    enrichWithRadioStats(lora, pkt, freqErr);

    ++loraPacketStats.recv_packets;
    ++loraPacketStats.recv_packets_crc_good;

    const char *pktRecvStats = "Received UPlink packet:\n" \
      " RSSI:\t\t\t%.1f dBm\n" \
      " SNR:\t\t\t%f dB\n" \
      " Frequency error:\t%f Hz\n" \
      " Data:\t\t\t%d bytes\n\n";

    logMessage(pktRecvStats, pkt.RSSI, pkt.SNR, freqErr, msg_length);
    hexPrint(msg, msg_length, stdout);

    pkt.msg = static_cast<const uint8_t*> (msg);
    pkt.msg_sz = msg_length;
    pkt.freq_mhz = cfg.lora_chip_settings.carrier_frequency_mhz;
    pkt.bandwidth_khz = cfg.lora_chip_settings.bandwidth_khz;
    pkt.sf = usedSF;

    return LoRaRecvStat::DATARECV;

  } else if (state == ERR_CRC_MISMATCH) {
    // packet was received, but is malformed

    ++loraPacketStats.recv_packets;
    logMessage("Received UPlink packet with CRC error - ignored!\n");
    return LoRaRecvStat::DATARECVFAIL;
  }

  return (insistDataReceiveFailure ? LoRaRecvStat::DATARECVFAIL : LoRaRecvStat::NODATA);
} // }}}

static struct DownlinkPacket
{
  bool initialised = false;

  bool send_immediately;
  std::time_t unix_epoch_timestamp;

  unsigned long concentrator_rf_chain;

  float output_power_dbm = 0;
  SpreadingFactor_t spreading_factor = SpreadingFactor_t::SF_ALL;
  float carrier_frequency_mhz = 0;
  float bandwidth_khz = 0;
  CodingRate_t coding_rate = CodingRate_t::CR_MIN;
  unsigned char preamble_length = 8;

  unsigned long fsk_datarate_bps = 0;
  unsigned long fsk_freq_deviation_hz = 0;

  bool iq_polatization_inversion = false;

  unsigned long payload_size = 0;
  unsigned char payload[255];

  bool disable_crc = true;
} NO_DP_DATA;

static DownlinkPacket downlinkTxJsonToPacket(PackagedDataToSend_t &pkt) {
    rapidjson::Document doc;
    doc.Parse(reinterpret_cast<const char*>(pkt.data.get()));
    if (doc.HasParseError()) {
      logMessage("Failed to parse the JSON payload!\n");
      return NO_DP_DATA;
    }

    if (!doc.IsObject() || !doc.HasMember("txpk") || !doc["txpk"].IsObject()) {
      logMessage("No txpk recognized!\n");
      return NO_DP_DATA;
    }

    DownlinkPacket result;

    const rapidjson::Value& txpkt = doc["txpk"].GetObject();
    result.send_immediately = (txpkt.HasMember("imme") ? txpkt["imme"].GetBool() : false);
    if (!result.send_immediately && (txpkt.HasMember("tmst") || txpkt.HasMember("tmms"))) {
      std::time_t now{std::time(nullptr)};
      uint32_t internalTsMicros = micros();

      if (txpkt.HasMember("tmst")) {
        uint32_t scheduledTsMicros = txpkt["tmst"].GetUint();
        int diffSeconds = ((long)scheduledTsMicros - (long)internalTsMicros) / 1000000;
        result.unix_epoch_timestamp = add_seconds(now, diffSeconds);
      } else {
        result.unix_epoch_timestamp = static_cast<std::time_t>(gps2unix(txpkt["tmms"].GetDouble(), false));
      }

      if (result.unix_epoch_timestamp > add_seconds(now, 24 * 3600) || result.unix_epoch_timestamp < add_seconds(now, -20)) {
        logMessage("Invalid time scheduled: %lu; local ts %lu, internal ts(micros) %lu!\n",
            result.unix_epoch_timestamp, now, internalTsMicros);
        return NO_DP_DATA;
      }
    } else {
      logMessage("Missing tx schedule!\n");
      return NO_DP_DATA;
    }

    if (!txpkt.HasMember("modu")) {
      logMessage("Missing tx modulation information!\n");
      return NO_DP_DATA;
    }
    if (!txpkt.HasMember("datr")) {
      logMessage("Missing tx data rate information!\n");
    }

    if (txpkt.HasMember("freq"))
    { result.carrier_frequency_mhz = static_cast<float>(txpkt["freq"].GetDouble()); }
    if (txpkt.HasMember("rfch"))
    { result.concentrator_rf_chain = txpkt["rfch"].GetUint(); }
    if (txpkt.HasMember("powe"))
    { result.output_power_dbm = static_cast<float>(txpkt["powe"].GetDouble()); }

    bool isLoraModulation = (strcmp(txpkt["modu"].GetString(), "LORA") == 0 &&
        txpkt["datr"].IsString() && txpkt.HasMember("codr"));
    if (isLoraModulation) {
      int sf = 0;
      float bw = 0;

      if (sscanf(txpkt["datr"].GetString(), "%*[SF]%d%*[BW]%f", &sf, &bw) < 2 ||
          sf < SF_MIN || sf > SF_MAX || bw < 1.0F || bw > 500.0F) {
        logMessage("Invalid SF or BW!\n");
        return NO_DP_DATA;
      }
      result.spreading_factor = SpreadingFactor_t(sf);
      result.bandwidth_khz = bw;

      int cr = 0;
      if (sscanf(txpkt["codr"].GetString(), "%*[4/]%d", &cr) < 1 ||
          cr < CodingRate_t::CR_MIN || cr > CodingRate_t::CR_MAX) {
        logMessage("Invalid codr!\n");
        return NO_DP_DATA;
      }
      result.coding_rate = static_cast<CodingRate_t>(cr);

      if (txpkt.HasMember("ipol")) {
        result.iq_polatization_inversion = txpkt["ipol"].GetBool();
      }

    } else if (txpkt["datr"].IsNumber() && txpkt.HasMember("fdev")) {
      result.fsk_datarate_bps = txpkt["datr"].GetUint();
      result.fsk_freq_deviation_hz = txpkt["fdev"].GetUint();
    } else {
      logMessage("Invalid data rate / frequency deviation for the specified modulation!\n");
      return NO_DP_DATA;
    }

    if (txpkt.HasMember("prea")) {
        int preamble = txpkt["prea"].GetUint();
        if (preamble > 0 && preamble < 256) {
          result.preamble_length = static_cast<unsigned char>(preamble);
        } else {
          logMessage("Invalid preamble length!\n");
          return NO_DP_DATA;
        }
    } else if (result.fsk_datarate_bps != 0) {
      result.preamble_length = 5;
    }

    if (txpkt.HasMember("size")) {
      result.payload_size = txpkt["size"].GetUint();
      if (result.payload_size > 255) {
        logMessage("Invalid payload size!\n");
        return NO_DP_DATA;
      }
    }

    if (txpkt.HasMember("data")) {
      b64_to_bin(txpkt["data"].GetString(), result.payload_size, result.payload, 255);
    }

    if (txpkt.HasMember("ncrc")) {
      result.disable_crc = txpkt["ncrc"].GetBool();
    }

    result.initialised = true;
    return result;
}

LoRaRecvStat sendLoRaDownlinkData(PhysicalLayer *lora, PlatformInfo_t &cfg, PackagedDataToSend_t &pkt,
                                  LoRaPacketTrafficStats_t &loraPacketStats) // {{{
{
  bool newPacket = false;
  if (!pkt.logged)
  {
    logMessage("Received DOWNlink packet:\n");
    hexPrint(pkt.data.get(), pkt.data_len, stdout);
    pkt.logged = true;
    newPacket = true;
  }

  DownlinkPacket converted = downlinkTxJsonToPacket(pkt);
  if (!converted.initialised)
  { return LoRaRecvStat::DATARECVFAIL; }

  time_t now{std::time(nullptr)};
  time_t when = (converted.send_immediately ? now : (newPacket ? converted.unix_epoch_timestamp : pkt.schedule));

  if (!converted.send_immediately)
  {
    if (now < add_seconds(when, -5))
    {
      if (newPacket)
      {
        pkt.schedule = converted.unix_epoch_timestamp;
        char asciiTime[25];
        ts_asciitime(converted.unix_epoch_timestamp, asciiTime, sizeof(asciiTime));
        logMessage("Scheduling DOWNlink packet for %s\n", asciiTime);
      }
      RequeuePacket(std::move(pkt), 9000000, DOWN_RX);
      return LoRaRecvStat::DATARECVFAIL;
    }
    else if (now > add_seconds(when, 10))
    {
      char asciiTime[25];
      ts_asciitime(converted.unix_epoch_timestamp, asciiTime, sizeof(asciiTime));
      logMessage("DOWNlink packet's schedule's too late: %s\n", asciiTime);
      return LoRaRecvStat::DATARECVFAIL;
    }
  }

  if (converted.spreading_factor == SF_ALL)
  { converted.spreading_factor = cfg.lora_chip_settings.spreading_factor; }
  if (std::abs(converted.carrier_frequency_mhz - cfg.lora_chip_settings.carrier_frequency_mhz) >= 40.0f)
  {
    converted.carrier_frequency_mhz = cfg.lora_chip_settings.carrier_frequency_mhz;
    converted.bandwidth_khz = cfg.lora_chip_settings.bandwidth_khz;
    converted.spreading_factor = cfg.lora_chip_settings.spreading_factor;
  }
  if (converted.output_power_dbm > 20.0f)
  { converted.output_power_dbm = 20.0f; }

  doRestartLoRaChip(lora, cfg);

  int8_t currentLimit_ma = 100, gain = 0;
  bool is_reinitted = false;
  uint16_t result = ERR_NONE + 1;

  MODULE_REINIT_FOR_TX(SX1261, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);
  MODULE_REINIT_FOR_TX(SX1262, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);
  MODULE_REINIT_FOR_TX(SX1268, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);
  MODULE_REINIT_FOR_TX(SX1272, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);
  MODULE_REINIT_FOR_TX(SX1273, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);
  MODULE_REINIT_FOR_TX(SX1276, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);
  MODULE_REINIT_FOR_TX(SX1277, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);
  MODULE_REINIT_FOR_TX(SX1278, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);
  MODULE_REINIT_FOR_TX(SX1279, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);
  MODULE_REINIT_FOR_TX(RFM95, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);
  MODULE_REINIT_FOR_TX(RFM96, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);
  MODULE_REINIT_FOR_TX(RFM97, lora, is_reinitted, result, cfg, converted, currentLimit_ma, gain);

  if (result == ERR_NONE)
  {
    while (now < when)
    {
      delay(10);
      now = std::time(nullptr);
    }
    result = lora->transmit(converted.payload, converted.payload_size);
  }

  if (result == ERR_NONE)
  { ++loraPacketStats.downlink_tx_packets; }
  else
  { logMessage("Transmission error: %d\n", result); }

  restartLoRaChip(lora, cfg);

  return (result == ERR_NONE ? LoRaRecvStat::NODATA : LoRaRecvStat::DATARECVFAIL);
}  //}}}
