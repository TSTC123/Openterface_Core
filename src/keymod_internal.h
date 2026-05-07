#ifndef KEYMOD_INTERNAL_H
#define KEYMOD_INTERNAL_H

#include "../include/keymod.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t hid;
    uint8_t shift;
} ascii_entry_t;

typedef struct {
    const char *name;
    uint8_t code;
} name_entry_t;

typedef struct {
    km_parsed_token_t *out;
    int count;
    int max;
    int active_mods;
} macro_parse_state_t;

extern const ascii_entry_t km_ascii_map[96];
extern const name_entry_t km_name_map[];
extern const int km_name_map_len;

int km_ascii_index_for_char(char c);
int km_lookup_named_key_code(const char *key_name);
const char *km_lookup_named_key_label(uint8_t hid_code);

#endif /* KEYMOD_INTERNAL_H */