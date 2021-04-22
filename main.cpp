#ifndef LINUX
        #define LINUX
#endif

#include <cstdio>
#include <ctime>
#include <csignal>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <typeinfo>

#include <unistd.h>
#include <linux/limits.h>

#include <RadioLib.h>

#include "smtUdpPacketForwarder/ConfigFileParser.h"
#include "smtUdpPacketForwarder/UdpUtils.h"
#include "smtUdpPacketForwarder/Radio.h"
#include "smtUdpPacketForwarder/TimeUtils.h"

extern char **environ;

static volatile sig_atomic_t keepRunning = 1;
static volatile unsigned long hearthbeat = 0;

void appIntubator(char* const argv[]) { // {{{
  unsigned long lastHearthbeat = hearthbeat;
  std::time_t lastObtained = std::time(nullptr);

  while (keepRunning) {
     if (lastHearthbeat != hearthbeat) {
      lastHearthbeat = hearthbeat;
      lastObtained = std::time(nullptr);
    } else if (std::time(nullptr) - lastObtained > 180) { // 3 minutes
      time_t currTime{std::time(nullptr)};
      char asciiTime[25];
      printf("(%s) Main thread block detected! Restarting the application...\n",
          ts_asciitime(currTime, asciiTime, sizeof(asciiTime)));
      fflush(stdout);

      char exePath[PATH_MAX];
      ssize_t len = ::readlink("/proc/self/exe", exePath, sizeof(exePath));
      if (len == -1 || len == sizeof(exePath)) {
        len = strlen(argv[0]);
        if (len > 0 && len != sizeof(exePath)) {
          strcpy(exePath, argv[0]);
        } else {
          len = 0;
        }
      }
      exePath[len] = '\0';

      execvpe(exePath, argv, environ);
      keepRunning = 0;
    }

    if (keepRunning) std::this_thread::sleep_for(std::chrono::seconds(1));
  }
} // }}}

void networkPacketExhangeWorker(LoRaPacketTrafficStats_t *loraPacketStats,
                                std::vector<Server_t> *servers) { // {{{

  static std::function<bool(char*, int, char*, int)> isValidUplinkAck =
    [](char* origMsg, int origMsgSz, char* respMsg, int respMsgSz) {
        if (origMsg != nullptr && origMsgSz > 4 && respMsg != nullptr && respMsgSz >= 4) {
          return origMsg[0] == respMsg[0] && origMsg[1] == respMsg[1] &&
                    origMsg[2] == respMsg[2] && (respMsg[3] == PKT_PUSH_ACK || respMsg[3] == PKT_PULL_ACK);
        }
        return false;
  };
  static std::function<bool(char*, int, char*, int*)> isValidDownlinkPkt =
    [](char *origMsg, int origMsgSz, char* respErrorJsonMsg, int *maxRespErrorJsonMsgSz) {
        if (origMsg != nullptr && origMsgSz > 15 && origMsg[0] == PROTOCOL_VERSION &&
            origMsg[3] == PKT_PULL_RESP)
        {
          maxRespErrorJsonMsgSz = 0;
          return true;
        }
        else
        {
          strncpy(respErrorJsonMsg, "Invalid packet or payload", *maxRespErrorJsonMsgSz);
          *maxRespErrorJsonMsgSz = strlen(respErrorJsonMsg);
        }

        return false;
  };

  struct TxPacket {
    PackagedDataToSend_t packet;
    Direction direction;

    TxPacket(PackagedDataToSend_t&& pkt, Direction directn) : packet(std::move(pkt)), direction(directn)
    { }
  };

  char downlinkMsg[RX_BUFF_DOWN_SIZE];
  bool iterateImmediately;
  do
  {
    iterateImmediately = false;
    for (auto it = servers->begin(); it != servers->end(); ++it)
    {
      bool received = RecvUdp(*it, downlinkMsg, sizeof(downlinkMsg), isValidDownlinkPkt);
      if (received)
      {
        ++(loraPacketStats->downlink_recv_packets);
        iterateImmediately = true;
      }
    }

    TxPacket txPackets[] = { TxPacket{DequeuePacket(UP_TX), UP_TX}, TxPacket{DequeuePacket(DOWN_TX), DOWN_TX} };

    for (size_t i = 0; i < sizeof(txPackets) / sizeof(TxPacket); ++i)
    {
       PackagedDataToSend_t& packet = txPackets[i].packet;
       if (packet.data_len > 0)
       {
          Direction direction = txPackets[i].direction;

          iterateImmediately = true;
          bool result = SendUdp(packet.destination, reinterpret_cast<char*>(packet.data.get()),
                            packet.data_len, direction, isValidUplinkAck);

          if (!result)
          {
            time_t currTime{std::time(nullptr)};
            char asciiTime[25];
            ts_asciitime(currTime, asciiTime, sizeof(asciiTime));
            printf("(%s) No %s ACK received from %s\n", asciiTime, (direction == UP_TX ? "uplink" : "downlink fetch request"),
              packet.destination.address.c_str());
            if (RequeuePacket(std::move(packet), 4, direction))
            { printf("(%s) Requeued the %s packet.\n", asciiTime, (direction == UP_TX ? "uplink" : "downlink fetch request")); }
            fflush(stdout);
          }
          else
          { ++(loraPacketStats->acked_forw_packets); }
       }
    }

    if (!iterateImmediately && keepRunning) std::this_thread::sleep_for(std::chrono::milliseconds(150));
  } while (keepRunning);

} // }}}

