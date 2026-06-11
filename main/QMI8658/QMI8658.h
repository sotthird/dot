#pragma once

#include "I2C_Driver.h"

#define QMI8658_L_SLAVE_ADDRESS (0x6B)
#define QMI8658_H_SLAVE_ADDRESS (0x6A)

#define QMI8658_WHO_AM_I 0x00
#define QMI8658_REVISION_ID 0x01
#define QMI8658_CTRL1 0x02
#define QMI8658_CTRL2 0x03
#define QMI8658_CTRL3 0x04
#define QMI8658_CTRL4 0x05
#define QMI8658_CTRL5 0x06
#define QMI8658_CTRL6 0x07
#define QMI8658_CTRL7 0x08
#define QMI8658_CTRL8 0x09
#define QMI8658_CTRL9 0x0A

#define QMI8658_CAL1_L 0x0B
#define QMI8658_CAL1_H 0x0C
#define QMI8658_CAL2_L 0x0D
#define QMI8658_CAL2_H 0x0E
#define QMI8658_CAL3_L 0x0F
#define QMI8658_CAL3_H 0x10
#define QMI8658_CAL4_L 0x11
#define QMI8658_CAL4_H 0x12

#define QMI8658_TEMP_L 0x33
#define QMI8658_TEMP_H 0x34

#define QMI8658_STATUSINT 0x2D

#define QMI8658_AX_L 0x35
#define QMI8658_AX_H 0x36
#define QMI8658_AY_L 0x37
#define QMI8658_AY_H 0x38
#define QMI8658_AZ_L 0x39
#define QMI8658_AZ_H 0x3A
#define QMI8658_GX_L 0x3B
#define QMI8658_GX_H 0x3C
#define QMI8658_GY_L 0x3D
#define QMI8658_GY_H 0x3E
#define QMI8658_GZ_L 0x3F
#define QMI8658_GZ_H 0x40

#define QMI8658_AODR_MASK 0x0F
#define QMI8658_GODR_MASK 0x0F
#define QMI8658_ASCALE_MASK 0x70
#define QMI8658_GSCALE_MASK 0x70
#define QMI8658_ALPF_MASK 0x06
#define QMI8658_GLPF_MASK 0x60
#define QMI8658_ASCALE_OFFSET 4
#define QMI8658_GSCALE_OFFSET 4
#define QMI8658_ALPF_OFFSET 1
#define QMI8658_GLPF_OFFSET 5

#define QMI8658_COMM_TIMEOUT 50

#define QMI8658_REFRESH_DELAY 2000

#define QMI8658_CTRL_CMD_AHB_CLOCK_GATING 0x12

typedef enum {
    acc_odr_norm_8000 = 0x0,
    acc_odr_norm_4000,
    acc_odr_norm_2000,
    acc_odr_norm_1000,
    acc_odr_norm_500,
    acc_odr_norm_250,
    acc_odr_norm_120,
    acc_odr_norm_60,
    acc_odr_norm_30,
    acc_odr_lp_128 = 0xC,
    acc_odr_lp_21,
    acc_odr_lp_11,
    acc_odr_lp_3,
} acc_odr_t;

typedef enum {
    gyro_odr_norm_8000 = 0x0,
    gyro_odr_norm_4000,
    gyro_odr_norm_2000,
    gyro_odr_norm_1000,
    gyro_odr_norm_500,
    gyro_odr_norm_250,
    gyro_odr_norm_120,
    gyro_odr_norm_60,
    gyro_odr_norm_30
} gyro_odr_t;

typedef enum { ACC_RANGE_2G = 0x0, ACC_RANGE_4G, ACC_RANGE_8G, ACC_RANGE_16G } acc_scale_t;

typedef enum {
    GYR_RANGE_16DPS = 0x0,
    GYR_RANGE_32DPS,
    GYR_RANGE_64DPS,
    GYR_RANGE_128DPS,
    GYR_RANGE_256DPS,
    GYR_RANGE_512DPS,
    GYR_RANGE_1024DPS
} gyro_scale_t;

typedef enum { LPF_MODE_0 = 0x0, LPF_MODE_1 = 0x2, LPF_MODE_2 = 0x4, LPF_MODE_3 = 0x6 } lpf_t;

typedef enum { sensor_default, sensor_power_down, sensor_running, sensor_locking } sensor_state_t;

typedef struct __IMUdata {
    float x;
    float y;
    float z;
} IMUdata;

extern IMUdata Accel;
extern IMUdata Gyro;

void QMI8658_Init(void);
void QMI8658_Loop(void);
void QMI8658_transmit(uint8_t addr, uint8_t data);
uint8_t QMI8658_receive(uint8_t addr);
void QMI8658_CTRL9_Write(uint8_t command);
void QMI8658_sensor_update();
void QMI8658_update_if_needed();
void setAccODR(acc_odr_t odr);
void setGyroODR(gyro_odr_t odr);
void setAccScale(acc_scale_t scale);
void setGyroScale(gyro_scale_t scale);
void setAccLPF(lpf_t lpf);
void setGyroLPF(lpf_t lpf);
void setState(sensor_state_t state);
void getRawReadings(int16_t* buf);
float getAccX();
float getAccY();
float getAccZ();
float getGyroX();
float getGyroY();
float getGyroZ();
void getAccelerometer(void);
void getGyroscope(void);
