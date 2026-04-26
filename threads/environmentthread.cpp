#include "environmentthread.h"

extern "C" {
#include "../drivers/i2c/bme280.h"
}

#include <chrono>

EnvironmentThread::EnvironmentThread() {}

EnvironmentThread::~EnvironmentThread() { stop(); }

void EnvironmentThread::start() {
  if (m_running)
    return;

  m_running = true;
  m_thread = std::thread(&EnvironmentThread::run, this);
}

void EnvironmentThread::stop() {
  if (!m_running)
    return;

  m_running = false;

  if (m_thread.joinable())
    m_thread.join();
}

bool EnvironmentThread::isRunning() const { return m_running; }

std::string EnvironmentThread::lastError() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_lastError;
}

sigc::signal<void, EnvironmentThread::Reading>
EnvironmentThread::signal_environment_changed() {
  return m_signalEnvironmentChanged;
}

void EnvironmentThread::run() {
  while (m_running) {
    Reading reading;

    if (readSensor(reading)) {
      m_signalEnvironmentChanged.emit(reading);
    }

    for (int i = 0; i < 60 && m_running; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

bool EnvironmentThread::readSensor(Reading &reading) {
  int fd = bme280Open();

  if (fd < 0) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError = "Failed to open BME280 on /dev/i2c-1";
    return false;
  }

  if (bme280Configure(fd) < 0) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError = "Failed to configure BME280";
    bme280Close(fd);
    return false;
  }

  bme280_calib_data cal;
  readCalibrationData(fd, &cal);

  bme280_raw_data raw;
  getRawData(fd, &raw);

  int32_t tFine = getTemperatureCalibration(&cal, raw.temperature);

  reading.temperatureC = compensateTemperature(tFine);
  reading.temperatureF = (reading.temperatureC * 9.0f / 5.0f) + 32.0f;
  reading.pressureHPa = compensatePressure(raw.pressure, &cal, tFine) / 100.0f;
  reading.humidity = compensateHumidity(raw.humidity, &cal, tFine);
  reading.altitudeM = getAltitude(reading.pressureHPa);

  bme280Close(fd);
  return true;
}
