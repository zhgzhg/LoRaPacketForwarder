#include "ConfigFileParser.h"

void PrintConfiguration(PlatformInfo_t &cfg)
{
  printf("\nPlatformInfo:\n");
  printf("------------\n\n");

  printf("ID (EUI-64): %s\n\n", cfg.__identifier);

  printf("Target LoRa Chip Model: %s\n\n", cfg.lora_chip_settings.ic_model.c_str());

  printf("SPI Settings:\n  SPI channel=%d\n  SPI clock speed=%d Hz\n\n", cfg.lora_chip_settings.spi_channel,
    cfg.lora_chip_settings.spi_speed_hz);

  printf("(WiringPI) Pins:\n  nss_cs=%d\n  dio0=%d\n  dio1=%d\n  rest=%d\n\n", cfg.lora_chip_settings.pin_nss_cs,
    cfg.lora_chip_settings.pin_dio0, cfg.lora_chip_settings.pin_dio1, cfg.lora_chip_settings.pin_rest);


  printf("LoRa %s Chip:\n  Freq=%f MHz\n  BW=%f KHz\n  SF=%d\n  CR=4/%d\n  SyncWord=0x%x\n  PreambleLength=%d\n\n",
    cfg.lora_chip_settings.ic_model.c_str(),
    cfg.lora_chip_settings.carrier_frequency_mhz,cfg.lora_chip_settings.bandwidth_khz,
    cfg.lora_chip_settings.spreading_factor, cfg.lora_chip_settings.coding_rate,
    cfg.lora_chip_settings.sync_word, cfg.lora_chip_settings.preamble_length);

  printf("Meta Information:\n  Latitude=%f\n  Longtitude=%f\n  Altitude=%d meters\n",
    cfg.latitude, cfg.longtitude, cfg.altitude_meters);

  printf("  Name/Definition: %s\n  E-mail: %s\n  Description: %s\n\n", cfg.platform_definition,
    cfg.platform_email, cfg.platform_description);
}

void SetGatewayIdentifier(PlatformInfo_t &cfg, const char identifier[25])
{
  memset(cfg.__identifier, 0, sizeof(cfg.__identifier));
  if (identifier)
  { strncpy(cfg.__identifier, identifier, sizeof(cfg.__identifier) - 1); }
}

PlatformInfo_t LoadConfiguration(std::string configurationFile)
{
  FILE* p_file = fopen(configurationFile.c_str(), "r");
  if (p_file == nullptr)
  {
    printf("Cannot open configuration file %s\n", configurationFile.c_str());
    exit(15);
  }

  char buffer[65536];
  rapidjson::FileReadStream fs(p_file, buffer, sizeof(buffer));

  rapidjson::Document doc;
  doc.ParseStream(fs);

  PlatformInfo_t result;

  result.lora_chip_settings.ic_model = std::string(doc["ic_model"].GetString());

  result.lora_chip_settings.spi_channel = (uint8_t) doc["spi_channel"].GetUint();
  if (result.lora_chip_settings.spi_channel > 1) {
    result.lora_chip_settings.spi_channel = 0;
  }

  result.lora_chip_settings.spi_speed_hz = doc["spi_speed_hz"].GetUint();
  if (result.lora_chip_settings.spi_speed_hz == 0) {
    result.lora_chip_settings.spi_speed_hz = 500000;
  }

  result.lora_chip_settings.pin_nss_cs = doc["pin_nss_cs"].GetInt();
  result.lora_chip_settings.pin_dio0 = doc["pin_dio0"].GetInt();
  result.lora_chip_settings.pin_dio1 = doc["pin_dio1"].GetInt();
  result.lora_chip_settings.pin_rest = doc["pin_rest"].GetInt();

  int sf = doc["spreading_factor"].GetInt();
  if (sf < SpreadingFactor_t::SF_MIN || sf > SpreadingFactor_t::SF_MAX) {
    result.lora_chip_settings.all_spreading_factors = (sf == SpreadingFactor_t::SF_ALL);
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
    serv.receive_timeout_ms = serversArr[i]["recv_timeout_ms"].GetUint();
    result.servers.push_back(serv);
  }

  fclose(p_file);

  return result;
}
