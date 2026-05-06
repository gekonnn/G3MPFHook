#ifndef G3MMPINF_H
#define G3MMPINF_H

#include <stdlib.h>
#include <stdint.h>

#define G3MMPINF_MAGIC  "G3MMPINF"
#define G3MMPINF_VER    4

typedef enum {
    MODT_UNKNOWN,
    MODT_ARCHIVE,
    MODT_SCRIPT,
    MODT_STRINGTABLE,
    MODT_LRTPLDATASC,
    MODT_INI
} g3mmmod_t;

typedef enum {
    MODFILEARR_T_ALL_IN_DIRECTORY,
    MODFILEARR_T_SELECTED_FILES
} g3mmmod_filearr_t;

#pragma pack(push, 1)

typedef struct {
    uint16_t LENGTH;
    char* DATA; // '\0' terminated
} g3mmstr;

typedef struct {
    g3mmmod_t TYPE;
    g3mmstr FILEPATH;
} g3mmmodf;

typedef struct {
    g3mmstr NAME;
    g3mmmod_filearr_t FILE_ARRAY_TYPE;
    uint16_t FILE_COUNT;		// only if FILE_ARRAY_TYPE = MODFILEARR_T_SELECTED_FILES
    g3mmmodf* FILE_ARRAY;		// only if FILE_ARRAY_TYPE = MODFILEARR_T_SELECTED_FILES
    g3mmstr FILES_DIRECTORY; 	// only if FILE_ARRAY_TYPE = MODFILEARR_T_ALL_IN_DIRECTORY
} g3mmmod;

typedef struct {
    uint8_t SHOW_CONSOLE;
    g3mmstr LAUNCHER_PATH; // launch source path
    g3mmstr GAME_PATH;
} g3mminfo;

typedef struct {
    char MAGIC[8];
    uint16_t VERSION;
    g3mminfo INFO;
    uint16_t MOD_COUNT;
    g3mmmod* MOD_ARRAY;
} g3mmpinf;

#pragma pack(pop)

void g3mmstr_free(g3mmstr* str);
void g3mmmodf_free(g3mmmodf* modf);
void g3mmmod_free(g3mmmod* mod);
void g3mminfo_free(g3mminfo* inf);
void g3mmpinf_free(g3mmpinf* inf);
void g3mmstr_set(g3mmstr* str, const char* text);
void g3mmstr_init(g3mmstr* str);
void g3mmmod_init(g3mmmod* mod);
void g3mminfo_init(g3mminfo* inf);
void g3mmpinf_init(g3mmpinf* inf);
size_t g3mmstr_size(g3mmstr* str);
size_t g3mminfo_size(g3mminfo* inf);
size_t g3mmpinf_size(g3mmpinf* inf);
char* g3mmpinf_serialize(g3mmpinf* inf, size_t* out_size);
int g3mmpinf_deserialize(char* buf, size_t size, g3mmpinf* inf);

#endif // G3MMPINF_H
