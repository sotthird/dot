
#pragma once

#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "EXIO/TCA9554PWR.h"
#include "driver/sdmmc_host.h"
#include "esp_flash.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define CONFIG_EXAMPLE_PIN_CLK 2
#define CONFIG_EXAMPLE_PIN_CMD 1
#define CONFIG_EXAMPLE_PIN_D0 42
#define CONFIG_EXAMPLE_PIN_D1 -1
#define CONFIG_EXAMPLE_PIN_D2 -1
#define CONFIG_EXAMPLE_PIN_D3 -1

esp_err_t SD_Card_D3_EN(void);
esp_err_t SD_Card_D3_Dis(void);

esp_err_t s_example_write_file(const char* path, char* data);
esp_err_t s_example_read_file(const char* path);

extern uint32_t SDCard_Size;
extern uint32_t Flash_Size;
void SD_Init(void);
void Flash_Searching(void);
