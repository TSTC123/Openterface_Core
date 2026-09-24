#include "openterface/protocol_ch9329.h"

#include <string.h>

uint8_t op_ch9329_checksum(const uint8_t data[], int len) {
    return op_input_checksum(data, len);
}

void op_ch9329_hex_dump(const uint8_t data[], int len, char out[]) {
    op_input_hex_dump(data, len, out);
}

int op_ch9329_build_keyboard_packet(uint8_t out[OP_CH9329_PKT_KEYBOARD_SIZE], uint8_t modifiers, const uint8_t keys[], int num_keys, uint8_t flags) {
    return op_input_build_keyboard(out, modifiers, keys, num_keys, flags);
}

int op_ch9329_build_mouse_rel_packet(uint8_t out[OP_CH9329_PKT_MOUSE_REL_SIZE], uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel) {
    return op_input_build_mouse_rel(out, buttons, dx, dy, wheel);
}

int op_ch9329_build_mouse_abs_packet(uint8_t out[OP_CH9329_PKT_MOUSE_ABS_SIZE], uint8_t buttons, uint16_t x, uint16_t y, int8_t wheel) {
    return op_input_build_mouse_abs(out, buttons, x, y, wheel);
}

int op_ch9329_build_press_release_packets(uint8_t out[2 * OP_CH9329_PKT_KEYBOARD_SIZE], uint8_t modifiers, uint8_t hid_code, uint8_t flags) {
    return op_input_build_press_release(out, modifiers, hid_code, flags);
}

int op_ch9329_build_usb_switch_packet(uint8_t out[OP_CH9329_PKT_USB_SWITCH_SIZE], uint8_t request_type) {
    if (out == NULL) {
        return 0;
    }

    memset(out, 0, OP_CH9329_PKT_USB_SWITCH_SIZE);
    out[0] = OP_CH9329_HEADER_0;
    out[1] = OP_CH9329_HEADER_1;
    out[2] = OP_CH9329_ADDR_DEFAULT;
    out[3] = OP_CH9329_CMD_USB_SWITCH;
    out[4] = 0x05u;
    out[9] = request_type;
    out[10] = op_ch9329_checksum(out, OP_CH9329_PKT_USB_SWITCH_SIZE);
    return (int)OP_CH9329_PKT_USB_SWITCH_SIZE;
}

op_status_t op_ch9329_parse_usb_switch_response(const uint8_t *packet, size_t length, uint8_t *out_status) {
    if (packet == NULL || out_status == NULL) {
        return OP_STATUS_INVALID_ARGUMENT;
    }

    if (length < OP_CH9329_PKT_USB_SWITCH_RESPONSE_SIZE) {
        return OP_STATUS_IO_ERROR;
    }

    if (packet[0] != OP_CH9329_HEADER_0 || packet[1] != OP_CH9329_HEADER_1 || packet[2] != OP_CH9329_ADDR_DEFAULT) {
        return OP_STATUS_IO_ERROR;
    }

    if (packet[3] != OP_CH9329_RESP_USB_SWITCH || packet[4] != 0x01u) {
        return OP_STATUS_IO_ERROR;
    }

    if (op_ch9329_checksum(packet, (int)OP_CH9329_PKT_USB_SWITCH_RESPONSE_SIZE) != packet[OP_CH9329_PKT_USB_SWITCH_RESPONSE_SIZE - 1u]) {
        return OP_STATUS_IO_ERROR;
    }

    if (packet[5] != OP_CH9329_USB_SWITCH_HOST && packet[5] != OP_CH9329_USB_SWITCH_TARGET) {
        return OP_STATUS_NOT_SUPPORTED;
    }

    *out_status = packet[5];
    return OP_STATUS_OK;
}

op_status_t op_ch9329_parse_packet(const uint8_t *raw, int len,
                                    op_ch9329_parsed_packet_t *out) {
    uint8_t data_len;
    int expected_len;

    if (raw == NULL || out == NULL) {
        return OP_STATUS_INVALID_ARGUMENT;
    }

    /* Minimum packet: 5-byte header + 1-byte checksum = 6 bytes */
    if (len < (int)OP_CH9329_MIN_PACKET_SIZE) {
        return OP_STATUS_IO_ERROR;
    }

    /* Header check */
    if (raw[0] != OP_CH9329_HEADER_0 || raw[1] != OP_CH9329_HEADER_1) {
        return OP_STATUS_IO_ERROR;
    }

    /* Extract payload length from header[4] */
    data_len = raw[4];

    /* Expected total: 5 header + data_len payload + 1 checksum */
    expected_len = 5 + (int)data_len + 1;
    if (len != expected_len) {
        return OP_STATUS_IO_ERROR;
    }

    /* Fill parsed result */
    memcpy(out->header, raw, 5);
    memcpy(out->data, raw + 5, data_len);
    out->data_len = data_len;
    out->checksum = raw[len - 1];
    out->checksum_calc = op_ch9329_checksum(raw, len);
    out->valid = (out->checksum == out->checksum_calc);

    if (!out->valid) {
        return OP_STATUS_IO_ERROR;
    }

    return OP_STATUS_OK;
}
