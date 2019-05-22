#ifndef LINUX
	#define LINUX
#endif

#include <cstdio>
#include <ctime>

#include "smtUdpPacketForwarder/ConfigFileParser.h"
#include "smtUdpPacketForwarder/UdpUtils.h"

#include "LoRaLib/LoRaLib.h"

#define DIO0 9 // WIRINGPI
#define NSS 6
#define DIO1 10


static LoRaPacketTrafficStats_t loraPacketStats;

SX1278 lora = new LoRa(NSS, DIO0, DIO1);


bool receiveData(LoRaDataPkt_t &pkt, String &msg) {
  int state = lora.receive(msg);

  // you can also receive data as byte array
  /*
    size_t len = 8;
    byte byteArr[len];
    int state = lora.receive(byteArr, len);
  */

  if (state == ERR_NONE) {
    ++loraPacketStats.recv_packets;
    ++loraPacketStats.recv_packets_crc_good;

    // packet was successfully received
    printf("success!\n");

    // print data of the packet
    printf("Data:\t\t\t");
    printf("%s\n", msg.c_str());

    // print RSSI (Received Signal Strength Indicator)
    // of the last received packet
    printf("RSSI:\t\t\t%d dBm\n", lora.getRSSI());

    // print SNR (Signal-to-Noise Ratio)
    // of the last received packet
    printf("SNR:\t\t\t%f dB\n", lora.getSNR());

    // print frequency error
    // of the last received packet
    printf("Frequency error:\t%f Hz\n", lora.getFrequencyError());

    pkt.RSSI = lora.getRSSI();
    pkt.SNR = lora.getSNR();
    pkt.msg = (const uint8_t*) msg.c_str();
    pkt.msg_sz = msg.length();
    return true;

  } else if (state == ERR_RX_TIMEOUT) {
    // timeout occurred while waiting for a packet
    //printf("timeout!\n");

  } else if (state == ERR_CRC_MISMATCH) {
    ++loraPacketStats.recv_packets;
 
    // packet was received, but is malformed
    printf("CRC error!\n");
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

  int state = lora.begin();
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

  while (true) {
    if ((accum % sendStatPktIntervalMs) == 0) {
      printf("Sending stat update to server(s)...\n");
      PublishStatProtocolPacket(netCfg, cfg, loraPacketStats);
    }

    String str;    
    if (receiveData(loraDataPacket, str)) {
      PublishLoRaProtocolPacket(netCfg, cfg, loraDataPacket);
    } else {
      delay(delayIntervalMs);
    }

    accum += delayIntervalMs;
  }
}
