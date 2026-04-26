#include "bme280.h"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int readReg8(int fd, uint8_t reg) {
  if (write(fd, &reg, 1) != 1)
    return -1;

  uint8_t value = 0;
  if (read(fd, &value, 1) != 1)
    return -1;

  return value;
}

static int readReg16LE(int fd, uint8_t reg) {
  int lo = readReg8(fd, reg);
  int hi = readReg8(fd, reg + 1);

  if (lo < 0 || hi < 0)
    return -1;

  return (hi << 8) | lo;
}

static int writeReg8(int fd, uint8_t reg, uint8_t value) {
  uint8_t buffer[2] = {reg, value};
  return write(fd, buffer, 2) == 2 ? 0 : -1;
}

int bme280Open(void) {
  int fd = open(BME280_I2C_DEVICE, O_RDWR);

  if (fd < 0)
    return -1;

  if (ioctl(fd, I2C_SLAVE, BME280_ADDRESS) < 0) {
    close(fd);
    return -1;
  }

  return fd;
}

void bme280Close(int fd) {
  if (fd >= 0)
    close(fd);
}

int bme280ReadChipId(int fd) {
  return readReg8(fd, BME280_REGISTER_CHIPID);
}

int bme280Configure(int fd) {
  if (writeReg8(fd, BME280_REGISTER_CONTROLHUMID, 0x01) < 0)
    return -1;

  if (writeReg8(fd, BME280_REGISTER_CONTROL, 0x25) < 0)
    return -1;

  return 0;
}

int32_t getTemperatureCalibration(bme280_calib_data *cal, int32_t adc_T) {
  int32_t var1 = ((((adc_T >> 3) - ((int32_t)cal->dig_T1 << 1))) *
                  ((int32_t)cal->dig_T2)) >>
                 11;

  int32_t var2 = (((((adc_T >> 4) - ((int32_t)cal->dig_T1)) *
                    ((adc_T >> 4) - ((int32_t)cal->dig_T1))) >>
                   12) *
                  ((int32_t)cal->dig_T3)) >>
                 14;

  return var1 + var2;
}

void readCalibrationData(int fd, bme280_calib_data *data) {
  data->dig_T1 = (uint16_t)readReg16LE(fd, BME280_REGISTER_DIG_T1);
  data->dig_T2 = (int16_t)readReg16LE(fd, BME280_REGISTER_DIG_T2);
  data->dig_T3 = (int16_t)readReg16LE(fd, BME280_REGISTER_DIG_T3);

  data->dig_P1 = (uint16_t)readReg16LE(fd, BME280_REGISTER_DIG_P1);
  data->dig_P2 = (int16_t)readReg16LE(fd, BME280_REGISTER_DIG_P2);
  data->dig_P3 = (int16_t)readReg16LE(fd, BME280_REGISTER_DIG_P3);
  data->dig_P4 = (int16_t)readReg16LE(fd, BME280_REGISTER_DIG_P4);
  data->dig_P5 = (int16_t)readReg16LE(fd, BME280_REGISTER_DIG_P5);
  data->dig_P6 = (int16_t)readReg16LE(fd, BME280_REGISTER_DIG_P6);
  data->dig_P7 = (int16_t)readReg16LE(fd, BME280_REGISTER_DIG_P7);
  data->dig_P8 = (int16_t)readReg16LE(fd, BME280_REGISTER_DIG_P8);
  data->dig_P9 = (int16_t)readReg16LE(fd, BME280_REGISTER_DIG_P9);

  data->dig_H1 = (uint8_t)readReg8(fd, BME280_REGISTER_DIG_H1);
  data->dig_H2 = (int16_t)readReg16LE(fd, BME280_REGISTER_DIG_H2);
  data->dig_H3 = (uint8_t)readReg8(fd, BME280_REGISTER_DIG_H3);

  int h4_msb = readReg8(fd, BME280_REGISTER_DIG_H4);
  int h4_h5 = readReg8(fd, BME280_REGISTER_DIG_H4 + 1);
  int h5_msb = readReg8(fd, BME280_REGISTER_DIG_H5 + 1);

  data->dig_H4 = (int16_t)((h4_msb << 4) | (h4_h5 & 0x0F));
  data->dig_H5 = (int16_t)((h5_msb << 4) | (h4_h5 >> 4));

  if (data->dig_H4 & 0x0800)
    data->dig_H4 |= 0xF000;

  if (data->dig_H5 & 0x0800)
    data->dig_H5 |= 0xF000;

  data->dig_H6 = (int8_t)readReg8(fd, BME280_REGISTER_DIG_H6);
}

