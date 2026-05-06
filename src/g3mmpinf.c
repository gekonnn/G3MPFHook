#include "pch.h"

#include <stdio.h>
#include <string.h>

#define _CRT_SECURE_NO_WARNINGS

void g3mmstr_free(g3mmstr* str) {
    if (str->DATA)
        free(str->DATA);

    str->DATA = NULL;
    str->LENGTH = 0;
}

void g3mmmodf_free(g3mmmodf* modf) {
    g3mmstr_free(&modf->FILEPATH);
}

void g3mmmod_free(g3mmmod* mod) {
    if (mod->FILE_ARRAY_TYPE == MODFILEARR_T_SELECTED_FILES)
    {
        for (uint16_t i = 0; i < mod->FILE_COUNT; ++i) {
            g3mmmodf_free(&mod->FILE_ARRAY[i]);
        }
        free(mod->FILE_ARRAY);
    }

    mod->FILE_ARRAY = NULL;
    mod->FILE_COUNT = 0;

    g3mmstr_free(&mod->FILES_DIRECTORY);
    g3mmstr_free(&mod->NAME);
}

void g3mminfo_free(g3mminfo* inf)
{
    inf->SHOW_CONSOLE = 0;
    g3mmstr_free(&inf->LAUNCHER_PATH);
    g3mmstr_free(&inf->GAME_PATH);
}

void g3mmpinf_free(g3mmpinf* inf) {
    for (uint16_t i = 0; i < inf->MOD_COUNT; ++i) {
        g3mmmod_free(&inf->MOD_ARRAY[i]);
    }

    if (inf->MOD_ARRAY)
        free(inf->MOD_ARRAY);

    g3mminfo_free(&inf->INFO);
    memset(inf, 0, sizeof(g3mmpinf));
}

void g3mmstr_set(g3mmstr* str, const char* text) {
    if (str->DATA != NULL)
        free(str->DATA);

    size_t length = strlen(text);
    str->DATA = (char*)malloc(length + 1);

    if (str->DATA != NULL) {
        memcpy(str->DATA, text, length + 1);
        str->LENGTH = (uint16_t)length;
    }
    else {
        str->LENGTH = 0;
    }
}

void g3mmstr_init(g3mmstr* str) {
    str->LENGTH = 0;
    str->DATA = NULL;
}

void g3mmmod_init(g3mmmod* mod) {
    mod->FILE_COUNT = 0;
    mod->FILE_ARRAY = NULL;
    mod->FILE_ARRAY_TYPE = MODFILEARR_T_SELECTED_FILES;
    g3mmstr_init(&mod->FILES_DIRECTORY);
}

void g3mminfo_init(g3mminfo* inf) {
    inf->SHOW_CONSOLE = 0;
    g3mmstr_init(&inf->LAUNCHER_PATH);
    g3mmstr_init(&inf->GAME_PATH);
}

void g3mmpinf_init(g3mmpinf* inf) {
    const char* magic = G3MMPINF_MAGIC;
    memcpy(inf->MAGIC, magic, strlen(magic));
    inf->VERSION = G3MMPINF_VER;
    g3mminfo_init(&inf->INFO);
    inf->MOD_COUNT = 0;
    inf->MOD_ARRAY = NULL;
}

size_t g3mmstr_size(g3mmstr* str) {
    size_t size = 0;
    size += sizeof(str->LENGTH); //  LENGTH
    size += str->LENGTH + 1;     //  DATA + '\0'
    return size;
}

size_t g3mminfo_size(g3mminfo* inf) {
    size_t size = 0;
    size += sizeof(inf->SHOW_CONSOLE);
    size += g3mmstr_size(&inf->LAUNCHER_PATH);
    size += g3mmstr_size(&inf->GAME_PATH);
    return size;
}

size_t g3mmpinf_size(g3mmpinf* inf) {
    size_t size = 0;

    size += sizeof(inf->MAGIC);
    size += sizeof(inf->VERSION);
    size += g3mminfo_size(&inf->INFO);

    size += sizeof(inf->MOD_COUNT);
    for (uint16_t i = 0; i < inf->MOD_COUNT; ++i) {
        g3mmmod* mod = &inf->MOD_ARRAY[i];
        size += g3mmstr_size(&mod->NAME);
        size += 1; // FILE_ARRAY_TYPE

        if (mod->FILE_ARRAY_TYPE == MODFILEARR_T_SELECTED_FILES)
        {
            size += sizeof(mod->FILE_COUNT);
            for (uint16_t j = 0; j < mod->FILE_COUNT; ++j) {
                g3mmmodf* modf = &mod->FILE_ARRAY[j];
                size += 1; // TYPE
                size += g3mmstr_size(&modf->FILEPATH);
            }
        }
        else if (mod->FILE_ARRAY_TYPE == MODFILEARR_T_ALL_IN_DIRECTORY)
            size += g3mmstr_size(&mod->FILES_DIRECTORY);
    }

    return size;
}

// serialization stuff

void write_bytes(char** ptr, void* src, size_t size) {
    memcpy(*ptr, src, size);
    *ptr += size;
}

