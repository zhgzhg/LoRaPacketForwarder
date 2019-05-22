#include "UdpUtils.h"

void Die(const char *s)
{
  perror(s);
  exit(1);
}

void SolveHostname(const char* p_hostname, uint16_t port, struct sockaddr_in* p_sin)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  char service[6] = { '\0' };
  snprintf(service, 6, "%hu", port);

  struct addrinfo* p_result = NULL;

  // Resolve the domain name into a list of addresses
  int error = getaddrinfo(p_hostname, service, &hints, &p_result);
  if (error != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
      exit(EXIT_FAILURE);
  }

  // Loop over all returned results
  for (struct addrinfo* p_rp = p_result; p_rp != NULL; p_rp = p_rp->ai_next) {
    struct sockaddr_in* p_saddr = (struct sockaddr_in*)p_rp->ai_addr;
    //printf("%s solved to %s\n", p_hostname, inet_ntoa(p_saddr->sin_addr));
    p_sin->sin_addr = p_saddr->sin_addr;
  }

  freeaddrinfo(p_result);
}

void SendUdp(NetworkConf_t &networkConf, Server_t &server, char *msg, int length)
{
  networkConf.si_other.sin_port = htons(server.port);
 
  SolveHostname(server.address.c_str(), server.port, &networkConf.si_other);
  
  if (sendto(networkConf.socket, (char *)msg, length, 0,
      (struct sockaddr *) &networkConf.si_other, sizeof(networkConf.si_other)) == -1) {
    Die("sendto()");
  }
}


