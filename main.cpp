//#include "pico/stdlib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "bsp/board.h"
#include "tusb.h"

#include "usb_descriptors.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"

#define INTERVAL_MS 10


#define KEYBOARD 0x01
#define GAMEPAD 0x02

struct State {
    uint32_t pressed = 0;

    const uint16_t adc_min = 0x800;
    int8_t sensor = -(1 << 7);
} state;

template<typename T>
float inv_lerp(T val, T min, T max) {
    return ((float) (val - min)) / ((float) (max - min));
}

template<typename T>
T lerp(float a, T min, T max) {
    return (T) (a * (float) (max - min)) + min;
}

void hid_task(State &state);
void update(State &state);

int main() {
    board_init();
    tusb_init();
    adc_init();

    adc_gpio_init(26);
    adc_select_input(0);

    uint32_t start_ms = 0;
    while (true) {
        tud_task();

        update(state);

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

static void send_handbrake_data(State &state) {
    if (!tud_hid_ready()) return;
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
    report.hat = GAMEPAD_HAT_CENTERED;
    report.rx = state.sensor;
    tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
    /* if (btn) { */
    /*     state.pressed |= GAMEPAD; */

    /*     report.hat = GAMEPAD_HAT_UP; */
    /*     report.buttons = GAMEPAD_BUTTON_A; */
    /*     report.rx = (int8_t) state.sensor; */
    /*     tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report)); */

    /* } else { */
    /*     report.hat = GAMEPAD_HAT_CENTERED; */
    /*     report.buttons = 0; */

    /*     if (state.pressed & GAMEPAD) { */
    /*         tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report)); */
    /*         state.pressed &= ~GAMEPAD; */
    /*     } */
    /* } */
}

void update(State &state) {
    const uint16_t adc_max = 0xFFF; // pico has 12 bits ADC

    uint16_t adc = adc_read();
    /* if (adc < state.adc_min && board_millis() < 1000) { */
        /* state.adc_min = adc; */
    /* } */

    uint16_t adc_begin = state.adc_min + (uint16_t) ((adc_max - state.adc_min) * 0.05f);

    float a = inv_lerp(adc, adc_begin, adc_max);
    if (a < 0) a = 0.0f;
    a = sqrt(a);

    state.sensor = (int8_t) lerp<int32_t>(a, -(1 << 7), (1 << 7) - 1);
}

void hid_task(State &state) {
    uint32_t const btn = board_button_read();
    if (tud_suspended()) return;

    send_handbrake_data(state);

}


