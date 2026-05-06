#include "pch.h"

std::string Utils::to_lower(std::string data)
{
    std::transform(data.begin(), data.end(), data.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return data;
}

void Utils::dumpbytes(const char* buf, size_t bytes, size_t offset, int bytes_per_row, bool dump_chars, int dump_chars_spacing)
{
    if (offset >= bytes) return;

    for (size_t i = offset; i < bytes;) {
        printf("%08zX  ", i);

        size_t row_end = std::min(i + bytes_per_row, bytes);
        for (size_t j = i; j < row_end; ++j) {
            printf("%02X ", static_cast<unsigned char>(buf[j]));
            if (bytes_per_row > 8 && j == i + 7) printf(" ");
        }

        if (row_end < i + bytes_per_row) {
            size_t missing = i + bytes_per_row - row_end;
            for (size_t j = 0; j < missing; ++j) {
                printf("   ");
            }
            if (bytes_per_row > 8 && row_end <= i + 8) {
                printf(" ");
            }
        }

        if (dump_chars) {
            printf("\t");
            for (size_t j = i; j < row_end; ++j) {
                char c = buf[j];
                printf("%c", (c >= 32 && c <= 126) ? c : '.');
                for (int k = 0; k < dump_chars_spacing; ++k) {
                    printf(" ");
                }
            }
        }

        printf("\n");
        i = row_end;
    }
}

std::string Utils::uint16_to_hex(uint16_t value, bool uppercase)
{
    char buf[5];
    if (uppercase)
        snprintf(buf, sizeof(buf), "%02X", value);
    else
        snprintf(buf, sizeof(buf), "%02x", value);
    return std::string(buf);
}