#ifndef LINUX
	#define LINUX
#endif

#include <cstdio>
#include <ctime>

#include "smtUdpPacketForwarder/ConfigFileParser.h"
#include "smtUdpPacketForwarder/UdpUtils.h"

#include <LoRaLib.h>

static LoRaPacketTrafficStats_t loraPacketStats;
static SX1278 *lora;

bool receiveData(LoRaDataPkt_t &pkt, String &msg) {
  int state = lora->receive(msg);

  // you can also receive data as byte array
  /*
    size_t len = 8;
    byte byteArr[len];
    int state = lora->receive(byteArr, len);
  */

  if (state == ERR_NONE) { // packet was successfully received
    ++loraPacketStats.recv_packets;
    ++loraPacketStats.recv_packets_crc_good;
    
    // print data of the packet
    
    printf("\nReceived packet:\n");
    printf(" Data:\t\t\t%s\n", msg.c_str());

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
    pkt.msg = (const uint8_t*) msg.c_str();
    pkt.msg_sz = msg.length();
    return true;

  } /*else if (state == ERR_RX_TIMEOUT) {
    // timeout occurred while waiting for a packet
    //printf("timeout!\n");
    return false;
  }*/ else if (state == ERR_CRC_MISMATCH) {
    // packet was received, but is malformed
    ++loraPacketStats.recv_packets;
    printf("Received packet CRC error - ignored!\n");
  }
  
  return false;
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

  lora = new SX1278(new LoRa(
      cfg.lora_chip_settings.pin_nss_cs,
      cfg.lora_chip_settings.pin_dio0,
      cfg.lora_chip_settings.pin_dio1
    )
  );

  int8_t power = 17, currentLimit = 100, gain = 0;
  
  uint16_t state = lora->begin(
    cfg.lora_chip_settings.carrier_frequency_mhz,
    cfg.lora_chip_settings.bandwidth_khz,
    cfg.lora_chip_settings.spreading_factor,
    cfg.lora_chip_settings.coding_rate,
    cfg.lora_chip_settings.sync_word,
    power,
    currentLimit,
    cfg.lora_chip_settings.preamble_length,
    gain
  );
  
  if (state == ERR_NONE) {
    printf("LoRa chip setup success!\n");
  } else {
    printf("LoRa chip setup failed, code %d\n", state);
    return 1;
  }

  const uint16_t delayIntervalMs = 20;
  const uint32_t sendStatPktIntervalMs = 80000;
  uint32_t accum = 0; 

  LoRaDataPkt_t loraDataPacket;
  String str;

  while (true) {
    if ((accum % sendStatPktIntervalMs) == 0) {
      printf("Sending stat update to server(s)... ");
      PublishStatProtocolPacket(netCfg, cfg, loraPacketStats);
      ++loraPacketStats.forw_packets_crc_good;
      ++loraPacketStats.forw_packets;
      printf("done\n");
    }

    if (receiveData(loraDataPacket, str)) {
      PublishLoRaProtocolPacket(netCfg, cfg, loraDataPacket);
      str.clear();
    } else {
      delay(delayIntervalMs);
    }

    accum += delayIntervalMs;
  }
  
}
