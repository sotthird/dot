#include "SD_MMC.h"

#define EXAMPLE_MAX_CHAR_SIZE 64
#define MOUNT_POINT "/sdcard"

static const char* SD_TAG = "SD";

uint32_t Flash_Size = 0;
uint32_t SDCard_Size = 0;
esp_err_t SD_Card_D3_EN(void) {
    Set_EXIO(TCA9554_EXIO4, true);
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}
esp_err_t SD_Card_D3_Dis(void) {
    Set_EXIO(TCA9554_EXIO4, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

esp_err_t s_example_write_file(const char* path, char* data) {
    ESP_LOGI(SD_TAG, "Opening file %s", path);
    FILE* f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(SD_TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(SD_TAG, "File written");

    return ESP_OK;
}

esp_err_t s_example_read_file(const char* path) {
    ESP_LOGI(SD_TAG, "Reading file %s", path);
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(SD_TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[EXAMPLE_MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    char* pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(SD_TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

void SD_Init(void) {
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true, .max_files = 5, .allocation_unit_size = 16 * 1024};
    sdmmc_card_t* card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(SD_TAG, "Initializing SD card");

    ESP_LOGI(SD_TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    SD_Card_D3_EN();

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;

    slot_config.clk = CONFIG_EXAMPLE_PIN_CLK;
    slot_config.cmd = CONFIG_EXAMPLE_PIN_CMD;
    slot_config.d0 = CONFIG_EXAMPLE_PIN_D0;
    slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;
    slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;
    slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;

    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(SD_TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(SD_TAG,
                     "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the "
                     "CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(SD_TAG,
                     "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(SD_TAG, "Filesystem mounted");

    sdmmc_card_print_info(stdout, card);
    SDCard_Size = ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024);
}
void Flash_Searching(void) {
    if (esp_flash_get_physical_size(NULL, &Flash_Size) == ESP_OK) {
        Flash_Size = Flash_Size / (uint32_t)(1024 * 1024);
        printf("Flash size: %ld MB\n", Flash_Size);
    } else {
        printf("Get flash size failed\n");
    }
}
