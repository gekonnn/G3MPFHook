#pragma once

#include "framework.h"

namespace Utils
{
    std::string to_lower(std::string data);
    
    void dumpbytes(const char* buf, size_t bytes, size_t offset = 0,
        int bytes_per_row = 16, bool dump_chars = false,
        int dump_chars_spacing = 0);

    std::string uint16_to_hex(uint16_t value, bool uppercase = false);
}