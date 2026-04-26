#include "teensyclient.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>

#ifndef UBUNTU
#define UBUNTU 0
#endif

#if UBUNTU != 1
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {
static inline uint8_t clampSpeed(uint8_t v) { return (v > 100) ? 100 : v; }
} // namespace

TeensyClient::TeensyClient() : bus_("/dev/i2c-1"), addr_(0x08) {
#if UBUNTU == 1
  fake_led_state_.assign(3 * NUM_OF_LEDS, 0);
#endif
}

TeensyClient::~TeensyClient() { closeBus(); }

// ============================================================
// Bus handling
// ============================================================

bool TeensyClient::ensureOpenLocked() {
#if UBUNTU == 1
  fake_connected_ = true;
  return true;
#else
  if (fd_ >= 0)
    return true;

  int fd = ::open(bus_.c_str(), O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    std::cerr << "[i2c] open failed: " << std::strerror(errno) << "\n";
    return false;
  }

  if (::ioctl(fd, I2C_SLAVE, addr_) < 0) {
    std::cerr << "[i2c] ioctl failed: " << std::strerror(errno) << "\n";
    ::close(fd);
    return false;
  }

  fd_ = fd;
  return true;
#endif
}

bool TeensyClient::ensureOpen() {
  std::lock_guard<std::mutex> lk(mtx_);
  return ensureOpenLocked();
}

bool TeensyClient::openBus() { return ensureOpen(); }

void TeensyClient::closeBus() {
  std::lock_guard<std::mutex> lk(mtx_);

#if UBUNTU == 1
  fake_connected_ = false;
#else
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
#endif
}

// ============================================================
// Low-level write
// ============================================================

bool TeensyClient::write8(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                          uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7) {

  if (!on.load(std::memory_order_relaxed))
    return true;

  std::lock_guard<std::mutex> lk(mtx_);
  if (!ensureOpenLocked())
    return false;

#if UBUNTU == 1
  std::cerr << "[fake-i2c] " << int(b0) << " " << int(b1) << " " << int(b2)
            << " " << int(b3) << " " << int(b4) << " " << int(b5) << " "
            << int(b6) << " " << int(b7) << "\n";
  return true;
#else
  uint8_t buf[8] = {b0, b1, b2, b3, b4, b5, b6, b7};
  ssize_t w = ::write(fd_, buf, 8);
  return (w == 8);
#endif
}

// ============================================================
// Live control
// ============================================================

bool TeensyClient::applyMaskedSingle(uint8_t channel, uint32_t mask24,
                                     uint8_t value) {
  if (channel > 2)
    return false;

  return write8(CMD_APPLY_MASK, channel, mask24 & 0xFF, (mask24 >> 8) & 0xFF,
                (mask24 >> 16) & 0xFF, value, 0, 0);
}

bool TeensyClient::applyMaskedRGB(uint32_t mask24, uint8_t r, uint8_t g,
                                  uint8_t b) {
  return write8(CMD_APPLY_MASK, 3, mask24 & 0xFF, (mask24 >> 8) & 0xFF,
                (mask24 >> 16) & 0xFF, r, g, b);
}

bool TeensyClient::applyThemePattern(uint8_t themeId, uint8_t patternId) {
  uint8_t channel = 4 + themeId;
  return write8(CMD_APPLY_MASK, channel, 0, 0, 0, patternId, 0, 0);
}

// 🔥 NEW — live preview speed (no file write)
bool TeensyClient::applyPatternSpeed(uint8_t patternId, uint8_t speed) {
  return write8(CMD_PATTERN_SPEED, patternId, clampSpeed(speed), 0, 0, 0, 0, 0);
}

// ============================================================
// Reads
// ============================================================

bool TeensyClient::requestThenRead(uint8_t req_code, uint8_t *rx,
                                   size_t rx_len) {

  if (!on.load(std::memory_order_relaxed))
    return false;

  std::lock_guard<std::mutex> lk(mtx_);
  if (!ensureOpenLocked())
    return false;

#if UBUNTU == 1
  memset(rx, 0, rx_len);
  return true;
#else
  struct i2c_rdwr_ioctl_data rdwr {};
  struct i2c_msg msgs[2];

  uint8_t req = req_code;

  msgs[0].addr = addr_;
  msgs[0].flags = 0;
  msgs[0].len = 1;
  msgs[0].buf = &req;

  msgs[1].addr = addr_;
  msgs[1].flags = I2C_M_RD;
  msgs[1].len = rx_len;
  msgs[1].buf = rx;

  rdwr.msgs = msgs;
  rdwr.nmsgs = 2;

  return (::ioctl(fd_, I2C_RDWR, &rdwr) == 2);
#endif
}

