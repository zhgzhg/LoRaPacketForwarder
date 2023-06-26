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

#include <sys/resource.h>
#include <sched.h>

#include <RadioLib.h>

#include "smtUdpPacketForwarder/version.h"
#include "smtUdpPacketForwarder/ConfigFileParser.h"
#include "smtUdpPacketForwarder/UdpUtils.h"
#include "smtUdpPacketForwarder/Radio.h"
#include "smtUdpPacketForwarder/TimeUtils.h"

extern char **environ;
extern char *optarg;
extern int optind, opterr, optopt;

static volatile sig_atomic_t keepRunning = 1;
static volatile unsigned long hearthbeat = 0;

void appIntubator(char* const argv[]) { // {{{
  unsigned long lastHearthbeat = hearthbeat;
  std::time_t lastObtained = std::time(nullptr);

  while (keepRunning) {
     if (lastHearthbeat != hearthbeat) {
      lastHearthbeat = hearthbeat;
      lastObtained = std::time(nullptr);
    } else if (difftime(std::time(nullptr), lastObtained) > 45.0) {
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
          //strncpy(respErrorJsonMsg, "NONE", *maxRespErrorJsonMsgSz); // no need
          //*maxRespErrorJsonMsgSz = 4;
          *maxRespErrorJsonMsgSz = 0;
          return true;
        }
        else
        {
          // Invalid packet or payload. Send vague, but compliant response
          strncpy(respErrorJsonMsg, "COLLISION_BEACON", *maxRespErrorJsonMsgSz);
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

    if (!iterateImmediately && keepRunning) std::this_thread::sleep_for(std::chrono::milliseconds(50));
  } while (keepRunning);

} // }}}

PlatformInfo_t loadConfig(int argc, char **argv, const char **confFile, bool *useIntubator,
                            char *networkIfaceName, size_t networkIfaceNameSz) { // {{{

  int opt;
  while ((opt = getopt(argc, argv, "c:dvh")) != -1) {
    switch (opt) {
      case 'c':
        *confFile = optarg;
        break;
      case 'd':
        *useIntubator = false;
        break;
      case 'v':
        fprintf(stderr, "LoRa Packet Forwarder Version: %s\n", TO_STR(GIT_VER));
        exit(0);
      case 'h':
      default: // i.e. ?
        fprintf(stderr, "Usage:\n%s [-h] [-v] [-c config.json] [-d] [network_interface_name]\n"
            "  -h displays this help\n"
            "  -v displays the version of the tool\n\n"

            "  -c specifies the JSON configuration file (by default config.json)\n"
            "  -d disables the automatic restart of the program if a stuck has been detected\n"
            "  network_interface_name (by default eth0) is used to compute the EUI\n", argv[0]);
        exit(opt == 'h' ? 0 : EXIT_FAILURE);
    }
  }

  if (optind < argc) {
    strncpy(networkIfaceName, argv[optind], networkIfaceNameSz);
  }

  return LoadConfiguration(*confFile);
} // }}}

int main(int argc, char **argv) {

  time_t currTime{std::time(nullptr)};
  char asciiTime[25];

  ts_asciitime(currTime, asciiTime, sizeof(asciiTime));
  printf("(%s) Started %s...\n", asciiTime, argv[0]);

  char networkIfaceName[64] = "eth0";
  char gatewayId[25];
  memset(gatewayId, 0, sizeof(gatewayId));

  const char *configFilePath = "config.json";
  bool useIntubator = true;

  PlatformInfo_t cfg = loadConfig(argc, argv, &configFilePath, &useIntubator,
                            networkIfaceName, sizeof(networkIfaceName) - 1);

  for (auto &serv : cfg.servers) {
    serv.uplink_network_cfg =
       PrepareNetworking(networkIfaceName, serv.receive_timeout_ms * 1000, gatewayId);
    serv.downlink_network_cfg =
       PrepareNetworking(networkIfaceName, serv.receive_timeout_ms * 1000, gatewayId);
  }

  SetGatewayIdentifier(cfg, gatewayId);
  PrintConfiguration(cfg);

  SPISettings spiSettings{cfg.lora_chip_settings.spi_speed_hz, MSBFIRST,
    SPI_MODE0, cfg.lora_chip_settings.spi_channel, cfg.lora_chip_settings.spi_port};
  SPI.beginTransaction(spiSettings);

  PhysicalLayer* lora = instantiateLoRaChip(cfg.lora_chip_settings, SPI, spiSettings);

  uint16_t state = RADIOLIB_ERR_NONE + 1;

  for (uint8_t c = 0; state != RADIOLIB_ERR_NONE && c < 200; ++c) {
    state = restartLoRaChip(lora, cfg);

    if (state == RADIOLIB_ERR_NONE) printf("LoRa chip setup succeeded! Waiting for data...\n\n");
    else printf("LoRa chip setup failed, code (%d) %s\n", state, decodeRadioLibErrorCode(state));
  }

  if (state != RADIOLIB_ERR_NONE) {
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
  uint8_t msg[RADIOLIB_SX127X_MAX_PACKET_LENGTH];

  // use realtime priority to impove the app's RF transmission timings
  setpriority(PRIO_PROCESS, 0, -20);
  sched_param schedPrio;
  schedPrio.sched_priority = sched_get_priority_max(SCHED_RR) - 10;
  sched_setscheduler(0, SCHED_RR, (const sched_param*) &schedPrio);

  std::thread packetExchanger{networkPacketExhangeWorker, &loraPacketStats, &cfg.servers};
  if (useIntubator) {
    std::thread intubator{appIntubator, argv};
    intubator.detach();
  }

  time_t lastRFInteractionTime{0};
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
      lastRFInteractionTime = std::time(nullptr);
    } else if (keepRunning && lastRecvResult == LoRaRecvStat::NODATA) {
      currTime = std::time(nullptr);

      PackagedDataToSend_t downlinkPacket{DequeuePacket(DOWN_RX)};
      if (downlinkPacket.data_len > 0) {
        if (sendLoRaDownlinkData(lora, cfg, downlinkPacket, loraPacketStats) == LoRaRecvStat::NODATA) {
          lastRFInteractionTime = currTime = std::time(nullptr);
          ts_asciitime(currTime, asciiTime, sizeof(asciiTime));
          printf("(%s) Downlink packet has been trasmitted with success!\n", asciiTime);
          // don't flush - there's a high chance for a subsequent uplink response
        }
      }

      if (cfg.lora_chip_settings.pin_rest > -1 && currTime >= nextChipRestTime
             && diff_timestamps(lastRFInteractionTime, currTime) > 6) {

        currTime = std::time(nullptr);
        nextChipRestTime = currTime + loraChipRestIntervalSeconds;

        do {
          state = restartLoRaChip(lora, cfg);
          ts_asciitime(currTime, asciiTime, sizeof(asciiTime));
          printf("(%s) Regular LoRa chip reset done - code %d, %s success\n",
               asciiTime, state, (state == RADIOLIB_ERR_NONE ? "with" : "WITHOUT"));

          if (state != RADIOLIB_ERR_NONE)
          { printf("(%s) Error: %s\n", asciiTime, decodeRadioLibErrorCode(state)); }

          fflush(stdout);
          delay(delayIntervalMs);
        } while (state != RADIOLIB_ERR_NONE);
      }

      if (!cfg.lora_chip_settings.all_spreading_factors
            && diff_timestamps(lastRFInteractionTime, currTime) > 6) {
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
