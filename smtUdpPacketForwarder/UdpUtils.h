#ifndef TTN_UDP_UTLS_H
#define TTN_UDP_UTLS_H

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <functional>
#include <mutex>
#include <memory>

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netdb.h>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "base64/base64.h"

#include "gpsTimestampUtils/GpsTimestampUtils.h"

#include "config.h"


#define STATUS_MSG_SIZE 1024 /* status report as a JSON object */
#define PROTOCOL_VERSION  2
#define PKT_PUSH_DATA 0
#define PKT_PUSH_ACK  1

#define PKT_PULL_DATA 2
#define PKT_PULL_RESP 3
#define PKT_PULL_ACK  4
#define PKT_TX_ACK 5

#define TX_BUFF_UP_SIZE 2048     /* buffer to compose the upstream packet */
#define TX_BUFF_DOWN_REQ_SIZE 12 /* buffer to compose downstream request packet */
#define RX_BUFF_DOWN_SIZE 2048

#define BASE64_MAX_LENGTH 341


typedef struct PackagedDataToSend
{
  uint32_t curr_attempt;
  uint32_t data_len;
  std::unique_ptr<uint8_t> data;
  Server_t destination;
  bool logged;
  std::time_t schedule;

  PackagedDataToSend(uint32_t curr_attempt, uint32_t data_len, uint8_t *data_content, Server_t& destination)
  {
    this->logged = false;
    this->schedule = 0;
    this->curr_attempt = curr_attempt;
    this->data_len = data_len;
    this->data = std::unique_ptr<uint8_t>(data_content);
    this->destination = destination; 
  }

  PackagedDataToSend(PackagedDataToSend &&origin)
  {
    logged = origin.logged;
    schedule = origin.schedule;
    curr_attempt = origin.curr_attempt;
    data_len = origin.data_len;
    data = std::move(origin.data);
    destination = origin.destination;
  }

} PackagedDataToSend_t;

enum Direction { UP_TX, DOWN_TX, DOWN_RX };

void Die(const char *s);
bool SolveHostname(const char* p_hostname, uint16_t port, struct sockaddr_in* p_sin);
bool SendUdp(Server_t &server, char *msg, int length, Direction direction,
             std::function<bool(char*, int, char*, int)> &validator);
bool RecvUdp(Server_t &server, char *msg, int size,
             std::function<bool(char*, int, char*, int*)> &validator);
NetworkConf_t PrepareNetworking(const char* networkInterfaceName, suseconds_t dataRecvTimeout, char gatewayId[25]);

void EnqueuePacket(uint8_t *data, uint32_t data_length, Server_t& dest, Direction direction);
bool RequeuePacket(PackagedDataToSend_t &&packet, uint32_t maxAttempts, Direction direction);
PackagedDataToSend_t DequeuePacket(Direction direction);


void PublishStatProtocolPacket(PlatformInfo_t &cfg, LoRaPacketTrafficStats_t &pktStats);
void PublishLoRaUplinkProtocolPacket(PlatformInfo_t &cfg, LoRaDataPkt_t &loraPacket);
void PublishLoRaDownlinkProtocolPacket(PlatformInfo_t &cfg);

#endif