NetworkConf_t PrepareNetworking(const char* networkInterfaceName, char gatewayId[25])
{
  NetworkConf_t result;

  if ((result.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
    Die("socket");
  }

  memset((char *) &result.si_other, 0, sizeof(result.si_other));
  result.si_other.sin_family = AF_INET;

  result.ifr.ifr_addr.sa_family = AF_INET;
  strncpy(result.ifr.ifr_name, networkInterfaceName, IFNAMSIZ - 1);

  ioctl(result.socket, SIOCGIFHWADDR, &result.ifr);

  sprintf(gatewayId, "%.2x:%.2x:%.2x:ff:ff:%.2x:%.2x:%.2x",
      (uint8_t)result.ifr.ifr_hwaddr.sa_data[0],
      (uint8_t)result.ifr.ifr_hwaddr.sa_data[1],
      (uint8_t)result.ifr.ifr_hwaddr.sa_data[2],
      (uint8_t)result.ifr.ifr_hwaddr.sa_data[3],
      (uint8_t)result.ifr.ifr_hwaddr.sa_data[4],
      (uint8_t)result.ifr.ifr_hwaddr.sa_data[5]
  );

  return result;  
}

void PublishStatProtocolPacket(NetworkConf_t &netCfg, PlatformInfo_t &cfg, LoRaPacketTrafficStats_t &pktStats)
{
  // see https://github.com/Lora-net/packet_forwarder/blob/master/PROTOCOL.TXT

  static char status_report[STATUS_MSG_SIZE]; /* status report as a JSON object */
  char stat_timestamp[24];

  int stat_index = 0;

  /* pre-fill the data buffer with fixed fields */
  status_report[0] = PROTOCOL_VERSION;
  status_report[3] = PKT_PUSH_DATA;

  status_report[4] = (unsigned char)netCfg.ifr.ifr_hwaddr.sa_data[0];
  status_report[5] = (unsigned char)netCfg.ifr.ifr_hwaddr.sa_data[1];
  status_report[6] = (unsigned char)netCfg.ifr.ifr_hwaddr.sa_data[2];
  status_report[7] = 0xFF;
  status_report[8] = 0xFF;
  status_report[9] = (unsigned char)netCfg.ifr.ifr_hwaddr.sa_data[3];
  status_report[10] = (unsigned char)netCfg.ifr.ifr_hwaddr.sa_data[4];
  status_report[11] = (unsigned char)netCfg.ifr.ifr_hwaddr.sa_data[5];

  /* start composing datagram with the header */
  uint8_t token_h = (uint8_t)rand(); /* random token */
  uint8_t token_l = (uint8_t)rand(); /* random token */
  status_report[1] = token_h;
  status_report[2] = token_l;
  stat_index = 12; /* 12-byte header */

  /* get timestamp for statistics */
  time_t t = time(NULL);
  strftime(stat_timestamp, sizeof stat_timestamp, "%F %T %Z", gmtime(&t));

  // Build JSON object.
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  writer.StartObject();
  writer.String("stat");
  writer.StartObject();
  writer.String("time");
  writer.String(stat_timestamp);
  writer.String("lati");
  writer.Double(cfg.latitude);
  writer.String("long");
  writer.Double(cfg.longtitude);
  writer.String("alti");
  writer.Int(cfg.altitude_meters);
  writer.String("rxnb");
  writer.Uint(pktStats.recv_packets);
  writer.String("rxok");
  writer.Uint(pktStats.recv_packets_crc_good);
  writer.String("rxfw");
  writer.Uint(pktStats.forw_packets);
  writer.String("ackr");
  writer.Double(0);
  writer.String("dwnb");
  writer.Uint(0);
  writer.String("txnb");
  writer.Uint(pktStats.forw_packets_crc_good); // not optimal

  // ==== not part ot the specification ====
  writer.String("pfrm");
  writer.String(cfg.platform_definition);
  writer.String("mail");
  writer.String(cfg.platform_email);
  writer.String("desc");
  writer.String(cfg.platform_description);
  // =======================================

  writer.EndObject();
  writer.EndObject();

  std::string json = sb.GetString();
  //printf("stat update: %s\n", json.c_str());

  memcpy(status_report + 12, json.c_str(), json.size());

  for (Server_t &serv : cfg.servers) {
    SendUdp(netCfg, serv, status_report, stat_index + json.size());
  }
}

void PublishLoRaProtocolPacket(NetworkConf_t &netCfg, PlatformInfo_t &cfg, LoRaDataPkt_t &loraPacket)
{
  // see https://github.com/Lora-net/packet_forwarder/blob/master/PROTOCOL.TXT

  char buff_up[TX_BUFF_UP_SIZE]; /* buffer to compose the upstream packet */
  int buff_index = 0;

  /* gateway <-> MAC protocol variables */
  //static uint32_t net_mac_h; /* Most Significant Nibble, network order */
  //static uint32_t net_mac_l; /* Least Significant Nibble, network order */

  /* pre-fill the data buffer with fixed fields */
  buff_up[0] = PROTOCOL_VERSION;
  buff_up[3] = PKT_PUSH_DATA;

  /* process some of the configuration variables */
  //net_mac_h = htonl((uint32_t)(0xFFFFFFFF & (lgwm>>32)));
  //net_mac_l = htonl((uint32_t)(0xFFFFFFFF &  lgwm  ));
  //*(uint32_t *)(buff_up + 4) = net_mac_h; 
  //*(uint32_t *)(buff_up + 8) = net_mac_l;

  buff_up[4] = (uint8_t)netCfg.ifr.ifr_hwaddr.sa_data[0];
  buff_up[5] = (uint8_t)netCfg.ifr.ifr_hwaddr.sa_data[1];
  buff_up[6] = (uint8_t)netCfg.ifr.ifr_hwaddr.sa_data[2]; 
  buff_up[7] = 0xFF;
  buff_up[8] = 0xFF;
  buff_up[9] = (uint8_t)netCfg.ifr.ifr_hwaddr.sa_data[3];
  buff_up[10] = (uint8_t)netCfg.ifr.ifr_hwaddr.sa_data[4];
  buff_up[11] = (uint8_t)netCfg.ifr.ifr_hwaddr.sa_data[5];

  /* start composing datagram with the header */
  uint8_t token_h = (uint8_t)rand(); /* random token */
  uint8_t token_l = (uint8_t)rand(); /* random token */
  buff_up[1] = token_h;
  buff_up[2] = token_l;
  buff_index = 12; /* 12-byte header */

  // TODO: tmst can jump is time is (re)set, not good.
  struct timeval now;
  gettimeofday(&now, NULL);

  //uint32_t tmst = (uint32_t)(now.tv_sec * 1000000 + now.tv_usec);

  // unix epoch in seconds ts to GPS timestamp in millis
  uint64_t tmst = std::round( (unix2gps(now.tv_sec) * 1000.0) + (now.tv_usec / 1000) );

  char compact_iso8610_time[28];
  strftime(compact_iso8610_time, sizeof compact_iso8610_time, "%FT%T", gmtime(&now.tv_sec));
  sprintf(compact_iso8610_time, "%s.%ldZ", compact_iso8610_time, now.tv_usec);

  // Build JSON object.
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  writer.StartObject();
  writer.String("rxpk");
  writer.StartArray();
  writer.StartObject();
  writer.String("time");
  writer.String(compact_iso8610_time);
  writer.String("tmst");
  writer.Uint64(tmst); 
  writer.String("freq");
  writer.Double(cfg.lora_chip_settings.carrier_frequency_mhz);
  writer.String("chan");
  writer.Uint(0);
  writer.String("rfch");
  writer.Uint(0);
  writer.String("stat");
  writer.Uint(1);
  writer.String("modu");
  writer.String("LORA");

  writer.String("datr");
  char datr[] = "SFxxBWxxxxxx";
  snprintf(datr, strlen(datr) + 1, "SF%hhuBW%g", cfg.lora_chip_settings.spreading_factor, cfg.lora_chip_settings.bandwidth_khz);
  writer.String(datr);

  writer.String("codr");
  char cr[4] = "4/x";
  snprintf(cr, sizeof(cr), "4/%d", cfg.lora_chip_settings.coding_rate);
  writer.String(cr);

  writer.String("rssi");
  writer.Int(((int)loraPacket.RSSI));
  writer.String("lsnr");
  writer.SetMaxDecimalPlaces(1);
  writer.Double(std::round(loraPacket.SNR * 10) / 10);
  writer.SetMaxDecimalPlaces(rapidjson::Writer::kDefaultMaxDecimalPlaces);
  writer.String("size");
  writer.Uint(loraPacket.msg_sz);
  writer.String("data");

  // Encode payload.
  char b64[BASE64_MAX_LENGTH];
  bin_to_b64(loraPacket.msg, loraPacket.msg_sz, b64, BASE64_MAX_LENGTH);

  writer.String(b64);
  writer.EndObject();
  writer.EndArray();
  writer.EndObject();

  std::string json = sb.GetString();
  //printf("%s\n", json.c_str());

  memcpy(buff_up + 12, json.c_str(), json.size());
  for (Server_t &serv : cfg.servers) {
    SendUdp(netCfg, serv, buff_up, buff_index + json.size());
  }
}
