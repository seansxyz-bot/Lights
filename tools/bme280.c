#include "bme280.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if __has_include(<i2c/smbus.h>)
#include <i2c/smbus.h>
#else
#error "Missing <i2c/smbus.h>. Install libi2c-dev (Debian/Ubuntu/Raspberry Pi OS)."
#endif

static bool bme280_write_reg8(int fd, uint8_t reg, uint8_t value) {
  return i2c_smbus_write_byte_data(fd, reg, value) >= 0;
}

static bool bme280_read_reg8(int fd, uint8_t reg, uint8_t *value) {
  int v = i2c_smbus_read_byte_data(fd, reg);
  if (v < 0) {
    return false;
  }
  *value = (uint8_t)v;
  return true;
}

static bool bme280_read_reg16_le(int fd, uint8_t reg, uint16_t *value) {
  int v = i2c_smbus_read_word_data(fd, reg);
  if (v < 0) {
    return false;
  }

  // SMBus read_word_data returns low byte first for devices like BME280
  *value = (uint16_t)v;
  return true;
}

bool bme280_open(int *fd_out) {
  if (!fd_out) {
    return false;
  }

  int fd = open(BME280_I2C_DEVICE, O_RDWR);
  if (fd < 0) {
    return false;
  }

  if (ioctl(fd, I2C_SLAVE, BME280_ADDRESS) < 0) {
    close(fd);
    return false;
  }

  *fd_out = fd;
  return true;
}

void bme280_close(int fd) {
  if (fd >= 0) {
    close(fd);
  }
}

bool bme280_read_environment(bme280_env_data *out) {
  if (!out) {
    return false;
  }

  int fd = -1;
  if (!bme280_open(&fd)) {
    return false;
  }

  bme280_calib_data cal;
  readCalibrationData(fd, &cal);

  if (!bme280_write_reg8(fd, BME280_REGISTER_CONTROLHUMID, 0x01)) {
    bme280_close(fd);
    return false;
  }

  if (!bme280_write_reg8(fd, BME280_REGISTER_CONTROL, 0x25)) {
    bme280_close(fd);
    return false;
  }

  bme280_raw_data raw;
  getRawData(fd, &raw);

  int32_t t_fine = getTemperatureCalibration(&cal, raw.temperature);

  out->temperature_c = compensateTemperature(t_fine);
  out->pressure_hpa = compensatePressure(raw.pressure, &cal, t_fine) / 100.0f;
  out->humidity = compensateHumidity(raw.humidity, &cal, t_fine);
  out->altitude_m = getAltitude(out->pressure_hpa);

  bme280_close(fd);
  return true;
}

void readCalibrationData(int fd, bme280_calib_data *data) {
  uint16_t u16 = 0;
  uint8_t b1 = 0, b2 = 0;

  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_T1, &u16);
  data->dig_T1 = (uint16_t)u16;
  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_T2, &u16);
  data->dig_T2 = (int16_t)u16;
  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_T3, &u16);
  data->dig_T3 = (int16_t)u16;

  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_P1, &u16);
  data->dig_P1 = (uint16_t)u16;
  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_P2, &u16);
  data->dig_P2 = (int16_t)u16;
  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_P3, &u16);
  data->dig_P3 = (int16_t)u16;
  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_P4, &u16);
  data->dig_P4 = (int16_t)u16;
  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_P5, &u16);
  data->dig_P5 = (int16_t)u16;
  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_P6, &u16);
  data->dig_P6 = (int16_t)u16;
  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_P7, &u16);
  data->dig_P7 = (int16_t)u16;
  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_P8, &u16);
  data->dig_P8 = (int16_t)u16;
  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_P9, &u16);
  data->dig_P9 = (int16_t)u16;

  bme280_read_reg8(fd, BME280_REGISTER_DIG_H1, &data->dig_H1);

  bme280_read_reg16_le(fd, BME280_REGISTER_DIG_H2, &u16);
  data->dig_H2 = (int16_t)u16;

  bme280_read_reg8(fd, BME280_REGISTER_DIG_H3, &data->dig_H3);

  bme280_read_reg8(fd, BME280_REGISTER_DIG_H4, &b1);
  bme280_read_reg8(fd, BME280_REGISTER_DIG_H4 + 1, &b2);
  data->dig_H4 = (int16_t)((b1 << 4) | (b2 & 0x0F));

  bme280_read_reg8(fd, BME280_REGISTER_DIG_H5, &b1);
  bme280_read_reg8(fd, BME280_REGISTER_DIG_H5 + 1, &b2);
  data->dig_H5 = (int16_t)((b2 << 4) | (b1 >> 4));

  {
    uint8_t h6 = 0;
    bme280_read_reg8(fd, BME280_REGISTER_DIG_H6, &h6);
    data->dig_H6 = (int8_t)h6;
  }
}

