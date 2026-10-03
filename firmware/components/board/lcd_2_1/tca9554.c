#include "tca9554.h"

#include "esp_check.h"

#define TCA9554_ADDRESS 0x20
#define TCA9554_OUTPUT_REG 0x01
#define TCA9554_CONFIG_REG 0x03
#define TIMEOUT_MS 1000

static const char* TAG = "tca9554";

static i2c_master_dev_handle_t dev;
static uint8_t output;  // shadow of the output register

static esp_err_t write_reg(uint8_t reg, uint8_t value) {
    const uint8_t buf[] = {reg, value};
    return i2c_master_transmit(dev, buf, sizeof(buf), TIMEOUT_MS);
}

esp_err_t tca9554_init(i2c_master_bus_handle_t bus) {
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TCA9554_ADDRESS,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &dev), TAG, "add device failed");

    const uint8_t reg = TCA9554_OUTPUT_REG;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(dev, &reg, 1, &output, 1, TIMEOUT_MS), TAG,
                        "read outputs failed");
    return write_reg(TCA9554_CONFIG_REG, 0x00);
}

esp_err_t tca9554_set(uint8_t pin, bool level) {
    ESP_RETURN_ON_FALSE(pin >= 1 && pin <= 8, ESP_ERR_INVALID_ARG, TAG, "pin must be 1-8");

    const uint8_t mask = 1u << (pin - 1);
    output = level ? (output | mask) : (output & ~mask);
    return write_reg(TCA9554_OUTPUT_REG, output);
}
