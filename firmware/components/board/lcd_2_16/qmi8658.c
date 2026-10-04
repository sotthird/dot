#include "qmi8658.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "qmi8658";

#define QMI8658_ADDR 0x6B /* SA0 high; 0x6A when low */
#define I2C_CLOCK_HZ 400000
#define I2C_TIMEOUT_MS 100

#define REG_WHO_AM_I 0x00
#define REG_CTRL1 0x02
#define REG_CTRL2 0x03 /* accelerometer range and data rate */
#define REG_CTRL5 0x06 /* low-pass filters */
#define REG_CTRL7 0x08 /* sensor enables */
#define REG_AX_L 0x35
#define REG_RST_RESULT 0x4D
#define REG_RESET 0x60

#define WHO_AM_I_VALUE 0x05
#define RESET_CMD 0xB0
#define RESET_DONE 0x80

#define CTRL1_ADDR_AUTO_INC (1 << 6)
#define CTRL2_RANGE_4G (1 << 4)
#define CTRL2_ODR_LOWPOWER_21HZ 0x0D /* low-power rates need the gyroscope off */
#define CTRL5_ACCEL_LPF_MASK 0x06
#define CTRL5_ACCEL_LPF_13_37PCT (3 << 1)
#define CTRL7_ACCEL_EN (1 << 0)

/* +/-4 g over the signed 16-bit range */
#define ACCEL_G_PER_LSB (4.0f / 32768.0f)

static i2c_master_dev_handle_t s_dev;

static esp_err_t read_regs(uint8_t reg, uint8_t* data, size_t len) {
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, I2C_TIMEOUT_MS);
}

static esp_err_t write_reg(uint8_t reg, uint8_t value) {
    const uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

static esp_err_t update_reg(uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t cur;
    ESP_RETURN_ON_ERROR(read_regs(reg, &cur, 1), TAG, "read reg 0x%02x", reg);
    return write_reg(reg, (cur & ~mask) | value);
}

esp_err_t qmi8658_init(i2c_master_bus_handle_t bus) {
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_ADDR,
        .scl_speed_hz = I2C_CLOCK_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &s_dev), TAG, "add device");

    ESP_RETURN_ON_ERROR(write_reg(REG_RESET, RESET_CMD), TAG, "no response at 0x%02x",
                        QMI8658_ADDR);
    uint8_t result = 0;
    for (int waited = 0; result != RESET_DONE && waited < 500; waited += 10) {
        vTaskDelay(pdMS_TO_TICKS(10));
        read_regs(REG_RST_RESULT, &result, 1);
    }
    ESP_RETURN_ON_FALSE(result == RESET_DONE, ESP_ERR_TIMEOUT, TAG, "reset did not finish");

    uint8_t id = 0;
    ESP_RETURN_ON_ERROR(read_regs(REG_WHO_AM_I, &id, 1), TAG, "read id");
    ESP_RETURN_ON_FALSE(id == WHO_AM_I_VALUE, ESP_ERR_NOT_FOUND, TAG, "unexpected id 0x%02x", id);

    ESP_RETURN_ON_ERROR(update_reg(REG_CTRL1, CTRL1_ADDR_AUTO_INC, CTRL1_ADDR_AUTO_INC), TAG,
                        "CTRL1");
    ESP_RETURN_ON_ERROR(write_reg(REG_CTRL2, CTRL2_RANGE_4G | CTRL2_ODR_LOWPOWER_21HZ), TAG,
                        "CTRL2");
    ESP_RETURN_ON_ERROR(update_reg(REG_CTRL5, CTRL5_ACCEL_LPF_MASK, CTRL5_ACCEL_LPF_13_37PCT), TAG,
                        "CTRL5");
    ESP_RETURN_ON_ERROR(update_reg(REG_CTRL7, CTRL7_ACCEL_EN, CTRL7_ACCEL_EN), TAG, "CTRL7");

    ESP_LOGI(TAG, "accelerometer ready (id 0x%02x)", id);
    return ESP_OK;
}

esp_err_t qmi8658_read_accel(float* ax, float* ay, float* az) {
    uint8_t raw[6];
    ESP_RETURN_ON_ERROR(read_regs(REG_AX_L, raw, sizeof(raw)), TAG, "read accel");
    *ax = (int16_t)((raw[1] << 8) | raw[0]) * ACCEL_G_PER_LSB;
    *ay = (int16_t)((raw[3] << 8) | raw[2]) * ACCEL_G_PER_LSB;
    *az = (int16_t)((raw[5] << 8) | raw[4]) * ACCEL_G_PER_LSB;
    return ESP_OK;
}
