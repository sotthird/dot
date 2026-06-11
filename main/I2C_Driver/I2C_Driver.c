#include "I2C_Driver.h"

static const char* I2C_TAG = "I2C";
i2c_master_bus_handle_t i2c_bus_handle = NULL;

static esp_err_t i2c_master_init(void) {
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_Touch_SCL_IO,
        .sda_io_num = I2C_Touch_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle);
}

void I2C_Init(void) {
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(I2C_TAG, "I2C initialized successfully");
}

esp_err_t I2C_Write(uint8_t Driver_addr, uint8_t Reg_addr, const uint8_t* Reg_data,
                    uint32_t Length) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = Driver_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev_handle;
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK)
        return ret;

    uint8_t buf[Length + 1];
    buf[0] = Reg_addr;
    memcpy(&buf[1], Reg_data, Length);
    ret = i2c_master_transmit(dev_handle, buf, Length + 1, I2C_MASTER_TIMEOUT_MS);
    i2c_master_bus_rm_device(dev_handle);
    return ret;
}

esp_err_t I2C_Read(uint8_t Driver_addr, uint8_t Reg_addr, uint8_t* Reg_data, uint32_t Length) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = Driver_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev_handle;
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK)
        return ret;

    ret = i2c_master_transmit_receive(dev_handle, &Reg_addr, 1, Reg_data, Length,
                                      I2C_MASTER_TIMEOUT_MS);
    i2c_master_bus_rm_device(dev_handle);
    return ret;
}