float compensateTemperature(int32_t t_fine) {
  float T = (t_fine * 5 + 128) >> 8;
  return T / 100;
}

float compensatePressure(int32_t adc_P, bme280_calib_data *cal,
                         int32_t t_fine) {
  int64_t var1, var2, p;

  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)cal->dig_P6;
  var2 = var2 + ((var1 * (int64_t)cal->dig_P5) << 17);
  var2 = var2 + (((int64_t)cal->dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)cal->dig_P3) >> 8) +
         ((var1 * (int64_t)cal->dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)cal->dig_P1) >> 33;

  if (var1 == 0)
    return 0;

  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)cal->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)cal->dig_P8) * p) >> 19;

  p = ((p + var1 + var2) >> 8) + (((int64_t)cal->dig_P7) << 4);
  return (float)p / 256;
}

float compensateHumidity(int32_t adc_H, bme280_calib_data *cal,
                         int32_t t_fine) {
  int32_t v_x1_u32r;

  v_x1_u32r = t_fine - ((int32_t)76800);

  v_x1_u32r =
      (((((adc_H << 14) - (((int32_t)cal->dig_H4) << 20) -
          (((int32_t)cal->dig_H5) * v_x1_u32r)) +
         ((int32_t)16384)) >>
        15) *
       (((((((v_x1_u32r * ((int32_t)cal->dig_H6)) >> 10) *
            (((v_x1_u32r * ((int32_t)cal->dig_H3)) >> 11) +
             ((int32_t)32768))) >>
           10) +
          ((int32_t)2097152)) *
             ((int32_t)cal->dig_H2) +
         8192) >>
        14));

  v_x1_u32r =
      v_x1_u32r -
      (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
        ((int32_t)cal->dig_H1)) >>
       4);

  v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
  v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;

  float h = v_x1_u32r >> 12;
  return h / 1024.0;
}

void getRawData(int fd, bme280_raw_data *raw) {
  uint8_t reg = BME280_REGISTER_PRESSUREDATA;

  if (write(fd, &reg, 1) != 1)
    return;

  uint8_t buffer[8] = {0};

  if (read(fd, buffer, 8) != 8)
    return;

  raw->pmsb = buffer[0];
  raw->plsb = buffer[1];
  raw->pxsb = buffer[2];

  raw->tmsb = buffer[3];
  raw->tlsb = buffer[4];
  raw->txsb = buffer[5];

  raw->hmsb = buffer[6];
  raw->hlsb = buffer[7];

  raw->pressure = ((uint32_t)raw->pmsb << 12) |
                  ((uint32_t)raw->plsb << 4) |
                  ((uint32_t)raw->pxsb >> 4);

  raw->temperature = ((uint32_t)raw->tmsb << 12) |
                     ((uint32_t)raw->tlsb << 4) |
                     ((uint32_t)raw->txsb >> 4);

  raw->humidity = ((uint32_t)raw->hmsb << 8) |
                  ((uint32_t)raw->hlsb);
}

float getAltitude(float pressure) {
  return 44330.0 *
         (1.0 - pow(pressure / MEAN_SEA_LEVEL_PRESSURE, 0.190294957));
}
