#ifndef KEYMOD_H
#define KEYMOD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ─────────────────────────────────────────────────────────────── */
#define KEYMOD_VERSION_MAJOR 0
#define KEYMOD_VERSION_MINOR 1
#define KEYMOD_VERSION_PATCH 0

/* ── Modifier bitmask ───────────────────────────────────────────────────── */
#define KM_MOD_NONE  0x00
#define KM_MOD_CTRL  0x01
#define KM_MOD_SHIFT 0x02
#define KM_MOD_ALT   0x04
#define KM_MOD_GUI   0x08

/* ── Packet size constants ──────────────────────────────────────────────── */
#define KM_PKT_KEYBOARD_SIZE  14  /* 5 header + 8 data + 1 checksum */
#define KM_PKT_MOUSE_REL_SIZE 11  /* 5 header + 5 data + 1 checksum */

/* ── CH9329 command codes ───────────────────────────────────────────────── */
#define KM_CMD_KB       0x02
#define KM_CMD_MS_REL   0x05

/* ── Mouse button mask ──────────────────────────────────────────────────── */
#define KM_MS_BTN_NONE   0x00
#define KM_MS_BTN_LEFT   0x01
#define KM_MS_BTN_RIGHT  0x02
#define KM_MS_BTN_MIDDLE 0x04

/* ── HID usage code lookup ──────────────────────────────────────────────── */

/**
 * Look up a HID usage code by key name.
 *
 * Recognised names: "A"–"Z", "a"–"z", "0"–"9", "Enter", "Escape",
 * "Backspace", "Tab", "Space", "-", "=", "[", "]", "\\", ";", "'",
 * "`", ",", ".", "/", "Caps", "F1"–"F12", "Delete", "Insert",
 * "Home", "End", "PageUp", "PageDown", "Right", "Left", "Down", "Up",
 * "Ctrl", "Shift", "Alt", "Cmd", "Win", "Numpad0"–"Numpad9",
 * "NumpadDot", "NumpadSlash", "NumpadAsterisk", "NumpadMinus",
 * "NumpadPlus", "NumpadEnter", "NumpadEquals", "NumLock".
 *
 * @return  HID usage code (4–0xE6), or -1 if unknown.
 */
int km_hid_code(const char *key_name);

/**
 * Map a printable ASCII character to its HID usage code.
 *
 * @param  c            The character to map.
 * @param  out_needs_shift  Set to 1 if the character requires Shift.
 * @return              HID usage code, or -1 if unmappable.
 */
int km_hid_code_for_char(char c, int *out_needs_shift);

/**
 * Convert a HID usage code to a human-readable label.
 *
 * @param  hid_code  The HID usage code.
 * @return           Static string label (e.g. "Enter"), or "Unknown".
 */
const char *km_hid_code_label(int hid_code);

/* ── Packet builders ────────────────────────────────────────────────────── */

/**
 * Build a CH9329 keyboard packet.
 *
 * Format: 57 AB 00 02 08 | modifier | 00 | key1..key6 | checksum
 *
 * @param  out         Output buffer (must be >= KM_PKT_KEYBOARD_SIZE bytes).
 * @param  modifiers   Modifier bitmask (KM_MOD_*).
 * @param  keys        Array of up to 6 HID usage codes (zeros pad empty slots).
 * @param  num_keys    Number of keys (0–6).
 * @return             Actual packet length (always KM_PKT_KEYBOARD_SIZE).
 */
int km_build_keyboard(uint8_t out[KM_PKT_KEYBOARD_SIZE],
                       uint8_t modifiers,
                       const uint8_t keys[],
                       int num_keys);

/**
 * Build a CH9329 relative-mouse packet.
 *
 * Format: 57 AB 00 05 05 | 01 | buttons | deltaX | deltaY | wheel | checksum
 *
 * @param  out       Output buffer (must be >= KM_PKT_MOUSE_REL_SIZE bytes).
 * @param  buttons   Button bitmask (KM_MS_BTN_*).
 * @param  dx        Signed X delta (-128..127).
 * @param  dy        Signed Y delta (-128..127).
 * @param  wheel     Scroll wheel delta (-128..127).
 * @return           Actual packet length (always KM_PKT_MOUSE_REL_SIZE).
 */
int km_build_mouse_rel(uint8_t out[KM_PKT_MOUSE_REL_SIZE],
                        uint8_t buttons,
                        int8_t dx,
                        int8_t dy,
                        int8_t wheel);

/**
 * Build a CH9329 keyboard packet for a single key press + release sequence.
 *
 * Writes two packets consecutively into @p out: first the press (with
 * @p modifiers and @p hid_code), then the release (all zeros).
 *
 * @param  out         Output buffer (must be >= 2 * KM_PKT_KEYBOARD_SIZE).
 * @param  modifiers   Modifier bitmask for the press.
 * @param  hid_code    HID usage code of the key.
 * @return             Total bytes written (2 * KM_PKT_KEYBOARD_SIZE).
 */
int km_build_press_release(uint8_t out[2 * KM_PKT_KEYBOARD_SIZE],
                            uint8_t modifiers,
                            uint8_t hid_code);

/* ── Token / macro parser ───────────────────────────────────────────────── */

/**
 * Parsed result from km_parse_token().
 */
typedef struct {
    int  hid_code;    /* HID usage code, -1 if invalid */
    int  modifiers;   /* Modifier bitmask at time of key recognition */
} km_parsed_token_t;

/**
 * Parse a macro token string into a HID code + modifier bitmask.
 *
 * Understands tokens like "<CTRL>C</CTRL>", "<CMD><SHIFT>V</SHIFT></CMD>",
 * plain characters ("a", "1", " "), and special tokens ("<ESC>", "<F1>",
 * "<ENTER>", etc.).  Delay tokens like "<DELAY1S>" are silently skipped.
 *
 * @param  token  Null-terminated token string.
 * @return        Parsed result (hid_code >= 0 on success).
 */
km_parsed_token_t km_parse_token(const char *token);

/**
 * Parse a full macro string with embedded tokens into an array of
 * (hid_code, modifiers_at_key) pairs.
 *
 * The parser walks the input, tracking open/close modifier tags, and
 * emits one entry per printable key / special token encountered.
 *
 * @param  input     Null-terminated macro string (e.g. "<CTRL>A</CTRL>").
 * @param  out       Output array (caller-allocated, at least @p max entries).
 * @param  max       Maximum number of entries to write.
 * @return           Number of entries written.
 */
int km_parse_macro(const char *input, km_parsed_token_t out[], int max);

/* ── Utility ────────────────────────────────────────────────────────────── */

/**
 * Compute the CH9329 checksum for a packet (sum of all bytes except the last,
 * masked to 8 bits).
 *
 * @param  data  Packet buffer.
 * @param  len   Length of the full packet (including checksum slot).
 * @return       Computed checksum byte.
 */
uint8_t km_checksum(const uint8_t data[], int len);

/**
 * Format a packet as an uppercase hex string (for debugging).
 *
 * @param  data   Packet bytes.
 * @param  len    Number of bytes.
 * @param  out    Output buffer (must be >= 2*len + 1 bytes).
 */
void km_hex_dump(const uint8_t data[], int len, char out[]);

#ifdef __cplusplus
}
#endif

#endif /* KEYMOD_H */