void write_uint16(char** ptr, uint16_t val) {
    write_bytes(ptr, &val, sizeof(val));
}

void write_uint8(char** ptr, uint8_t val) {
    write_bytes(ptr, &val, sizeof(val));
}

void write_g3mmstr(char** ptr, g3mmstr str) {
    write_uint16(ptr, str.LENGTH);
    write_bytes(ptr, str.DATA, str.LENGTH + 1);
}

void write_g3mminfo(char** ptr, g3mminfo inf) {
    write_uint8(ptr, inf.SHOW_CONSOLE);
    write_g3mmstr(ptr, inf.LAUNCHER_PATH);
    write_g3mmstr(ptr, inf.GAME_PATH);
}

char* g3mmpinf_serialize(g3mmpinf* inf, size_t* out_size) {
    *out_size = g3mmpinf_size(inf);
    char* buf = (char*)malloc(*out_size);
    char* ptr = buf;

    write_bytes(&ptr, inf->MAGIC, 8);
    write_uint16(&ptr, inf->VERSION);
    write_g3mminfo(&ptr, inf->INFO);
    write_uint16(&ptr, inf->MOD_COUNT);

    for (uint16_t i = 0; i < inf->MOD_COUNT; ++i) {
        g3mmmod* mod = &inf->MOD_ARRAY[i];
        write_g3mmstr(&ptr, mod->NAME);
        write_uint8(&ptr, mod->FILE_ARRAY_TYPE);

        if (mod->FILE_ARRAY_TYPE == MODFILEARR_T_SELECTED_FILES)
        {
            write_uint16(&ptr, mod->FILE_COUNT);

            for (uint16_t j = 0; j < mod->FILE_COUNT; ++j) {
                g3mmmodf* modf = &mod->FILE_ARRAY[j];
                write_uint8(&ptr, modf->TYPE);
                write_g3mmstr(&ptr, modf->FILEPATH);
            }
        }
        else if (mod->FILE_ARRAY_TYPE == MODFILEARR_T_ALL_IN_DIRECTORY)
        {
            write_g3mmstr(&ptr, mod->FILES_DIRECTORY);
        }
    }

    return buf;
}

// deserialization stuff

void read_bytes(char** ptr, void* dst, size_t size) {
    memcpy(dst, *ptr, size);
    *ptr += size;
}

uint16_t read_uint16(char** ptr) {
    uint16_t val;
    read_bytes(ptr, &val, sizeof(val));
    return val;
}

uint8_t read_uint8(char** ptr) {
    uint8_t val;
    read_bytes(ptr, &val, sizeof(val));
    return val;
}

g3mmstr read_g3mmstr(char** ptr) {
    g3mmstr str;
    str.LENGTH = read_uint16(ptr);
    str.DATA = (char*)malloc(str.LENGTH + 1);
    read_bytes(ptr, str.DATA, str.LENGTH + 1);
    return str;
}

g3mminfo read_g3mminfo(char** ptr) {
    g3mminfo inf;
    inf.SHOW_CONSOLE = read_uint8(ptr);
    inf.LAUNCHER_PATH = read_g3mmstr(ptr);
    inf.GAME_PATH = read_g3mmstr(ptr);
    return inf;
}

int g3mmpinf_deserialize(char* buf, size_t size, g3mmpinf* inf) {
    char* ptr = buf;

    memcpy(inf->MAGIC, ptr, 8);
    if (strncmp(inf->MAGIC, G3MMPINF_MAGIC, sizeof(inf->MAGIC)) != 0) return 1;

    ptr += 8;

    inf->VERSION = read_uint16(&ptr);
    inf->INFO = read_g3mminfo(&ptr);
    inf->MOD_COUNT = read_uint16(&ptr);
    inf->MOD_ARRAY = (g3mmmod*)malloc(sizeof(g3mmmod) * inf->MOD_COUNT);
    if (inf->MOD_ARRAY == NULL) return 1;

    for (uint16_t i = 0; i < inf->MOD_COUNT; ++i) {
        g3mmmod* mod = &inf->MOD_ARRAY[i];
        g3mmmod_init(mod);

        mod->NAME = read_g3mmstr(&ptr);
        mod->FILE_ARRAY_TYPE = (g3mmmod_filearr_t)read_uint8(&ptr);

        if (mod->FILE_ARRAY_TYPE == MODFILEARR_T_SELECTED_FILES)
        {
            mod->FILE_COUNT = read_uint16(&ptr);
            mod->FILE_ARRAY = (g3mmmodf*)malloc(sizeof(g3mmmodf) * mod->FILE_COUNT);
            if (mod->FILE_ARRAY == NULL) return 1;

            for (uint16_t j = 0; j < mod->FILE_COUNT; ++j) {
                g3mmmodf* modf = &mod->FILE_ARRAY[j];
                modf->TYPE = (g3mmmod_t)read_uint8(&ptr);
                modf->FILEPATH = read_g3mmstr(&ptr);
            }
        }
        else if (mod->FILE_ARRAY_TYPE == MODFILEARR_T_ALL_IN_DIRECTORY)
        {
            mod->FILES_DIRECTORY = read_g3mmstr(&ptr);
        }
    }

    return 0;
}
