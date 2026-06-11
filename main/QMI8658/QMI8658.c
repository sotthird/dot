#include "QMI8658.h"

IMUdata Accel;
IMUdata Gyro;

uint8_t Device_addr;
acc_scale_t acc_scale = ACC_RANGE_4G;
gyro_scale_t gyro_scale = GYR_RANGE_64DPS;
acc_odr_t acc_odr = acc_odr_norm_8000;
gyro_odr_t gyro_odr = gyro_odr_norm_8000;
sensor_state_t sensor_state = sensor_default;
lpf_t acc_lpf;

float accelScales, gyroScales;
float accelScales = 0;
uint8_t readings[12];
uint32_t reading_timestamp_us;

void QMI8658_Init(void) {
    uint8_t buf[1];
    Device_addr = QMI8658_L_SLAVE_ADDRESS;
    I2C_Read(Device_addr, QMI8658_REVISION_ID, buf, 1);
    printf("QMI8658 Device ID: %x\r\n", buf[0]);
    setState(sensor_running);

    setAccScale(acc_scale);
    setAccODR(acc_odr);
    setAccLPF(LPF_MODE_0);
    switch (acc_scale) {
        case ACC_RANGE_2G:
            accelScales = 2.0 / 32768.0;
            break;
        case ACC_RANGE_4G:
            accelScales = 4.0 / 32768.0;
            break;
        case ACC_RANGE_8G:
            accelScales = 8.0 / 32768.0;
            break;
        case ACC_RANGE_16G:
            accelScales = 16.0 / 32768.0;
            break;
    }

    setGyroScale(gyro_scale);
    setGyroODR(gyro_odr);
    setGyroLPF(LPF_MODE_3);
    switch (gyro_scale) {
        case GYR_RANGE_16DPS:
            gyroScales = 16.0 / 32768.0;
            break;
        case GYR_RANGE_32DPS:
            gyroScales = 32.0 / 32768.0;
            break;
        case GYR_RANGE_64DPS:
            gyroScales = 64.0 / 32768.0;
            break;
        case GYR_RANGE_128DPS:
            gyroScales = 128.0 / 32768.0;
            break;
        case GYR_RANGE_256DPS:
            gyroScales = 256.0 / 32768.0;
            break;
        case GYR_RANGE_512DPS:
            gyroScales = 512.0 / 32768.0;
            break;
        case GYR_RANGE_1024DPS:
            gyroScales = 1024.0 / 32768.0;
            break;
    }
}
void QMI8658_Loop(void) {
    getAccelerometer();
}

void QMI8658_transmit(uint8_t addr, uint8_t data) {
    I2C_Write(Device_addr, addr, &data, 1);
}

uint8_t QMI8658_receive(uint8_t addr) {
    uint8_t retval;
    I2C_Read(Device_addr, addr, &retval, 1);
    return retval;
}

void QMI8658_CTRL9_Write(uint8_t command) {
    QMI8658_transmit(QMI8658_CTRL9, command);

    while (((QMI8658_receive(QMI8658_STATUSINT)) & 0x80) == 0x00);
}

void setAccODR(acc_odr_t odr) {
    if (sensor_state != sensor_default) {
        uint8_t ctrl2 = QMI8658_receive(QMI8658_CTRL2);
        ctrl2 &= ~QMI8658_AODR_MASK;
        ctrl2 |= odr;
        QMI8658_transmit(QMI8658_CTRL2, ctrl2);
    }
    acc_odr = odr;
}

void setGyroODR(gyro_odr_t odr) {
    if (sensor_state != sensor_default) {
        uint8_t ctrl3 = QMI8658_receive(QMI8658_CTRL3);
        ctrl3 &= ~QMI8658_GODR_MASK;
        ctrl3 |= odr;
        QMI8658_transmit(QMI8658_CTRL3, ctrl3);
    }
    gyro_odr = odr;
}

void setAccScale(acc_scale_t scale) {
    if (sensor_state != sensor_default) {
        uint8_t ctrl2 = QMI8658_receive(QMI8658_CTRL2);
        ctrl2 &= ~QMI8658_ASCALE_MASK;
        ctrl2 |= scale << QMI8658_ASCALE_OFFSET;
        QMI8658_transmit(QMI8658_CTRL2, ctrl2);
    }
    acc_scale = scale;
}

