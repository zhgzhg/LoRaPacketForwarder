#ifndef TTN_CFG_FILE_PARSER_H
#define TTN_CFG_FILE_PARSER_H

#ifdef LINUX
        #undef String
        #undef max
        #undef ceil
#endif

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "config.h"

PlatformInfo_t LoadConfiguration(std::string configurationFile, const char identifier[129]);
void PrintConfiguration(PlatformInfo_t &cfg);

#endif
