#include <stdio.h>
#include "pico/stdlib.h"

namespace {

constexpr uint PIN_MISO = 8;
constexpr uint PIN_CSM = 9;
constexpr uint PIN_SCK = 10;
constexpr uint PIN_MOSI = 11;
constexpr uint PIN_CSAG = 13;

constexpr uint8_t WHO_AM_I_REG = 0x0F;
constexpr uint8_t EXPECTED_AG_ID = 0x68;
constexpr uint8_t EXPECTED_MAG_ID = 0x3D;

constexpr uint STATIC_TEST_MS = 5000;
constexpr uint HALF_CLOCK_US = 5;

void restore_idle_levels() {
    // LSM9DS1 SPI mode 3: SCK idles high and both chip selects are inactive high.
    gpio_put(PIN_CSAG, 1);
    gpio_put(PIN_CSM, 1);
    gpio_put(PIN_SCK, 1);
    gpio_put(PIN_MOSI, 0);
}

void init_gpio_for_software_spi() {
    gpio_init(PIN_MISO);
    gpio_set_dir(PIN_MISO, GPIO_IN);
    // A weak pull-up makes an undriven MISO line visibly read as 0xFF.
    // This is diagnostic only; the normal I-26 firmware does not enable it.
    gpio_pull_up(PIN_MISO);

    gpio_init(PIN_CSM);
    gpio_init(PIN_SCK);
    gpio_init(PIN_MOSI);
    gpio_init(PIN_CSAG);

    gpio_set_dir(PIN_CSM, GPIO_OUT);
    gpio_set_dir(PIN_SCK, GPIO_OUT);
    gpio_set_dir(PIN_MOSI, GPIO_OUT);
    gpio_set_dir(PIN_CSAG, GPIO_OUT);

    restore_idle_levels();
}

void hold_level(const char *label, uint pin, bool level) {
    printf("#STATIC %s GP%u = %s for %u s\n",
           label, pin, level ? "HIGH" : "LOW", STATIC_TEST_MS / 1000);
    gpio_put(pin, level ? 1 : 0);
    sleep_ms(STATIC_TEST_MS);
}

void static_pin_test() {
    printf("#--- Static pin test (measure these pins with a multimeter) ---\n");
    printf("#All tests keep the other SPI pins in safe/inactive states.\n");

    restore_idle_levels();
    hold_level("CSAG", PIN_CSAG, false);
    hold_level("CSAG", PIN_CSAG, true);

    restore_idle_levels();
    hold_level("CSM", PIN_CSM, false);
    hold_level("CSM", PIN_CSM, true);

    restore_idle_levels();
    // CS pins are high, so changing the clock here cannot issue a command.
    hold_level("SCK", PIN_SCK, false);
    hold_level("SCK", PIN_SCK, true);

    restore_idle_levels();
    hold_level("MOSI", PIN_MOSI, false);
    hold_level("MOSI", PIN_MOSI, true);

    restore_idle_levels();
    printf("#STATIC MISO GP%u idle level = %d (input only)\n",
           PIN_MISO, gpio_get(PIN_MISO));
    printf("#--- Static pin test complete ---\n");
}

uint8_t software_spi_mode3_transfer(uint8_t tx) {
    uint8_t rx = 0;

    for (int bit = 7; bit >= 0; --bit) {
        // CPOL=1, CPHA=1. Data is sampled on the rising edge.
        gpio_put(PIN_MOSI, (tx >> bit) & 0x01);
        sleep_us(HALF_CLOCK_US);

        gpio_put(PIN_SCK, 0);  // leading/falling edge
        sleep_us(HALF_CLOCK_US);

        gpio_put(PIN_SCK, 1);  // trailing/rising edge
        sleep_us(HALF_CLOCK_US);

        rx = static_cast<uint8_t>((rx << 1) | (gpio_get(PIN_MISO) ? 1 : 0));
    }

    return rx;
}

uint8_t read_register(uint cs_pin, uint8_t command) {
    restore_idle_levels();
    sleep_us(10);

    gpio_put(cs_pin, 0);
    sleep_us(10);

    (void)software_spi_mode3_transfer(command);
    const uint8_t value = software_spi_mode3_transfer(0x00);

    sleep_us(10);
    gpio_put(cs_pin, 1);
    sleep_us(10);
    return value;
}

uint8_t read_ag_who_am_i() {
    // A/G SPI: bit7=1 selects read. WHO_AM_I is register 0x0F.
    return read_register(PIN_CSAG, static_cast<uint8_t>(WHO_AM_I_REG | 0x80));
}

uint8_t read_mag_who_am_i() {
    // Magnetometer SPI: bit7=read, bit6=address auto-increment.
    // Auto-increment is harmless for this single-byte WHO_AM_I read and
    // matches the existing I-26 driver convention.
    return read_register(PIN_CSM,
                         static_cast<uint8_t>(WHO_AM_I_REG | 0x80 | 0x40));
}

void print_interpretation(uint8_t ag, uint8_t mag) {
    if (ag == EXPECTED_AG_ID && mag == EXPECTED_MAG_ID) {
        printf("#RESULT PASS: software SPI can communicate with both LSM9DS1 devices.\n");
        printf("#If the normal firmware still reports FF/FF, investigate the hardware-SPI configuration/path.\n");
    } else if (ag == 0xFF && mag == 0xFF) {
        printf("#RESULT FF/FF: neither device is driving MISO during software SPI.\n");
        printf("#Check SDO+DOM->GP8, CSAG/CSM, SCK, board power/GND, and the breakout itself.\n");
    } else if (ag == EXPECTED_AG_ID && mag != EXPECTED_MAG_ID) {
        printf("#RESULT: A/G responds, MAG does not. Focus on CSM/DOM and the magnetometer side.\n");
    } else if (ag != EXPECTED_AG_ID && mag == EXPECTED_MAG_ID) {
        printf("#RESULT: MAG responds, A/G does not. Focus on CSAG/SDO and the accel/gyro side.\n");
    } else {
        printf("#RESULT: both devices returned unexpected values; inspect SPI waveforms/levels and wiring.\n");
    }
}

}  // namespace

int main() {
    stdio_init_all();
    init_gpio_for_software_spi();

    // Give Windows enough time to enumerate USB CDC before the instructions begin.
    sleep_ms(3000);

    printf("\n#==============================================\n");
    printf("# I-26 LSM9DS1 SPI DIAGNOSTIC (software SPI)\n");
    printf("# MISO=GP8 CSM=GP9 SCK=GP10 MOSI=GP11 CSAG=GP13\n");
    printf("# Expected WHO_AM_I: A/G=0x68 MAG=0x3D\n");
    printf("# This firmware does NOT start motors or the flight-control loop.\n");
    printf("#==============================================\n");
    printf("#Static multimeter test starts in 5 seconds. Reboot Pico to repeat it.\n");
    sleep_ms(5000);

    static_pin_test();

    printf("#--- Continuous software-SPI WHO_AM_I test ---\n");
    while (true) {
        const uint8_t ag = read_ag_who_am_i();
        const uint8_t mag = read_mag_who_am_i();

        printf("#SW-SPI WHO_AM_I A/G=0x%02X (expected 0x%02X) MAG=0x%02X (expected 0x%02X) MISO-idle=%d\n",
               ag, EXPECTED_AG_ID, mag, EXPECTED_MAG_ID, gpio_get(PIN_MISO));
        print_interpretation(ag, mag);
        sleep_ms(1000);
    }

    return 0;
}