int main(int argc, char **argv) {

  time_t currTime{std::time(nullptr)};
  char asciiTime[25];

  ts_asciitime(currTime, asciiTime, sizeof(asciiTime));
  printf("(%s) Started %s...\n", asciiTime, argv[0]);

  char networkIfaceName[64] = "eth0";
  char gatewayId[25];
  memset(gatewayId, 0, sizeof(gatewayId));

  if (argc > 1)
  { strcpy(networkIfaceName, (const char*) argv[1]); }

  PlatformInfo_t cfg = LoadConfiguration("./config.json");

  for (auto &serv : cfg.servers) {
    serv.uplink_network_cfg =
       PrepareNetworking(networkIfaceName, serv.receive_timeout_ms * 1000, gatewayId);
    serv.downlink_network_cfg =
       PrepareNetworking(networkIfaceName, serv.receive_timeout_ms * 1000, gatewayId);
  }

  SetGatewayIdentifier(cfg, gatewayId);
  PrintConfiguration(cfg);

  SPISettings spiSettings{cfg.lora_chip_settings.spi_speed_hz, MSBFIRST,
    SPI_MODE0, cfg.lora_chip_settings.spi_channel};
  SPI.beginTransaction(spiSettings);

  PhysicalLayer* lora = instantiateLoRaChip(cfg.lora_chip_settings, SPI, spiSettings);

  uint16_t state = ERR_NONE + 1;

  for (uint8_t c = 0; state != ERR_NONE && c < 200; ++c) {
    state = restartLoRaChip(lora, cfg);

    if (state == ERR_NONE) printf("LoRa chip setup succeeded! Waiting for data...\n\n");
    else printf("LoRa chip setup failed, code %d\n", state);
  }

  if (state != ERR_NONE) {
    currTime = std::time(nullptr);
    ts_asciitime(currTime, asciiTime, sizeof(asciiTime));
    printf("Giving up due to failing LoRa chip setup!\n(%.24s) Exiting!\n", asciiTime);
    SPI.endTransaction();
    return 1;
  }

  fflush(stdout);


  auto signalHandler = [](int sigNum) { keepRunning = 0; };

  signal(SIGHUP, signalHandler);  // Process' terminal is closed, the
                                  // user has logger out, etc.
  signal(SIGINT, signalHandler);  // Interrupted process (Ctrl + c)
  signal(SIGQUIT, signalHandler); // Quit request (via console Ctrl + \)
  signal(SIGTERM, signalHandler); // Termination request
  signal(SIGXFSZ, signalHandler); // Creation of a file so large that
                                  // it's not allowed anymore to grow


  const uint16_t delayIntervalMs = 20;
  const uint32_t sendStatPktIntervalSeconds = 20;
  const uint32_t loraChipRestIntervalSeconds = 2700;

  time_t nextStatUpdateTime = std::time(nullptr) - 1;
  time_t nextChipRestTime = nextStatUpdateTime + 1 + loraChipRestIntervalSeconds;

  LoRaPacketTrafficStats_t loraPacketStats={};
  LoRaDataPkt_t loraDataPacket;
  uint8_t msg[SX127X_MAX_PACKET_LENGTH];

  std::thread packetExchanger{networkPacketExhangeWorker, &loraPacketStats, &cfg.servers};
  #ifndef NOINTUBATION
    std::thread intubator{appIntubator, argv};
    intubator.detach();
  #endif

  while (keepRunning) {
    ++hearthbeat;
    currTime = std::time(nullptr);

    if (keepRunning && currTime >= nextStatUpdateTime) {
      nextStatUpdateTime = currTime + sendStatPktIntervalSeconds;

      PublishStatProtocolPacket(cfg, loraPacketStats);
      ++loraPacketStats.forw_packets_crc_good;
      ++loraPacketStats.forw_packets;

      PublishLoRaDownlinkProtocolPacket(cfg);
      ++loraPacketStats.forw_packets_crc_good;
      ++loraPacketStats.forw_packets;
    }

    if (!keepRunning) break;

    LoRaRecvStat lastRecvResult = recvLoRaUplinkData(
		                    lora, cfg, loraDataPacket, msg, loraPacketStats);

    if (lastRecvResult == LoRaRecvStat::DATARECV) {
      PublishLoRaUplinkProtocolPacket(cfg, loraDataPacket);
    } else if (keepRunning && lastRecvResult == LoRaRecvStat::NODATA) {

      if (cfg.lora_chip_settings.pin_rest > -1 && currTime >= nextChipRestTime) {
        nextChipRestTime = currTime + loraChipRestIntervalSeconds;

        do {
          state = restartLoRaChip(lora, cfg);
          ts_asciitime(currTime, asciiTime, sizeof(asciiTime));
          printf("(%s) Regular LoRa chip reset done - code %d, %s success\n",
               asciiTime, state, (state == ERR_NONE ? "with" : "WITHOUT"));
          fflush(stdout);
          delay(delayIntervalMs);
        } while (state != ERR_NONE);

      }

      PackagedDataToSend_t downlinkPacket{DequeuePacket(DOWN_RX)};
      if (downlinkPacket.data_len > 0)
      {
        if (sendLoRaDownlinkData(lora, cfg, downlinkPacket, loraPacketStats) == LoRaRecvStat::NODATA)
        {
          ts_asciitime(currTime, asciiTime, sizeof(asciiTime));
          printf("(%s) Downlink packet has been trasmitted with success!\n", asciiTime);
          fflush(stdout);
        }
      }

      if (!cfg.lora_chip_settings.all_spreading_factors) {
        delay(delayIntervalMs);
      }
    }

  }

  currTime = std::time(nullptr);
  ts_asciitime(currTime, asciiTime, sizeof(asciiTime));

  printf("\n(%s) Shutting down...\n", asciiTime);
  fflush(stdout);
  SPI.endTransaction();
  packetExchanger.join();
}
