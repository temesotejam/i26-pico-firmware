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
constexpr uint I2C_HALF_US = 10;

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
    // Weak pull-up is diagnostic only. If neither LSM9DS1 output drives MISO,
    // reads become a very obvious 0xFF instead of a random floating value.
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
    printf("#--- Static pin test (measure these pins at the LSM9DS1 header) ---\n");
    printf("#All tests keep the other SPI pins in safe/inactive states.\n");

    restore_idle_levels();
    hold_level("CSAG", PIN_CSAG, false);
    hold_level("CSAG", PIN_CSAG, true);

    restore_idle_levels();
    hold_level("CSM", PIN_CSM, false);
    hold_level("CSM", PIN_CSM, true);

    restore_idle_levels();
    hold_level("SCK", PIN_SCK, false);
    hold_level("SCK", PIN_SCK, true);

    restore_idle_levels();
    hold_level("MOSI", PIN_MOSI, false);
    hold_level("MOSI", PIN_MOSI, true);

    restore_idle_levels();
    printf("#STATIC MISO GP%u idle level = %d (input only, weak pull-up enabled)\n",
           PIN_MISO, gpio_get(PIN_MISO));
    printf("#--- Static pin test complete ---\n");
}

uint8_t software_spi_mode3_transfer(uint8_t tx) {
    uint8_t rx = 0;

    for (int bit = 7; bit >= 0; --bit) {
        gpio_put(PIN_MOSI, (tx >> bit) & 0x01);
        sleep_us(HALF_CLOCK_US);

        gpio_put(PIN_SCK, 0);  // leading/falling edge
        sleep_us(HALF_CLOCK_US);

        gpio_put(PIN_SCK, 1);  // trailing/rising sampling edge
        sleep_us(HALF_CLOCK_US);

        rx = static_cast<uint8_t>((rx << 1) | (gpio_get(PIN_MISO) ? 1 : 0));
    }

    return rx;
}

