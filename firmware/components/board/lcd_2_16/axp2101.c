#include "axp2101.h"

#include "esp_check.h"

static const char* TAG = "axp2101";

#define AXP2101_ADDR 0x34
#define I2C_CLOCK_HZ 400000
#define I2C_TIMEOUT_MS 100

#define REG_STATUS1 0x00 /* bit 3: battery connected, bit 5: USB power good */
#define REG_STATUS2 0x01 /* bits 7:5: charger state */
#define REG_ADC_CHANNEL_CTRL 0x30
#define REG_INT_ENABLE2 0x41
#define REG_INT_STATUS2 0x49 /* bit 3: PWR key short press; write 1 to clear */
#define REG_BAT_DET_CTRL 0x68
#define REG_BAT_PERCENT 0xA4

#define STATUS1_BATTERY_PRESENT (1 << 3)
#define STATUS1_VBUS_GOOD (1 << 5)
#define STATUS2_CHARGER_SHIFT 5
#define CHARGER_CHARGING 0x01
#define ADC_BATTERY_VOLTAGE (1 << 0)
#define BAT_DETECT_EN (1 << 0)
#define INT2_PKEY_SHORT (1 << 3)

static i2c_master_dev_handle_t s_dev;

static esp_err_t read_reg(uint8_t reg, uint8_t* value) {
    return i2c_master_transmit_receive(s_dev, &reg, 1, value, 1, I2C_TIMEOUT_MS);
}

static esp_err_t write_reg(uint8_t reg, uint8_t value) {
    const uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

static esp_err_t set_bits(uint8_t reg, uint8_t bits) {
    uint8_t cur;
    ESP_RETURN_ON_ERROR(read_reg(reg, &cur), TAG, "read reg 0x%02x", reg);
    const uint8_t buf[2] = {reg, cur | bits};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

esp_err_t axp2101_init(i2c_master_bus_handle_t bus) {
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_ADDR,
        .scl_speed_hz = I2C_CLOCK_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &s_dev), TAG, "add device");

    uint8_t status;
    ESP_RETURN_ON_ERROR(read_reg(REG_STATUS1, &status), TAG, "not responding");
    ESP_RETURN_ON_ERROR(set_bits(REG_BAT_DET_CTRL, BAT_DETECT_EN), TAG, "enable detection");
    ESP_RETURN_ON_ERROR(set_bits(REG_INT_ENABLE2, INT2_PKEY_SHORT), TAG, "enable power key");
    ESP_RETURN_ON_ERROR(write_reg(REG_INT_STATUS2, INT2_PKEY_SHORT), TAG, "clear power key");
    return set_bits(REG_ADC_CHANNEL_CTRL, ADC_BATTERY_VOLTAGE);
}

esp_err_t axp2101_read(int* percent, bool* charging, bool* usb_power) {
    uint8_t status1, status2, pct;
    ESP_RETURN_ON_ERROR(read_reg(REG_STATUS1, &status1), TAG, "status1");
    ESP_RETURN_ON_ERROR(read_reg(REG_STATUS2, &status2), TAG, "status2");

    *usb_power = status1 & STATUS1_VBUS_GOOD;
    *charging = (status2 >> STATUS2_CHARGER_SHIFT) == CHARGER_CHARGING;
    if (!(status1 & STATUS1_BATTERY_PRESENT)) {
        *percent = -1;
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(read_reg(REG_BAT_PERCENT, &pct), TAG, "percent");
    *percent = pct > 100 ? 100 : pct;
    return ESP_OK;
}

esp_err_t axp2101_power_key_pressed(bool* pressed) {
    uint8_t status;
    ESP_RETURN_ON_ERROR(read_reg(REG_INT_STATUS2, &status), TAG, "int status");
    *pressed = status & INT2_PKEY_SHORT;
    return *pressed ? write_reg(REG_INT_STATUS2, INT2_PKEY_SHORT) : ESP_OK;
}
