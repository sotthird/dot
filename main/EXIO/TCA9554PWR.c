#include "TCA9554PWR.h"

uint8_t Read_REG(uint8_t REG) {
    uint8_t bitsStatus = 0;
    I2C_Read(TCA9554_ADDRESS, REG, &bitsStatus, 1);
    return bitsStatus;
}

void Write_REG(uint8_t REG, uint8_t Data) {
    I2C_Write(TCA9554_ADDRESS, REG, &Data, 1);
}

void Mode_EXIO(uint8_t Pin, uint8_t State) {
    uint8_t bitsStatus = Read_REG(TCA9554_CONFIG_REG);
    uint8_t Data = (0x01 << (Pin - 1)) | bitsStatus;
    Write_REG(TCA9554_CONFIG_REG, Data);
}

void Mode_EXIOS(uint8_t PinState) {
    Write_REG(TCA9554_CONFIG_REG, PinState);
}

uint8_t Read_EXIO(uint8_t Pin) {
    uint8_t inputBits = Read_REG(TCA9554_INPUT_REG);
    return (inputBits >> (Pin - 1)) & 0x01;
}

uint8_t Read_EXIOS(void) {
    return Read_REG(TCA9554_INPUT_REG);
}

void Set_EXIO(uint8_t Pin, uint8_t State) {
    if (State < 2 && Pin < 9 && Pin > 0) {
        uint8_t bitsStatus = Read_REG(TCA9554_OUTPUT_REG);
        uint8_t Data =
            (State == 1) ? ((0x01 << (Pin - 1)) | bitsStatus) : (~(0x01 << (Pin - 1)) & bitsStatus);
        Write_REG(TCA9554_OUTPUT_REG, Data);
    } else {
        printf("Parameter error, please enter the correct parameter!\r\n");
    }
}

void Set_EXIOS(uint8_t PinState) {
    Write_REG(TCA9554_OUTPUT_REG, PinState);
}

void Set_Toggle(uint8_t Pin) {
    uint8_t bitsStatus = Read_EXIO(Pin);
    Set_EXIO(Pin, (uint8_t)!bitsStatus);
}

void TCA9554PWR_Init(uint8_t PinState) {
    Mode_EXIOS(PinState);
}

esp_err_t EXIO_Init(void) {
    TCA9554PWR_Init(0x00);
    Buzzer_Off();
    return ESP_OK;
}
