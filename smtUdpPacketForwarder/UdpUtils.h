#ifndef TTN_UDP_UTLS_H
#define TTN_UDP_UTLS_H

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

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

#define TX_BUFF_UP_SIZE 2048 /* buffer to compose the upstream packet */

#define BASE64_MAX_LENGTH 341


void Die(const char *s);
void SolveHostname(const char* p_hostname, uint16_t port, struct sockaddr_in* p_sin);
void SendUdp(NetworkConf_t &networkConf, Server_t &server, char *msg, int length);
NetworkConf_t PrepareNetworking(const char* networkInterfaceName, char gatewayId[25]);
void PublishStatProtocolPacket(NetworkConf_t &netCfg, PlatformInfo_t &cfg, LoRaPacketTrafficStats_t &pktStats);
void PublishLoRaProtocolPacket(NetworkConf_t &netCfg, PlatformInfo_t &cfg, LoRaDataPkt_t &loraPacket);

#endif
