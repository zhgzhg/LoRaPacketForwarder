#include "Radio.h"

#include <functional>
#include <map>
#include <string>
#include <typeinfo>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"

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
	}

uint16_t restartLoRaChip(PhysicalLayer *lora, PlatformInfo_t &cfg) { // {{{
  if (cfg.lora_chip_settings.pin_rest > -1) {
    bool is_reset = false;
    MODULE_RESET(SX1261, lora, is_reset); MODULE_RESET(SX1262, lora, is_reset);  MODULE_RESET(SX1268, lora, is_reset); 
    MODULE_RESET(SX1272, lora, is_reset); MODULE_RESET(SX1273, lora, is_reset);  MODULE_RESET(SX1276, lora, is_reset); 
    MODULE_RESET(SX1277, lora, is_reset); MODULE_RESET(SX1278, lora, is_reset);  MODULE_RESET(SX1279, lora, is_reset); 
    MODULE_RESET(RFM95, lora, is_reset); MODULE_RESET(RFM96, lora, is_reset); MODULE_RESET(RFM97, lora, is_reset); 
  }

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
	lora_packet.RSSI = inst->getRSSI(); \
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

#define ITER_ALL_SF(origin_class, lora, lora_type, is_matched, state, insist_data_recv_fail, sf_min, sf_max) \
if (!is_matched && lora_type == typeid(origin_class)) { \
	is_matched = true; \
	origin_class* inst = static_cast<origin_class*>(lora); \
	for (unsigned i = sf_min; i <= sf_max; ++i) {\
		inst->setSpreadingFactor(i); \
		state = inst->scanChannel(); \
		if (state == PREAMBLE_DETECTED) /*&& lora->getRSSI() > -124.0) */{ \
			state = inst->receive(msg, SX127X_MAX_PACKET_LENGTH); \
			insist_data_recv_fail = (state != ERR_NONE); \
			printf("Got preamble at SF%d, RSSI %f!\n", i, inst->getRSSI()); \
			break; \
		} \
	} \
}

LoRaRecvStat recvLoRaUplinkData(PhysicalLayer *lora,bool receiveOnAllChannels, LoRaDataPkt_t &pkt,
                                uint8_t msg[], LoRaPacketTrafficStats_t &loraPacketStats) { // {{{

  int state = ERR_RX_TIMEOUT;
  bool insistDataReceiveFailure = false;

  if (!receiveOnAllChannels) {
    state = lora->receive(msg, SX127X_MAX_PACKET_LENGTH);
  } else {
    const std::type_info &loraTypeInfo = typeid(*lora);
    bool is_matched = false;
    ITER_ALL_SF(SX1261, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
    ITER_ALL_SF(SX1262, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
    ITER_ALL_SF(SX1268, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
    ITER_ALL_SF(SX1273, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
    ITER_ALL_SF(SX1276, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
    ITER_ALL_SF(SX1277, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
    ITER_ALL_SF(SX1278, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
    ITER_ALL_SF(SX1279, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
    ITER_ALL_SF(RFM95, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
    ITER_ALL_SF(RFM96, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
    ITER_ALL_SF(RFM97, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
  }

  if (state == ERR_NONE) { // packet was successfully received
    time_t timestamp{std::time(nullptr)};
    char asciiTime[25];
    std::strftime(asciiTime, sizeof(asciiTime), "%c", std::localtime(&timestamp));

    int msg_length = lora->getPacketLength(false);
    float freqErr = 0.0f;

    enrichWithRadioStats(lora, pkt, freqErr);

    ++loraPacketStats.recv_packets;
    ++loraPacketStats.recv_packets_crc_good;

    printf("\n(%s) Received UPlink packet:\n", asciiTime);
    printf(" RSSI:\t\t\t%.1f dBm\n", pkt.RSSI);
    printf(" SNR:\t\t\t%f dB\n", pkt.SNR);
    printf(" Frequency error:\t%f Hz\n", freqErr);
    printf(" Data:\t\t\t%d bytes\n\n", msg_length);
    hexPrint(msg, msg_length, stdout);

    pkt.msg = static_cast<const uint8_t*> (msg);
    pkt.msg_sz = msg_length;

    return LoRaRecvStat::DATARECV;

  } else if (state == ERR_CRC_MISMATCH) {
    // packet was received, but is malformed
    time_t timestamp{std::time(nullptr)};
    char asciiTime[25];
    std::strftime(asciiTime, sizeof(asciiTime), "%c", std::localtime(&timestamp));

    ++loraPacketStats.recv_packets;
    printf("(%s) Received packet CRC error - ignored!\n", asciiTime);
    fflush(stdout);
    return LoRaRecvStat::DATARECVFAIL;
  }

  return (insistDataReceiveFailure ? LoRaRecvStat::DATARECVFAIL : LoRaRecvStat::NODATA);
} // }}}

struct DownlinkPacket
{
  bool send_immediately;

  std::time_t unix_epoch_timestamp;
  unsigned long long  gps_timestamp;

  unsigned long concentrator_rf_chain;
  float output_power_dbm;
  SpreadingFactor_t spreading_factor;
  float carrier_frequency_mhz;
  float bandwidth_khz;
  CodingRate_t coding_rate;
  unsigned char preamble_size;

  unsigned fsk_datarate_bps;
  unsigned long fsk_freq_deviation_hz;

  bool iq_polatization_inversion;

  unsigned long payload_size;
  std::string payload_base64_enc;

  bool disable_crc;
};

void downlinkTxJsonToPacket(PackagedDataToSend_t &pkt)
{
    rapidjson::Document doc;
    doc.Parse(reinterpret_cast<const char*>(pkt.data.get()));
    // TODO
}

LoRaRecvStat sendLoRaDownlinkData(PhysicalLayer *lora, PackagedDataToSend_t &pkt, LoRaPacketTrafficStats_t &loraPacketStats) // {{{
{
  time_t timestamp{std::time(nullptr)};
  char asciiTime[25];
  std::strftime(asciiTime, sizeof(asciiTime), "%c", std::localtime(&timestamp));

  printf("\n(%s) Received DOWNlink packet:\n", asciiTime);
  hexPrint(pkt.data.get(), pkt.data_len, stdout);

  // TODO  

  return LoRaRecvStat::DATARECVFAIL;
}  //}}}
