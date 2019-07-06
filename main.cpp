#ifndef LINUX
        #define LINUX
#endif

#include <cstdio>
#include <ctime>
#include <csignal>

#include "smtUdpPacketForwarder/ConfigFileParser.h"
#include "smtUdpPacketForwarder/UdpUtils.h"

#include <LoRaLib.h>


static volatile sig_atomic_t keepRunning = 1;

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

  time_t timestamp{std::time(nullptr)};

  if (state == ERR_NONE) { // packet was successfully received
    ++loraPacketStats.recv_packets;
    ++loraPacketStats.recv_packets_crc_good;

    // print data of the packet

    printf("\nReceived packet (%.24s):\n",
        std::asctime(std::localtime(&timestamp)));
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

  lora = new SX1278(new LoRa(
      cfg.lora_chip_settings.pin_nss_cs,
      cfg.lora_chip_settings.pin_dio0,
      cfg.lora_chip_settings.pin_dio1
    )
  );

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
  String str;

  while (keepRunning) {
    if (keepRunning && (accumPktStats % sendStatPktIntervalMs) == 0) {
      printf("Sending stat update to server(s)... ");
      PublishStatProtocolPacket(netCfg, cfg, loraPacketStats);
      ++loraPacketStats.forw_packets_crc_good;
      ++loraPacketStats.forw_packets;
      printf("done\n");
    }

    if (keepRunning && receiveData(loraDataPacket, str)) {
      PublishLoRaProtocolPacket(netCfg, cfg, loraDataPacket);
      str.clear();
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