bool TeensyClient::readWakeReady(bool &ready) {
  uint8_t b = 0;
  if (!requestThenRead(REQ_WAKE_READY, &b, 1))
    return false;
  ready = b;
  return true;
}

bool TeensyClient::readLedState(std::vector<uint8_t> &out) {
  out.resize(3 * NUM_OF_LEDS);
  return requestThenRead(REQ_LED_STATE, out.data(), out.size());
}

bool TeensyClient::readShutdownAck(bool &allOff) {
  uint8_t b = 0;
  if (!requestThenRead(REQ_SHUTDOWN, &b, 1))
    return false;
  allOff = b;
  return true;
}

bool TeensyClient::readFileStatus(uint8_t &status) {
  return requestThenRead(REQ_FILE_STATUS, &status, 1);
}

// ============================================================
// File transfer
// ============================================================

bool TeensyClient::beginFile(uint8_t type, uint8_t id, uint8_t lines,
                             uint8_t version) {
  return write8(CMD_BEGIN_FILE, type, id, lines, version, 0, 0, 0);
}

bool TeensyClient::sendThemeColor(uint8_t r, uint8_t g, uint8_t b) {
  return write8(CMD_FILE_CHUNK, r, g, b, 0, 0, 0, 0);
}

bool TeensyClient::sendThemeColors(uint8_t themeId,
                                   const std::vector<RGB_Color> &colors) {

  if (!beginFile(FILE_THEME, themeId, colors.size(), 1))
    return false;

  for (const auto &c : colors) {
    if (!sendThemeColor(c.r, c.g, c.b)) {
      abortFile();
      return false;
    }
  }

  return endFile(colors.size());
}

// ============================================================
// Pattern speeds (REAL FIX)
// ============================================================

// Single line (patternId + speed)
bool TeensyClient::sendPatternSpeed(uint8_t speed) {
  return write8(CMD_FILE_CHUNK, speed, 0, 0, 0, 0, 0, 0);
}

// Send ALL patterns (this matches MainWindow usage)
bool TeensyClient::sendPatternSpeeds(const std::vector<Pattern> &patterns) {
  constexpr uint8_t kPatternBulkFileId = 1;
  constexpr uint8_t kPatternSpeedCount = 7; // IDs 2..8
  constexpr uint8_t kPatternFileVersion = 1;

  if (!beginFile(FILE_PATTERN, kPatternBulkFileId, kPatternSpeedCount,
                 kPatternFileVersion)) {
    return false;
  }

  for (uint8_t patternId = 2; patternId <= 8; ++patternId) {
    auto it = std::find_if(
        patterns.begin(), patterns.end(),
        [patternId](const Pattern &p) { return p.id == patternId; });

    const uint8_t speed =
        (it != patterns.end()) ? static_cast<uint8_t>(it->speed) : 50;

    if (!sendPatternSpeed(speed)) {
      abortFile();
      return false;
    }
  }

  return endFile(kPatternSpeedCount);
}

bool TeensyClient::sendLedFrame(
    const std::vector<std::array<uint8_t, 3>> &frame) {

  const size_t count = std::min(frame.size(), static_cast<size_t>(NUM_OF_LEDS));

  for (size_t i = 0; i < count; ++i) {
    const auto &rgb = frame[i];

    if (!write8(CMD_LED_FRAME, static_cast<uint8_t>(i), rgb[0], rgb[1], rgb[2],
                0, 0, 0)) {
      return false;
    }
  }

  return true;
}

// ============================================================

bool TeensyClient::endFile(uint8_t expectedLines) {
  return write8(CMD_END_FILE, expectedLines, 0, 0, 0, 0, 0, 0);
}

bool TeensyClient::abortFile() {
  return write8(CMD_ABORT_FILE, 0, 0, 0, 0, 0, 0, 0);
}
