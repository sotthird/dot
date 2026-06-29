#include "CST820.h"

#define POINT_NUM_MAX (1)

#define DATA_START_REG (0x15)
#define CHIP_ID_REG (0x01)
#define TOUCH_NUM (0x02)
#define TOUCH_POSITION (0x03)

static const char* TAG = "cst820";

static esp_err_t read_data(esp_lcd_touch_handle_t tp);
static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t* x, uint16_t* y, uint16_t* strength,
                   uint8_t* point_num, uint8_t max_point_num);
static esp_err_t del(esp_lcd_touch_handle_t tp);

static esp_err_t i2c_read_bytes(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t* data,
                                uint8_t len);

static esp_err_t reset(esp_lcd_touch_handle_t tp);
static esp_err_t read_id(esp_lcd_touch_handle_t tp);

esp_err_t esp_lcd_touch_new_i2c_cst820(const esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_touch_config_t* config,
                                       esp_lcd_touch_handle_t* tp) {
    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_ARG, TAG, "Invalid io");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid config");
    ESP_RETURN_ON_FALSE(tp, ESP_ERR_INVALID_ARG, TAG, "Invalid touch handle");

    esp_err_t ret = ESP_OK;
    esp_lcd_touch_handle_t cst820 = calloc(1, sizeof(esp_lcd_touch_t));
    ESP_GOTO_ON_FALSE(cst820, ESP_ERR_NO_MEM, err, TAG, "Touch handle malloc failed");

    cst820->io = io;

    cst820->read_data = read_data;
    cst820->get_xy = get_xy;
    cst820->del = del;

    cst820->data.lock.owner = portMUX_FREE_VAL;

    memcpy(&cst820->config, config, sizeof(esp_lcd_touch_config_t));

    if (cst820->config.int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {.mode = GPIO_MODE_INPUT,
                                               .intr_type = GPIO_INTR_NEGEDGE,
                                               .pin_bit_mask = BIT64(cst820->config.int_gpio_num)};
        ESP_GOTO_ON_ERROR(gpio_config(&int_gpio_config), err, TAG, "GPIO intr config failed");

        if (cst820->config.interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(cst820, cst820->config.interrupt_callback);
        }
    }

    if (cst820->config.rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t rst_gpio_config = {.mode = GPIO_MODE_OUTPUT,
                                               .pin_bit_mask = BIT64(cst820->config.rst_gpio_num)};
        ESP_GOTO_ON_ERROR(gpio_config(&rst_gpio_config), err, TAG, "GPIO reset config failed");
    }

    ESP_GOTO_ON_ERROR(reset(cst820), err, TAG, "Reset failed");

    ESP_GOTO_ON_ERROR(read_id(cst820), err, TAG, "Read version failed");

    /* Disable auto-sleep (reg 0xFE on the Hynitron CST8xx family). When the
     * controller naps it stops ACKing reads — with our unconditional polling
     * that produced continuous "i2c transaction failed" errors and delayed the
     * next touch. Best-effort: parts without this register just ignore it. */
    uint8_t dis_auto_sleep = 0x01;
    esp_lcd_panel_io_tx_param(cst820->io, 0xFE, &dis_auto_sleep, 1);

    *tp = cst820;

    return ESP_OK;
err:
    if (cst820) {
        del(cst820);
    }
    ESP_LOGE(TAG, "Initialization failed!");
    return ret;
}

static esp_err_t read_data(esp_lcd_touch_handle_t tp) {
    esp_err_t err;
    uint8_t buf[41];
    uint8_t touch_cnt = 0;
    size_t i = 0;

    assert(tp != NULL);

    /* Poll the touch registers unconditionally. This driver is purely polled
     * (no INT callback is registered), and on this CST820 the INT line only
     * pulses low per report rather than staying low for the whole contact —
     * gating the I2C read on INT level dropped most samples, so quick taps were
     * missed and a long press was needed to land a poll inside an INT pulse. */
    err = i2c_read_bytes(tp, TOUCH_NUM, buf, 1);
    if (err != ESP_OK) {
        return ESP_OK;
    }

    if ((buf[0] & 0x0F) != 0x00) {
        touch_cnt = buf[0] & 0x0F;
        if (touch_cnt > 2 || touch_cnt == 0) {
            return ESP_OK;
        }

        err = i2c_read_bytes(tp, TOUCH_POSITION, &buf[0], touch_cnt * 6);
        if (err != ESP_OK) {
            return ESP_OK;
        }

        taskENTER_CRITICAL(&tp->data.lock);

        if (touch_cnt > CONFIG_ESP_LCD_TOUCH_MAX_POINTS)
            touch_cnt = CONFIG_ESP_LCD_TOUCH_MAX_POINTS;
        tp->data.points = (uint8_t)touch_cnt;

        for (i = 0; i < touch_cnt; i++) {
            tp->data.coords[i].x =
                (uint16_t)(((uint16_t)(buf[(i * 6)] & 0x0F) << 8) + (buf[(i * 6) + 1]));
            tp->data.coords[i].y =
                (uint16_t)(((uint16_t)(buf[(i * 6) + 2] & 0x0F) << 8) + (buf[(i * 6) + 3]));
            tp->data.coords[i].strength = 50;
        }

        taskEXIT_CRITICAL(&tp->data.lock);
    }

    return ESP_OK;
}

static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t* x, uint16_t* y, uint16_t* strength,
                   uint8_t* point_num, uint8_t max_point_num) {
    portENTER_CRITICAL(&tp->data.lock);

    *point_num = (tp->data.points > max_point_num ? max_point_num : tp->data.points);
    for (size_t i = 0; i < *point_num; i++) {
        x[i] = tp->data.coords[i].x;
        y[i] = tp->data.coords[i].y;

        if (strength) {
            strength[i] = tp->data.coords[i].strength;
        }
    }

    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);

    return (*point_num > 0);
}

static esp_err_t del(esp_lcd_touch_handle_t tp) {
    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
    }
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.rst_gpio_num);
    }

    free(tp);

    return ESP_OK;
}

static esp_err_t reset(esp_lcd_touch_handle_t tp) {
    Set_EXIO(TCA9554_EXIO2, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    Set_EXIO(TCA9554_EXIO2, true);
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

static esp_err_t read_id(esp_lcd_touch_handle_t tp) {
    uint8_t id = 0x10;

    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, CHIP_ID_REG, &id, 1), TAG, "I2C read failed");
    ESP_LOGI(TAG, "IC id: %d", id);
    return ESP_OK;
}

static esp_err_t i2c_read_bytes(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t* data,
                                uint8_t len) {
    assert(tp != NULL);
    assert(data != NULL);

    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
}

esp_lcd_touch_handle_t tp = NULL;
void Touch_Init(void) {
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST820_CONFIG();
    ESP_LOGI(TAG, "Initialize touch IO (I2C)");

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &tp_io_config, &tp_io_handle));
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_V_RES,
        .y_max = EXAMPLE_LCD_H_RES,
        .rst_gpio_num = I2C_Touch_RST_IO,
        .int_gpio_num = I2C_Touch_INT_IO,
        .flags =
            {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
    };

    ESP_LOGI(TAG, "Initialize touch controller CST820");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst820(tp_io_handle, &tp_cfg, &tp));
}
