#ifndef TTN_FWD_CFG_H
#define TTN_FWD_CFG_H

#include <cstdlib>
#include <stdint.h>
#include <inttypes.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netdb.h>

#include <vector>
#include <string>


typedef enum SpreadingFactor {
  SF_ALL = -1,
  SF_MIN = 6, 
  SF6 = SF_MIN,
  SF7 = 7,
  SF8 = 8,
  SF9 = 9,
  SF10 = 10,
  SF11 = 11,
  SF_MAX = 12,
  SF12 = SF_MAX
} SpreadingFactor_t;

typedef enum CodingRate {
  CR_MIN = 5,
  CR_4_5 = CR_MIN,
  CR_4_6 = 6,
  CR_4_7 = 7,
  CR_MAX = 8,
  CR_4_8 = CR_MAX
} CodingRate_t;

typedef struct LoRaChipSettings {
  std::string ic_model;

  uint8_t spi_channel;
  uint32_t spi_speed_hz;

  int pin_nss_cs;
  int pin_dio0;
  int pin_dio1;
  int pin_rest; // negative value means not used

  bool all_spreading_factors;
  SpreadingFactor_t spreading_factor;
  float carrier_frequency_mhz;
  float bandwidth_khz;
  CodingRate_t coding_rate;
  uint8_t sync_word;
  uint16_t preamble_length;

} LoRaChipSettings_t;

typedef struct NetworkConf {
  struct sockaddr_in si_other;
  struct ifreq ifr;
  int socket;
  struct timeval recv_timeout;
} NetworkConf_t;

typedef struct Server {
  std::string address;
  uint16_t port;
  uint32_t receive_timeout_ms;
  NetworkConf_t network_cfg;
} Server_t;

typedef struct PlatformInfo {
  LoRaChipSettings_t lora_chip_settings;  

  float latitude;
  float longtitude;
  int altitude_meters;
  
  char platform_definition[25];
  char platform_email[41];
  char platform_description[65];

  char __identifier[129];

  std::vector<Server_t> servers;
} PlatformInfo_t;

typedef struct LoRaPacketTrafficStats {
  uint32_t recv_packets;
  uint32_t recv_packets_crc_good;
  uint32_t forw_packets;
  uint32_t forw_packets_crc_good;
} LoRaPacketTrafficStats_t;

typedef struct LoRaDataPkt {
  const uint8_t *msg;
  uint32_t msg_sz;
  float SNR;
  float RSSI; 
} LoRaDataPkt_t;

#endif
