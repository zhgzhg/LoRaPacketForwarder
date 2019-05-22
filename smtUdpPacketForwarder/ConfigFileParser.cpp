#include "ConfigFileParser.h"

void PrintConfiguration(PlatformInfo_t &cfg)
{
  printf("\nPlatformInfo:\n");
  printf("------------\n\n");

  printf("ID (EUI-64): %s\n\n", cfg.__identifier);

  printf("(WiringPI) Pins:\n  nss_cs=%d\n  dio0=%d\n  dio1=%d\n\n", cfg.lora_chip_settings.pin_nss_cs,
    cfg.lora_chip_settings.pin_dio0, cfg.lora_chip_settings.pin_dio1);

  printf("LoRa SX127x Chip:\n  Freq=%f MHz\n  BW=%f KHz\n  SF=%d\n  CR=4/%d\n  SyncWord=0x%x\n  PreambleLength=%d\n\n",
    cfg.lora_chip_settings.carrier_frequency_mhz,cfg.lora_chip_settings.bandwidth_khz,
    cfg.lora_chip_settings.spreading_factor, cfg.lora_chip_settings.coding_rate,
    cfg.lora_chip_settings.sync_word, cfg.lora_chip_settings.preamble_length);

  printf("Meta Information:\n  Latitude=%f\n  Longtitude=%f\n  Altitude=%d meters\n",
    cfg.latitude, cfg.longtitude, cfg.altitude_meters);

  printf("  Name/Definition: %s\n  E-mail: %s\n  Description: %s\n\n", cfg.platform_definition,
    cfg.platform_email, cfg.platform_description);
}


PlatformInfo_t LoadConfiguration(std::string configurationFile, const char identifier[129]) 
{
  FILE* p_file = fopen(configurationFile.c_str(), "r");
  char buffer[65536];
  rapidjson::FileReadStream fs(p_file, buffer, sizeof(buffer));

  rapidjson::Document doc;
  doc.ParseStream(fs);

  PlatformInfo_t result;

  result.lora_chip_settings.pin_nss_cs = doc["pin_nss_cs"].GetInt();
  result.lora_chip_settings.pin_dio0 = doc["pin_dio0"].GetInt();
  result.lora_chip_settings.pin_dio1 = doc["pin_dio1"].GetInt();

  int sf = doc["spreading_factor"].GetInt();
  if (sf < SpreadingFactor_t::SF_MIN || sf > SpreadingFactor_t::SF_MAX) {
    sf = SpreadingFactor_t::SF7;
  }
  result.lora_chip_settings.spreading_factor = static_cast<SpreadingFactor_t>(sf);

  result.lora_chip_settings.carrier_frequency_mhz = (float) doc["carrier_frequency_mhz"].GetDouble();
  result.lora_chip_settings.bandwidth_khz = (float) doc["bandwidth_khz"].GetDouble();

  int cr = doc["coding_rate"].GetInt();
  if (cr < CodingRate_t::CR_MIN || cr > CodingRate_t::CR_MAX) {
    cr = CodingRate_t::CR_MIN;
  }
  result.lora_chip_settings.coding_rate = static_cast<CodingRate_t>(cr);

  result.lora_chip_settings.sync_word = (uint8_t) doc["sync_word"].GetUint();
  result.lora_chip_settings.preamble_length = (uint16_t) doc["preamble_length"].GetUint();

  result.latitude = (float) doc["latitude"].GetDouble();
  result.longtitude = (float) doc["longtitude"].GetDouble();
  result.altitude_meters = doc["altitude_meters"].GetInt();

  
  memset(result.platform_definition, 0, sizeof(result.platform_definition));
  strcpy(result.platform_definition, std::string(doc["platform_definition"].GetString()).substr(0, sizeof(PlatformInfo_t::platform_definition) - 1).c_str());

  memset(result.platform_email, 0, sizeof(result.platform_email));
  strcpy(result.platform_email, std::string(doc["platform_email"].GetString()).substr(0, sizeof(PlatformInfo_t::platform_email) - 1).c_str());

  memset(result.platform_description, 0, sizeof(result.platform_description));
  strcpy(result.platform_description, std::string(doc["platform_description"].GetString()).substr(0, sizeof(PlatformInfo_t::platform_description) - 1).c_str());
  
  const rapidjson::Value& serversArr = doc["servers"];
  for (rapidjson::SizeType i = 0; i < serversArr.Size(); i++) {
    if (!serversArr[i]["enabled"].GetBool()) continue;
    Server_t serv;
    serv.address = serversArr[i]["address"].GetString();
    serv.port = (uint16_t) serversArr[i]["port"].GetUint();
    // serv.enabled = true;
    result.servers.push_back(serv);
  }

  fclose(p_file);

  memset(result.__identifier, 0, sizeof(result.__identifier));
  if (identifier) {
    strncpy(result.__identifier, identifier, sizeof(result.__identifier) - 1);
  }

  return result; 
}