uint8_t spi_read_register(uint cs_pin, uint8_t command) {
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

uint8_t spi_read_ag_who_am_i() {
    return spi_read_register(PIN_CSAG, static_cast<uint8_t>(WHO_AM_I_REG | 0x80));
}

uint8_t spi_read_mag_who_am_i() {
    // Single-byte read only needs the read bit. Avoid the auto-increment bit here
    // so this matches Adafruit's documented single-register SPI transaction.
    return spi_read_register(PIN_CSM, static_cast<uint8_t>(WHO_AM_I_REG | 0x80));
}

void print_spi_interpretation(uint8_t ag, uint8_t mag) {
    if (ag == EXPECTED_AG_ID && mag == EXPECTED_MAG_ID) {
        printf("#SPI RESULT PASS: software SPI can communicate with both devices.\n");
    } else if (ag == 0xFF && mag == 0xFF) {
        printf("#SPI RESULT FF/FF: neither device drove MISO low during the read.\n");
    } else if (ag == EXPECTED_AG_ID && mag != EXPECTED_MAG_ID) {
        printf("#SPI RESULT: A/G responds, MAG does not. Focus on CSM/DOM.\n");
    } else if (ag != EXPECTED_AG_ID && mag == EXPECTED_MAG_ID) {
        printf("#SPI RESULT: MAG responds, A/G does not. Focus on CSAG/SDOAG.\n");
    } else {
        printf("#SPI RESULT: unexpected values on both devices.\n");
    }
}

// ---------------------------------------------------------------------------
// Independent software-I2C cross-check.
// In I2C mode CSAG and CSM are kept high. SCL=GP10 and SDA=GP11 are bit-banged
// as open-drain GPIO, so this test does not use the Pico hardware SPI or MISO.
// ---------------------------------------------------------------------------

void od_low(uint pin) {
    gpio_put(pin, 0);
    gpio_set_dir(pin, GPIO_OUT);
}

void od_release(uint pin) {
    gpio_set_dir(pin, GPIO_IN);
}

void i2c_delay() {
    sleep_us(I2C_HALF_US);
}

void i2c_start() {
    od_release(PIN_MOSI); // SDA
    od_release(PIN_SCK);  // SCL
    i2c_delay();
    od_low(PIN_MOSI);
    i2c_delay();
    od_low(PIN_SCK);
    i2c_delay();
}

void i2c_stop() {
    od_low(PIN_MOSI);
    i2c_delay();
    od_release(PIN_SCK);
    i2c_delay();
    od_release(PIN_MOSI);
    i2c_delay();
}

bool i2c_write_byte(uint8_t value) {
    for (int bit = 7; bit >= 0; --bit) {
        if ((value >> bit) & 1) {
            od_release(PIN_MOSI);
        } else {
            od_low(PIN_MOSI);
        }
        i2c_delay();
        od_release(PIN_SCK);
        i2c_delay();
        od_low(PIN_SCK);
        i2c_delay();
    }

    od_release(PIN_MOSI);
    i2c_delay();
    od_release(PIN_SCK);
    i2c_delay();
    const bool ack = !gpio_get(PIN_MOSI);
    od_low(PIN_SCK);
    i2c_delay();
    return ack;
}

uint8_t i2c_read_byte(bool ack) {
    uint8_t value = 0;
    od_release(PIN_MOSI);

    for (int bit = 0; bit < 8; ++bit) {
        od_release(PIN_SCK);
        i2c_delay();
        value = static_cast<uint8_t>((value << 1) | (gpio_get(PIN_MOSI) ? 1 : 0));
        od_low(PIN_SCK);
        i2c_delay();
    }

    if (ack) {
        od_low(PIN_MOSI);
    } else {
        od_release(PIN_MOSI);
    }
    i2c_delay();
    od_release(PIN_SCK);
    i2c_delay();
    od_low(PIN_SCK);
    od_release(PIN_MOSI);
    i2c_delay();
    return value;
}

bool i2c_read_register(uint8_t address7, uint8_t reg, uint8_t *value) {
    i2c_start();
    if (!i2c_write_byte(static_cast<uint8_t>(address7 << 1))) {
        i2c_stop();
        return false;
    }
    if (!i2c_write_byte(reg)) {
        i2c_stop();
        return false;
    }

    i2c_start();
    if (!i2c_write_byte(static_cast<uint8_t>((address7 << 1) | 1))) {
        i2c_stop();
        return false;
    }

    *value = i2c_read_byte(false);
    i2c_stop();
    return true;
}

void prepare_i2c_mode(bool address_high) {
    // In I2C mode both CS inputs must be high.
    gpio_init(PIN_CSAG);
    gpio_init(PIN_CSM);
    gpio_set_dir(PIN_CSAG, GPIO_OUT);
    gpio_set_dir(PIN_CSM, GPIO_OUT);
    gpio_put(PIN_CSAG, 1);
    gpio_put(PIN_CSM, 1);

    // SDOAG and SDOM are tied together on I-26. In I2C mode they are address
    // select inputs, so GP8 can safely choose both low-address or high-address.
    gpio_disable_pulls(PIN_MISO);
    gpio_set_dir(PIN_MISO, GPIO_OUT);
    gpio_put(PIN_MISO, address_high ? 1 : 0);

    // Re-use the physical SPI clock/data wiring as arbitrary open-drain I2C GPIO.
    gpio_init(PIN_SCK);   // LSM9DS1 SCL
    gpio_init(PIN_MOSI);  // LSM9DS1 SDA
    gpio_disable_pulls(PIN_SCK);
    gpio_disable_pulls(PIN_MOSI);
    od_release(PIN_SCK);
    od_release(PIN_MOSI);
    sleep_ms(20);
}

void run_i2c_case(bool address_high) {
    prepare_i2c_mode(address_high);

    const uint8_t ag_addr = address_high ? 0x6B : 0x6A;
    const uint8_t mag_addr = address_high ? 0x1E : 0x1C;
    uint8_t ag = 0xFF;
    uint8_t mag = 0xFF;

    const int scl_idle = gpio_get(PIN_SCK);
    const int sda_idle = gpio_get(PIN_MOSI);
    const bool ag_ack = i2c_read_register(ag_addr, WHO_AM_I_REG, &ag);
    const bool mag_ack = i2c_read_register(mag_addr, WHO_AM_I_REG, &mag);

    printf("#SW-I2C addr-select=%s SCL-idle=%d SDA-idle=%d ",
           address_high ? "HIGH" : "LOW", scl_idle, sda_idle);
    printf("A/G@0x%02X ack=%d id=0x%02X MAG@0x%02X ack=%d id=0x%02X\n",
           ag_addr, ag_ack ? 1 : 0, ag, mag_addr, mag_ack ? 1 : 0, mag);

    if (ag_ack && mag_ack && ag == EXPECTED_AG_ID && mag == EXPECTED_MAG_ID) {
        printf("#I2C RESULT PASS: LSM9DS1 itself, power, SCL and SDA are working.\n");
        printf("#If SPI is still FF/FF, focus on CSAG/CSM and the SDOAG+SDOM -> GP8 path.\n");
    } else if (!ag_ack && !mag_ack) {
        printf("#I2C RESULT NO-ACK: neither internal device answered on I2C.\n");
    } else {
        printf("#I2C RESULT PARTIAL/UNEXPECTED: at least one internal device behaved differently.\n");
    }
}

void run_i2c_crosscheck() {
    printf("#--- Independent software-I2C cross-check ---\n");
    printf("#This does not use hardware SPI or the MISO data path.\n");
    run_i2c_case(false);
    sleep_ms(50);
    run_i2c_case(true);
    printf("#--- Software-I2C cross-check complete ---\n");
}

}  // namespace

int main() {
    stdio_init_all();
    init_gpio_for_software_spi();

    sleep_ms(3000);

    printf("\n#==============================================\n");
    printf("# I-26 LSM9DS1 BUS DIAGNOSTIC\n");
    printf("# SPI: MISO=GP8 CSM=GP9 SCK=GP10 MOSI=GP11 CSAG=GP13\n");
    printf("# Expected WHO_AM_I: A/G=0x68 MAG=0x3D\n");
    printf("# This firmware does NOT start motors or the flight-control loop.\n");
    printf("#==============================================\n");
    printf("#Static multimeter test starts in 5 seconds. Reboot Pico to repeat it.\n");
    sleep_ms(5000);

    static_pin_test();

    printf("#--- Software-SPI WHO_AM_I test ---\n");
    for (int i = 0; i < 5; ++i) {
        const uint8_t ag = spi_read_ag_who_am_i();
        const uint8_t mag = spi_read_mag_who_am_i();

        printf("#SW-SPI WHO_AM_I A/G=0x%02X (expected 0x%02X) MAG=0x%02X (expected 0x%02X) MISO-idle=%d\n",
               ag, EXPECTED_AG_ID, mag, EXPECTED_MAG_ID, gpio_get(PIN_MISO));
        print_spi_interpretation(ag, mag);
        sleep_ms(500);
    }

    run_i2c_crosscheck();

    printf("#--- Diagnostic complete; results repeat every 3 seconds ---\n");
    while (true) {
        init_gpio_for_software_spi();
        const uint8_t ag = spi_read_ag_who_am_i();
        const uint8_t mag = spi_read_mag_who_am_i();
        printf("#REPEAT SW-SPI A/G=0x%02X MAG=0x%02X\n", ag, mag);

        run_i2c_crosscheck();
        sleep_ms(3000);
    }

    return 0;
}
