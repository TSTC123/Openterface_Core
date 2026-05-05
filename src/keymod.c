#include "keymod.h"
#include <stdlib.h>
#include <string.h>

/* ── Checksum ───────────────────────────────────────────────────────────── */

uint8_t km_checksum(const uint8_t data[], int len) {
    uint32_t sum = 0;
    for (int i = 0; i < len - 1; i++) {
        sum += data[i];
    }
    return (uint8_t)(sum & 0xFF);
}

/* ── Hex dump ───────────────────────────────────────────────────────────── */

void km_hex_dump(const uint8_t data[], int len, char out[]) {
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < len; i++) {
        out[i * 2]     = hex[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex[data[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

/* ── Packet builders ────────────────────────────────────────────────────── */

/* Common 5-byte magic header: 57 AB 00 <cmd> <len> */
static const uint8_t KB_HDR[]  = { 0x57, 0xAB, 0x00, KM_CMD_KB,     0x08 };
static const uint8_t MS_HDR[]  = { 0x57, 0xAB, 0x00, KM_CMD_MS_REL, 0x05 };

int km_build_keyboard(uint8_t out[KM_PKT_KEYBOARD_SIZE],
                       uint8_t modifiers,
                       const uint8_t keys[],
                       int num_keys) {
    memcpy(out, KB_HDR, 5);
    out[5] = modifiers;
    out[6] = 0x00; /* reserved */
    int n = num_keys < 6 ? num_keys : 6;
    for (int i = 0; i < n; i++) {
        out[7 + i] = keys[i];
    }
    for (int i = n; i < 6; i++) {
        out[7 + i] = 0x00;
    }
    out[13] = km_checksum(out, KM_PKT_KEYBOARD_SIZE);
    return KM_PKT_KEYBOARD_SIZE;
}

int km_build_mouse_rel(uint8_t out[KM_PKT_MOUSE_REL_SIZE],
                        uint8_t buttons,
                        int8_t dx,
                        int8_t dy,
                        int8_t wheel) {
    memcpy(out, MS_HDR, 5);
    out[5] = 0x01;        /* relative mode */
    out[6] = buttons;
    out[7] = (uint8_t)dx;
    out[8] = (uint8_t)dy;
    out[9] = (uint8_t)wheel;
    out[10] = km_checksum(out, KM_PKT_MOUSE_REL_SIZE);
    return KM_PKT_MOUSE_REL_SIZE;
}

int km_build_press_release(uint8_t out[2 * KM_PKT_KEYBOARD_SIZE],
                            uint8_t modifiers,
                            uint8_t hid_code) {
    uint8_t keys[6] = { hid_code, 0, 0, 0, 0, 0 };
    km_build_keyboard(out, modifiers, keys, 1);

    /* Release: all zeros */
    uint8_t zeros[6] = { 0 };
    km_build_keyboard(out + KM_PKT_KEYBOARD_SIZE, 0x00, zeros, 0);
    return 2 * KM_PKT_KEYBOARD_SIZE;
}

/* ── HID code lookup ────────────────────────────────────────────────────── */

/*
 * We use two small lookup tables instead of a long switch:
 *   1) ascii_hid[96]  →  HID code for ASCII 0x20..0x7E  (' '..'~')
 *   2) name_map[]     →  string → HID for special keys
 */

/* ── ASCII → HID + shift table (index = c - 0x20) ───────────────────────── */

static const struct { uint8_t hid; uint8_t shift; } ascii_map[96] = {
    /* 0x20 ' '  */ { 0x2C, 0 },
    /* 0x21 '!'  */ { 0x1E, 1 },
    /* 0x22 '"'  */ { 0x34, 1 },
    /* 0x23 '#'  */ { 0x35, 1 },
    /* 0x24 '$'  */ { 0x21, 1 },
    /* 0x25 '%'  */ { 0x22, 1 },
    /* 0x26 '&'  */ { 0x23, 1 },
    /* 0x27 '''  */ { 0x34, 0 },
    /* 0x28 '('  */ { 0x26, 1 },
    /* 0x29 ')'  */ { 0x27, 1 },
    /* 0x2A '*'  */ { 0x25, 1 },
    /* 0x2B '+'  */ { 0x2E, 1 },
    /* 0x2C ','  */ { 0x36, 0 },
    /* 0x2D '-'  */ { 0x2D, 0 },
    /* 0x2E '.'  */ { 0x37, 0 },
    /* 0x2F '/'  */ { 0x38, 0 },
    /* 0x30 '0'  */ { 0x27, 0 },
    /* 0x31 '1'  */ { 0x1E, 0 },
    /* 0x32 '2'  */ { 0x1F, 0 },
    /* 0x33 '3'  */ { 0x20, 0 },
    /* 0x34 '4'  */ { 0x21, 0 },
    /* 0x35 '5'  */ { 0x22, 0 },
    /* 0x36 '6'  */ { 0x23, 0 },
    /* 0x37 '7'  */ { 0x24, 0 },
    /* 0x38 '8'  */ { 0x25, 0 },
    /* 0x39 '9'  */ { 0x26, 0 },
    /* 0x3A ':'  */ { 0x33, 1 },
    /* 0x3B ';'  */ { 0x33, 0 },
    /* 0x3C '<'  */ { 0x36, 1 },
    /* 0x3D '='  */ { 0x2E, 0 },
    /* 0x3E '>'  */ { 0x37, 1 },
    /* 0x3F '?'  */ { 0x38, 1 },
    /* 0x40 '@'  */ { 0x1F, 1 },
    /* 0x41 'A'  */ { 0x04, 1 },
    /* 0x42 'B'  */ { 0x05, 1 },
    /* 0x43 'C'  */ { 0x06, 1 },
    /* 0x44 'D'  */ { 0x07, 1 },
    /* 0x45 'E'  */ { 0x08, 1 },
    /* 0x46 'F'  */ { 0x09, 1 },
    /* 0x47 'G'  */ { 0x0A, 1 },
    /* 0x48 'H'  */ { 0x0B, 1 },
    /* 0x49 'I'  */ { 0x0C, 1 },
    /* 0x4A 'J'  */ { 0x0D, 1 },
    /* 0x4B 'K'  */ { 0x0E, 1 },
    /* 0x4C 'L'  */ { 0x0F, 1 },
    /* 0x4D 'M'  */ { 0x10, 1 },
    /* 0x4E 'N'  */ { 0x11, 1 },
    /* 0x4F 'O'  */ { 0x12, 1 },
    /* 0x50 'P'  */ { 0x13, 1 },
    /* 0x51 'Q'  */ { 0x14, 1 },
    /* 0x52 'R'  */ { 0x15, 1 },
    /* 0x53 'S'  */ { 0x16, 1 },
    /* 0x54 'T'  */ { 0x17, 1 },
    /* 0x55 'U'  */ { 0x18, 1 },
    /* 0x56 'V'  */ { 0x19, 1 },
    /* 0x57 'W'  */ { 0x1A, 1 },
    /* 0x58 'X'  */ { 0x1B, 1 },
    /* 0x59 'Y'  */ { 0x1C, 1 },
    /* 0x5A 'Z'  */ { 0x1D, 1 },
    /* 0x5B '['  */ { 0x2F, 0 },
    /* 0x5C '\'  */ { 0x64, 0 },
    /* 0x5D ']'  */ { 0x30, 0 },
    /* 0x5E '^'  */ { 0x23, 1 },
    /* 0x5F '_'  */ { 0x2D, 1 },
    /* 0x60 '`'  */ { 0x35, 0 },
    /* 0x61 'a'  */ { 0x04, 0 },
    /* 0x62 'b'  */ { 0x05, 0 },
    /* 0x63 'c'  */ { 0x06, 0 },
    /* 0x64 'd'  */ { 0x07, 0 },
    /* 0x65 'e'  */ { 0x08, 0 },
    /* 0x66 'f'  */ { 0x09, 0 },
    /* 0x67 'g'  */ { 0x0A, 0 },
    /* 0x68 'h'  */ { 0x0B, 0 },
    /* 0x69 'i'  */ { 0x0C, 0 },
    /* 0x6A 'j'  */ { 0x0D, 0 },
    /* 0x6B 'k'  */ { 0x0E, 0 },
    /* 0x6C 'l'  */ { 0x0F, 0 },
    /* 0x6D 'm'  */ { 0x10, 0 },
    /* 0x6E 'n'  */ { 0x11, 0 },
    /* 0x6F 'o'  */ { 0x12, 0 },
    /* 0x70 'p'  */ { 0x13, 0 },
    /* 0x71 'q'  */ { 0x14, 0 },
    /* 0x72 'r'  */ { 0x15, 0 },
    /* 0x73 's'  */ { 0x16, 0 },
    /* 0x74 't'  */ { 0x17, 0 },
    /* 0x75 'u'  */ { 0x18, 0 },
    /* 0x76 'v'  */ { 0x19, 0 },
    /* 0x77 'w'  */ { 0x1A, 0 },
    /* 0x78 'x'  */ { 0x1B, 0 },
    /* 0x79 'y'  */ { 0x1C, 0 },
    /* 0x7A 'z'  */ { 0x1D, 0 },
    /* 0x7B '{'  */ { 0x2F, 1 },
    /* 0x7C '|'  */ { 0x64, 1 },
    /* 0x7D '}'  */ { 0x30, 1 },
    /* 0x7E '~'  */ { 0x35, 1 },
};

int km_hid_code_for_char(char c, int *out_needs_shift) {
    if (c < 0x20 || c > 0x7E) {
        if (out_needs_shift) *out_needs_shift = 0;
        return -1;
    }
    int idx = c - 0x20;
    if (out_needs_shift) *out_needs_shift = ascii_map[idx].shift;
    return ascii_map[idx].hid;
}

/* ── Named key table ────────────────────────────────────────────────────── */

typedef struct { const char *name; uint8_t code; } name_entry_t;

static const name_entry_t name_map[] = {
    /* Letters (also in ascii_map, kept for macro token lookup) */
    { "A", 0x04 }, { "B", 0x05 }, { "C", 0x06 }, { "D", 0x07 },
    { "E", 0x08 }, { "F", 0x09 }, { "G", 0x0A }, { "H", 0x0B },
    { "I", 0x0C }, { "J", 0x0D }, { "K", 0x0E }, { "L", 0x0F },
    { "M", 0x10 }, { "N", 0x11 }, { "O", 0x12 }, { "P", 0x13 },
    { "Q", 0x14 }, { "R", 0x15 }, { "S", 0x16 }, { "T", 0x17 },
    { "U", 0x18 }, { "V", 0x19 }, { "W", 0x1A }, { "X", 0x1B },
    { "Y", 0x1C }, { "Z", 0x1D },

    /* Numbers */
    { "0", 0x27 }, { "1", 0x1E }, { "2", 0x1F }, { "3", 0x20 },
    { "4", 0x21 }, { "5", 0x22 }, { "6", 0x23 }, { "7", 0x24 },
    { "8", 0x25 }, { "9", 0x26 },

    /* Special */
    { "Enter",     0x28 }, { "Escape",    0x29 }, { "Backspace", 0x2A },
    { "Tab",       0x2B }, { "Space",     0x2C }, { "Caps",      0x39 },
    { "Delete",    0x4C }, { "Insert",    0x49 }, { "Home",      0x4A },
    { "End",       0x4D }, { "PageUp",    0x4B }, { "PageDown",  0x4E },
    { "PgUp",      0x4B }, { "PgDn",      0x4E },

    /* Arrows */
    { "Right",     0x4F }, { "Left",      0x50 },
    { "Down",      0x51 }, { "Up",        0x52 },

    /* Function keys */
    { "F1",  0x3A }, { "F2",  0x3B }, { "F3",  0x3C },
    { "F4",  0x3D }, { "F5",  0x3E }, { "F6",  0x3F },
    { "F7",  0x40 }, { "F8",  0x41 }, { "F9",  0x42 },
    { "F10", 0x43 }, { "F11", 0x44 }, { "F12", 0x45 },

    /* Numpad */
    { "Numpad0",      0x62 }, { "Numpad1",  0x59 },
    { "Numpad2",      0x5A }, { "Numpad3",  0x5B },
    { "Numpad4",      0x5C }, { "Numpad5",  0x5D },
    { "Numpad6",      0x5E }, { "Numpad7",  0x5F },
    { "Numpad8",      0x60 }, { "Numpad9",  0x61 },
    { "NumpadDot",    0x63 }, { "NumpadSlash",    0x54 },
    { "NumpadAsterisk", 0x55 }, { "NumpadMinus",  0x56 },
    { "NumpadPlus",   0x57 }, { "NumpadEnter",    0x58 },
    { "NumpadEquals", 0x67 }, { "NumLock",        0x53 },

    /* Modifiers */
    { "Ctrl",  0xE0 }, { "Shift", 0xE1 }, { "Alt",   0xE2 },
    { "Cmd",   0xE3 }, { "Win",   0xE3 },
};

static const int NAME_MAP_LEN = sizeof(name_map) / sizeof(name_map[0]);

int km_hid_code(const char *key_name) {
    if (!key_name || !key_name[0]) return -1;

    /* Single letter or digit — fast path via ascii_map */
    if (key_name[1] == '\0' && key_name[0] >= 0x20 && key_name[0] <= 0x7E) {
        return ascii_map[key_name[0] - 0x20].hid;
    }

    for (int i = 0; i < NAME_MAP_LEN; i++) {
        if (strcmp(name_map[i].name, key_name) == 0) {
            return name_map[i].code;
        }
    }
    return -1;
}

const char *km_hid_code_label(int hid_code) {
    for (int i = 0; i < NAME_MAP_LEN; i++) {
        if (name_map[i].code == (uint8_t)hid_code) {
            return name_map[i].name;
        }
    }
    return "Unknown";
}

/* ── Token / macro parser ───────────────────────────────────────────────── */

static int parse_modifier_open(const char *tok) {
    if (strcmp(tok, "<CTRL>")  == 0) return KM_MOD_CTRL;
    if (strcmp(tok, "<SHIFT>") == 0) return KM_MOD_SHIFT;
    if (strcmp(tok, "<ALT>")   == 0) return KM_MOD_ALT;
    if (strcmp(tok, "<CMD>")   == 0) return KM_MOD_GUI;
    if (strcmp(tok, "<WIN>")   == 0) return KM_MOD_GUI;
    return 0;
}

static int parse_modifier_close(const char *tok) {
    if (strcmp(tok, "</CTRL>")  == 0) return KM_MOD_CTRL;
    if (strcmp(tok, "</SHIFT>") == 0) return KM_MOD_SHIFT;
    if (strcmp(tok, "</ALT>")   == 0) return KM_MOD_ALT;
    if (strcmp(tok, "</CMD>")   == 0) return KM_MOD_GUI;
    if (strcmp(tok, "</WIN>")   == 0) return KM_MOD_GUI;
    return 0;
}

static int is_delay_token(const char *tok) {
    return strncmp(tok, "<DELAY", 6) == 0;
}

/* Map a special token like "<ENTER>" to its HID code */
static int special_token_hid(const char *tok) {
    /* Strip < > and compare */
    if (tok[0] != '<') return -1;
    const char *inner = tok + 1;
    size_t len = strlen(inner);
    if (len < 2 || inner[len - 1] != '>') return -1;

    /* Build a temp string without the trailing > */
    char buf[16];
    size_t inner_len = len - 1;
    if (inner_len >= sizeof(buf)) return -1;
    memcpy(buf, inner, inner_len);
    buf[inner_len] = '\0';

    /* Check special names (case-insensitive via quick uppercase) */
    if (strcmp(buf, "ENTER") == 0)     return 0x28;
    if (strcmp(buf, "ESC") == 0)       return 0x29;
    if (strcmp(buf, "BACK") == 0)      return 0x2A;
    if (strcmp(buf, "TAB") == 0)       return 0x2B;
    if (strcmp(buf, "SPACE") == 0)     return 0x2C;
    if (strcmp(buf, "LEFT") == 0)      return 0x50;
    if (strcmp(buf, "RIGHT") == 0)     return 0x4F;
    if (strcmp(buf, "UP") == 0)        return 0x52;
    if (strcmp(buf, "DOWN") == 0)      return 0x51;
    if (strcmp(buf, "HOME") == 0)      return 0x4A;
    if (strcmp(buf, "END") == 0)       return 0x4D;
    if (strcmp(buf, "DEL") == 0)       return 0x4C;
    if (strcmp(buf, "DELETE") == 0)    return 0x4C;
    if (strcmp(buf, "INS") == 0)       return 0x49;
    if (strcmp(buf, "INSERT") == 0)    return 0x49;
    if (strcmp(buf, "PGUP") == 0)      return 0x4B;
    if (strcmp(buf, "PAGEUP") == 0)    return 0x4B;
    if (strcmp(buf, "PGDN") == 0)      return 0x4E;
    if (strcmp(buf, "PAGEDOWN") == 0)  return 0x4E;

    /* Function keys F1-F12 */
    if (buf[0] == 'F' && buf[1] >= '0' && buf[1] <= '9') {
        int num = atoi(buf + 1);
        if (num >= 1 && num <= 12) {
            return (uint8_t)(0x3A + num - 1);
        }
    }

    return -1;
}

km_parsed_token_t km_parse_token(const char *token) {
    km_parsed_token_t result = { -1, 0 };

    if (!token || !token[0]) return result;

    /* Opening modifier tag — tracked, but no key emitted */
    if (parse_modifier_open(token)) return result;

    /* Closing modifier tag — ignored by single-token parse */
    if (parse_modifier_close(token)) return result;

    /* Delay token — ignored */
    if (is_delay_token(token)) return result;

    /* Special token like <ENTER> */
    if (token[0] == '<') {
        result.hid_code = special_token_hid(token);
        return result;
    }

    /* Single printable character */
    if (token[1] == '\0') {
        result.hid_code = km_hid_code_for_char(token[0], NULL);
    }

    return result;
}

int km_parse_macro(const char *input, km_parsed_token_t out[], int max) {
    int count = 0;
    int active_mods = 0;

    if (!input || !out || max <= 0) return 0;

    const char *p = input;
    while (*p && count < max) {
        if (*p == '<') {
            /* Extract full token including > */
            const char *start = p;
            while (*p && *p != '>') p++;
            if (*p == '>') p++; /* include closing > */

            size_t tok_len = (size_t)(p - start);
            char tok[32];
            if (tok_len >= sizeof(tok)) {
                tok_len = sizeof(tok) - 1;
            }
            memcpy(tok, start, tok_len);
            tok[tok_len] = '\0';

            /* Check modifier open/close */
            int mod = parse_modifier_open(tok);
            if (mod) { active_mods |= mod; continue; }
            int close = parse_modifier_close(tok);
            if (close) { active_mods &= ~close; continue; }
            if (is_delay_token(tok)) continue;

            /* Special token with HID code */
            int hid = special_token_hid(tok);
            if (hid >= 0) {
                out[count].hid_code  = hid;
                out[count].modifiers = active_mods;
                count++;
            }
        } else {
            /* Single character — case-insensitive for letters so <CTRL>C
               produces Ctrl+C (no extra Shift), matching standard shortcut syntax. */
            char ch = *p;
            if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
            int hid = km_hid_code_for_char(ch, NULL);
            if (hid >= 0) {
                int needs_shift = 0;
                km_hid_code_for_char(ch, &needs_shift);
                int mods = active_mods;
                if (needs_shift) mods |= KM_MOD_SHIFT;
                out[count].hid_code  = hid;
                out[count].modifiers = mods;
                count++;
            }
            p++;
        }
    }

    return count;
}
