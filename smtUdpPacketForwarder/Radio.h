#ifndef LORA_PF_RADIO
#define LORA_PF_RADIO

#include <cstdio>
#include <ctime>
#include <cstdint>
#include <RadioLib.h>
#include <PhysicalLayer/PhysicalLayer.h>
#include "ConfigFileParser.h"


enum class LoRaRecvStat { NODATA, DATARECV, DATARECVFAIL };

PhysicalLayer* instantiateLoRaChip(LoRaChipSettings_t& lora_chip_settings, SPIClass &spiClass, SPISettings &spiSettings);

uint16_t restartLoRaChip(PhysicalLayer *lora, PlatformInfo_t &cfg);

void hexPrint(uint8_t data[], int length, FILE *dest);

LoRaRecvStat recvLoRaUplinkData(LoRaChipSettings_t& lora_chip_settings, PhysicalLayer *lora, bool receiveOnAllChannels, LoRaDataPkt_t &pkt, uint8_t msg[], LoRaPacketTrafficStats_t &loraPacketStats);

#endif