int32_t getTemperatureCalibration(bme280_calib_data *cal, int32_t adc_T) {
  int32_t var1 = ((((adc_T >> 3) - ((int32_t)cal->dig_T1 << 1))) *
                  ((int32_t)cal->dig_T2)) >> 11;

  int32_t var2 = (((((adc_T >> 4) - ((int32_t)cal->dig_T1)) *
                    ((adc_T >> 4) - ((int32_t)cal->dig_T1))) >> 12) *
                  ((int32_t)cal->dig_T3)) >> 14;

  return var1 + var2;
}

float compensateTemperature(int32_t t_fine) {
  float T = (float)((t_fine * 5 + 128) >> 8);
  return T / 100.0f;
}

float compensatePressure(int32_t adc_P, bme280_calib_data *cal, int32_t t_fine) {
  int64_t var1, var2, p;

  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)cal->dig_P6;
  var2 = var2 + ((var1 * (int64_t)cal->dig_P5) << 17);
  var2 = var2 + (((int64_t)cal->dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)cal->dig_P3) >> 8) +
         ((var1 * (int64_t)cal->dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)cal->dig_P1) >> 33;

  if (var1 == 0) {
    return 0.0f;
  }

  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)cal->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)cal->dig_P8) * p) >> 19;

  p = ((p + var1 + var2) >> 8) + (((int64_t)cal->dig_P7) << 4);
  return (float)p / 256.0f;
}

float compensateHumidity(int32_t adc_H, bme280_calib_data *cal, int32_t t_fine) {
  int32_t v_x1_u32r;

  v_x1_u32r = (t_fine - ((int32_t)76800));

  v_x1_u32r = (((((adc_H << 14) - (((int32_t)cal->dig_H4) << 20) -
                  (((int32_t)cal->dig_H5) * v_x1_u32r)) +
                 ((int32_t)16384)) >> 15) *
               (((((((v_x1_u32r * ((int32_t)cal->dig_H6)) >> 10) *
                    (((v_x1_u32r * ((int32_t)cal->dig_H3)) >> 11) +
                     ((int32_t)32768))) >> 10) +
                  ((int32_t)2097152)) *
                     ((int32_t)cal->dig_H2) +
                 8192) >> 14));

  v_x1_u32r =
      (v_x1_u32r -
       (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
         ((int32_t)cal->dig_H1)) >> 4));

  v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
  v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;

  float h = (float)(v_x1_u32r >> 12);
  return h / 1024.0f;
}

void getRawData(int fd, bme280_raw_data *raw) {
  uint8_t buf[8] = {0};

  int rc = i2c_smbus_read_i2c_block_data(fd, BME280_REGISTER_PRESSUREDATA, 8, buf);
  if (rc < 0) {
    raw->temperature = 0;
    raw->pressure = 0;
    raw->humidity = 0;
    return;
  }

  raw->pmsb = buf[0];
  raw->plsb = buf[1];
  raw->pxsb = buf[2];

  raw->tmsb = buf[3];
  raw->tlsb = buf[4];
  raw->txsb = buf[5];

  raw->hmsb = buf[6];
  raw->hlsb = buf[7];

  raw->temperature = 0;
  raw->temperature = (raw->temperature | raw->tmsb) << 8;
  raw->temperature = (raw->temperature | raw->tlsb) << 8;
  raw->temperature = (raw->temperature | raw->txsb) >> 4;

  raw->pressure = 0;
  raw->pressure = (raw->pressure | raw->pmsb) << 8;
  raw->pressure = (raw->pressure | raw->plsb) << 8;
  raw->pressure = (raw->pressure | raw->pxsb) >> 4;

  raw->humidity = 0;
  raw->humidity = (raw->humidity | raw->hmsb) << 8;
  raw->humidity = (raw->humidity | raw->hlsb);
}

float getAltitude(float pressure) {
  return 44330.0f *
         (1.0f - powf(pressure / (float)MEAN_SEA_LEVEL_PRESSURE, 0.190294957f));
}
