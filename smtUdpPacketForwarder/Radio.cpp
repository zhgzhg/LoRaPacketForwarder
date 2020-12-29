#include "Radio.h"

#include <functional>
#include <map>
#include <string>
#include <typeinfo>


#define MODULE_RESET(chip_class, origin, is_reset) if (!is_reset) { \
	chip_class* module = dynamic_cast<chip_class*>(origin); \
	if (module != nullptr) { \
	  is_reset = true; \
	  module->reset(); \
	  delay(10); \
	} \
}

#define MODULE_REINIT(chip_class, origin, is_reinitted, result, pi_cfg, power, current_lim_ma, gain) { \
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
	} \
}

uint16_t restartLoRaChip(PhysicalLayer *lora, PlatformInfo_t &cfg) { // {{{
  if (cfg.lora_chip_settings.pin_rest > -1) {
    bool is_reset = false;
    MODULE_RESET(SX1272, lora, is_reset); 
    MODULE_RESET(SX1273, lora, is_reset); 
    MODULE_RESET(SX1276, lora, is_reset); 
    MODULE_RESET(SX1277, lora, is_reset); 
    MODULE_RESET(SX1278, lora, is_reset); 
    MODULE_RESET(SX1279, lora, is_reset); 
    MODULE_RESET(RFM95, lora, is_reset); 
    MODULE_RESET(RFM96, lora, is_reset); 
    MODULE_RESET(RFM97, lora, is_reset); 
  }

  int8_t power = 17, currentLimit_ma = 100, gain = 0;
  bool is_reinitted = false;
  uint16_t result = ERR_NONE + 1;

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

PhysicalLayer* instantiateLoRaChip(LoRaChipSettings_t& lora_chip_settings, SPIClass &spiClass, SPISettings &spiSettings) { // {{{
  static const std::map<std::string, std::function<PhysicalLayer*(Module*)> > LORA_CHIPS {
    { "SX1272", [](Module* lora_module_settings) { return new SX1272(lora_module_settings); } },
    { "SX1273", [](Module* lora_module_settings) { return new SX1273(lora_module_settings); } },
    { "SX1276", [](Module* lora_module_settings) { return new SX1276(lora_module_settings); } },
    { "SX1277", [](Module* lora_module_settings) { return new SX1277(lora_module_settings); } },
    { "SX1278", [](Module* lora_module_settings) { return new SX1278(lora_module_settings); } },
    { "SX1279", [](Module* lora_module_settings) { return new SX1279(lora_module_settings); } },
    { "RFM95", [](Module* lora_module_settings) { return new RFM95(lora_module_settings); } },
    { "RFM96", [](Module* lora_module_settings) { return new RFM96(lora_module_settings); } },
    { "RFM97", [](Module* lora_module_settings) { return new RFM97(lora_module_settings); } },
    { "RFM98", [](Module* lora_module_settings) { return new RFM96(lora_module_settings); } } // LIKE RFM96
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

#define PKT_ENRICHER(origin_class) ([](PhysicalLayer *physical_layer, LoRaDataPkt_t &packet, float &freqErr) -> void { \
	origin_class *inst = static_cast<origin_class*>(physical_layer); \
	if (inst != nullptr) { \
	  packet.RSSI = inst->getRSSI(); \
	  packet.SNR = inst->getSNR(); \
	  freqErr = inst->getFrequencyError(); \
	} else { \
	  packet.RSSI = 1.0f; \
	  packet.SNR = 1.0f; \
          freqErr = 0.0f; \
	} \
})

#define PKT_ENRICHER2(origin_class, instance_ptr, inst_type_info, lora_packet, freq_err) \
if (inst_type_info == typeid(origin_class)) { \
	origin_class *inst = static_cast<origin_class*>(instance_ptr); \
	lora_packet.RSSI = inst->getRSSI(); \
	lora_packet.SNR = inst->getSNR(); \
	freq_err = inst->getFrequencyError(); \
	return; \
}


void enrichWithRadioStats(PhysicalLayer *lora, LoRaDataPkt_t &pkt, float &freqErr) { // {{{
  /*static const std::map<std::string, std::function<void(PhysicalLayer*, LoRaDataPkt_t&, float&)> > PKT_ENRICHERS {
    { "SX1272", PKT_ENRICHER(SX127x) },
    { "SX1273", PKT_ENRICHER(SX127x) },
    { "SX1276", PKT_ENRICHER(SX127x) },
    { "SX1277", PKT_ENRICHER(SX127x) },
    { "SX1278", PKT_ENRICHER(SX127x) },
    { "SX1279", PKT_ENRICHER(SX127x) },
    { "RFM95", PKT_ENRICHER(SX127x) },
    { "RFM96", PKT_ENRICHER(SX127x) },
    { "RFM97", PKT_ENRICHER(SX127x) },
    { "RFM98", PKT_ENRICHER(SX127x) }
  };

  PKT_ENRICHERS.at(lora_chip_settings.ic_model)(lora, pkt, freqErr);*/
  const std::type_info &loraInstTypeInfo = typeid(*lora);

  PKT_ENRICHER2(SX1272, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER2(SX1273, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER2(SX1276, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER2(SX1277, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER2(SX1278, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER2(SX1279, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER2(RFM95, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER2(RFM96, lora, loraInstTypeInfo, pkt, freqErr);
  PKT_ENRICHER2(RFM97, lora, loraInstTypeInfo, pkt, freqErr);

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

LoRaRecvStat recvLoRaUplinkData(LoRaChipSettings_t& lora_chip_settings, PhysicalLayer *lora,
		bool receiveOnAllChannels, LoRaDataPkt_t &pkt, uint8_t msg[],
		LoRaPacketTrafficStats_t &loraPacketStats) { // {{{

  int state = ERR_RX_TIMEOUT;
  bool insistDataReceiveFailure = false;

  if (!receiveOnAllChannels) {
    state = lora->receive(msg, SX127X_MAX_PACKET_LENGTH);
  } else {
    const std::type_info &loraTypeInfo = typeid(*lora);
    bool is_matched = false;
    ITER_ALL_SF(SX1272, lora, loraTypeInfo, is_matched, state, insistDataReceiveFailure, SpreadingFactor_t::SF7, SpreadingFactor_t::SF_MAX);
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

    printf("\n(%s) Received packet:\n", asciiTime);
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
