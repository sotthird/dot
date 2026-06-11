#pragma once

#include <stdio.h>

#include "Buzzer.h"
#include "I2C_Driver.h"
#include "esp_err.h"

#define TCA9554_EXIO1 0x01
#define TCA9554_EXIO2 0x02
#define TCA9554_EXIO3 0x03
#define TCA9554_EXIO4 0x04
#define TCA9554_EXIO5 0x05
#define TCA9554_EXIO6 0x06
#define TCA9554_EXIO7 0x07
#define TCA9554_EXIO8 0x08

#define TCA9554_ADDRESS 0x20
#define TCA9554_INPUT_REG 0x00
#define TCA9554_OUTPUT_REG 0x01
#define TCA9554_Polarity_REG 0x02
#define TCA9554_CONFIG_REG 0x03

uint8_t Read_REG(uint8_t REG);
void Write_REG(uint8_t REG, uint8_t Data);
void Mode_EXIO(uint8_t Pin, uint8_t State);

void Mode_EXIOS(uint8_t PinState);
uint8_t Read_EXIO(uint8_t Pin);
uint8_t Read_EXIOS(void);

void Set_EXIO(uint8_t Pin, uint8_t State);
void Set_EXIOS(uint8_t PinState);

void Set_Toggle(uint8_t Pin);
void TCA9554PWR_Init(uint8_t PinState);

esp_err_t EXIO_Init(void);