void setGyroScale(gyro_scale_t scale) {
    if (sensor_state != sensor_default) {
        uint8_t ctrl3 = QMI8658_receive(QMI8658_CTRL3);
        ctrl3 &= ~QMI8658_GSCALE_MASK;
        ctrl3 |= scale << QMI8658_GSCALE_OFFSET;
        QMI8658_transmit(QMI8658_CTRL3, ctrl3);
    }
    gyro_scale = scale;
}

void setAccLPF(lpf_t lpf) {
    if (sensor_state != sensor_default) {
        uint8_t ctrl5 = QMI8658_receive(QMI8658_CTRL5);
        ctrl5 &= !QMI8658_ALPF_MASK;
        ctrl5 |= lpf << QMI8658_ALPF_OFFSET;
        ctrl5 |= 0x01;
        QMI8658_transmit(QMI8658_CTRL5, ctrl5);
    }
    acc_lpf = lpf;
}

void setGyroLPF(lpf_t lpf) {
    if (sensor_state != sensor_default) {
        uint8_t ctrl5 = QMI8658_receive(QMI8658_CTRL5);
        ctrl5 &= !QMI8658_GLPF_MASK;
        ctrl5 |= lpf << QMI8658_GLPF_OFFSET;
        ctrl5 |= 0x10;
        QMI8658_transmit(QMI8658_CTRL5, ctrl5);
    }
}

void setState(sensor_state_t state) {
    uint8_t ctrl1;
    switch (state) {
        case sensor_running:
            ctrl1 = QMI8658_receive(QMI8658_CTRL1);

            ctrl1 &= 0xFE;

            ctrl1 |= 0x40;
            QMI8658_transmit(QMI8658_CTRL1, ctrl1);

            QMI8658_transmit(QMI8658_CTRL7, 0x43);

            QMI8658_transmit(QMI8658_CTRL6, 0x00);
            break;
        case sensor_power_down:

            QMI8658_transmit(QMI8658_CTRL7, 0x00);

            ctrl1 = QMI8658_receive(QMI8658_CTRL1);

            ctrl1 |= 0x01;
            QMI8658_transmit(QMI8658_CTRL1, ctrl1);
            break;
        case sensor_locking:
            ctrl1 = QMI8658_receive(QMI8658_CTRL1);

            ctrl1 &= 0xFE;

            ctrl1 |= 0x40;
            QMI8658_transmit(QMI8658_CTRL1, ctrl1);

            QMI8658_transmit(QMI8658_CTRL7, 0x83);

            QMI8658_transmit(QMI8658_CTRL6, 0x00);

            QMI8658_transmit(QMI8658_CAL1_L, 0x01);
            QMI8658_CTRL9_Write(0x12);

            QMI8658_transmit(QMI8658_CAL1_L, 0x00);
            QMI8658_CTRL9_Write(0x12);
            break;
        default:
            break;
    }
    sensor_state = state;
}

void getAccelerometer(void) {
    uint8_t buf[6];
    I2C_Read(Device_addr, QMI8658_AX_L, buf, 6);
    Accel.x = (float)((int16_t)((buf[1] << 8) | (buf[0])));
    Accel.y = (float)((int16_t)((buf[3] << 8) | (buf[2])));
    Accel.z = (float)((int16_t)((buf[5] << 8) | (buf[4])));
    Accel.x = Accel.x * accelScales;
    Accel.y = Accel.y * accelScales;
    Accel.z = Accel.z * accelScales;
}
void getGyroscope(void) {
    uint8_t buf[6];
    I2C_Read(Device_addr, QMI8658_GX_L, buf, 6);
    Gyro.x = (float)((int16_t)((buf[1] << 8) | (buf[0])));
    Gyro.y = (float)((int16_t)((buf[3] << 8) | (buf[2])));
    Gyro.z = (float)((int16_t)((buf[5] << 8) | (buf[4])));
    Gyro.x = Gyro.x * gyroScales;
    Gyro.y = Gyro.y * gyroScales;
    Gyro.z = Gyro.z * gyroScales;
}
