//#include "pico/stdlib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"

#include "usb_descriptors.h"

#define INTERVAL_MS 10


#define KEYBOARD 0x01
#define GAMEPAD 0x02

struct State {
    uint32_t pressed = 0;
} state;

void hid_task(State &state);

int main() {
    board_init();
    tusb_init();

    uint32_t start_ms = 0;
    while (true) {
        tud_task();

        if (board_millis() - start_ms >= INTERVAL_MS) {
            start_ms += INTERVAL_MS;
            hid_task(state);
        }
    }

    return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    if (report_type == HID_REPORT_TYPE_OUTPUT) {
        if (report_id == REPORT_ID_KEYBOARD) {
            if (bufsize < 1) return;

            uint8_t const kbd_leds = buffer[0];
            if (kbd_leds & KEYBOARD_LED_CAPSLOCK) {
                board_led_write(true);
            } else {
                board_led_write(false);
            }
        }
    }
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint8_t report_id, State &state, uint32_t btn) {
    if (!tud_hid_ready()) return;

    if (report_id == REPORT_ID_KEYBOARD) {
        if (btn) {
            state.pressed |= KEYBOARD;

            uint8_t keycode[6] = {0};
            keycode[0] = HID_KEY_A;

            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
        } else {
            if (state.pressed & KEYBOARD) {
                state.pressed &= ~KEYBOARD;
                tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
            }
        }

    } else if (report_id == REPORT_ID_GAMEPAD) {
        hid_gamepad_report_t report = {
            .x = 0,
            .y = 0,
            .z = 0,
            .rz = 0,
            .rx = 0,
            .ry = 0,
            .hat = 0,
            .buttons = 0
        };
        if (btn) {
            state.pressed |= GAMEPAD;

            report.hat = GAMEPAD_HAT_UP;
            report.buttons = GAMEPAD_BUTTON_A;
            tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));

        } else {
            report.hat = GAMEPAD_HAT_CENTERED;
            report.buttons = 0;

            if (state.pressed & GAMEPAD) {
                tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
                state.pressed &= ~GAMEPAD;
            }
        }
    }
}

void hid_task(State &state) {
    uint32_t const btn = board_button_read();
    if (tud_suspended()) return;

    send_hid_report(REPORT_ID_GAMEPAD, state, btn);

}


