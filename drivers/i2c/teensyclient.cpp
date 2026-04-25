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

bool TeensyClient::ensureOpenLocked() {
#if UBUNTU == 1
  fake_connected_ = true;
  return true;
#else
  if (fd_ >= 0)
    return true;

  int fd = ::open(bus_.c_str(), O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    std::cerr << "[i2c] open(" << bus_ << ") failed: " << std::strerror(errno)
              << "\n";
    return false;
  }

  if (::ioctl(fd, I2C_SLAVE, addr_) < 0) {
    std::cerr << "[i2c] ioctl(I2C_SLAVE, 0x" << std::hex << int(addr_)
              << std::dec << ") failed: " << std::strerror(errno) << "\n";
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
  fake_file_status_ = FILE_IDLE;
  fake_file_type_ = 0;
  fake_file_id_ = 0;
  fake_expected_lines_ = 0;
  fake_received_lines_ = 0;
#else
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
#endif
}

#if UBUNTU == 1
void TeensyClient::fakeHandleWriteLocked(uint8_t b0, uint8_t b1, uint8_t b2,
                                         uint8_t b3, uint8_t b4, uint8_t b5,
                                         uint8_t b6, uint8_t b7) {
  (void)b4;
  (void)b5;
  (void)b6;
  (void)b7;

  switch (b0) {
  case CMD_APPLY_MASK: {
    const uint8_t channel = b1;
    const uint32_t mask24 = static_cast<uint32_t>(b2) |
                            (static_cast<uint32_t>(b3) << 8) |
                            (static_cast<uint32_t>(b4) << 16);

    if (channel <= 2) {
      for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
        if ((mask24 >> i) & 0x1u) {
          fake_led_state_[i * 3 + channel] = b5;
        }
      }
    } else if (channel == 3) {
      for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
        if ((mask24 >> i) & 0x1u) {
          fake_led_state_[i * 3 + 0] = b5;
          fake_led_state_[i * 3 + 1] = b6;
          fake_led_state_[i * 3 + 2] = b7;
        }
      }
    }
    break;
  }

  case CMD_BEGIN_FILE:
    fake_file_type_ = b1;
    fake_file_id_ = b2;
    fake_expected_lines_ = b3;
    fake_received_lines_ = 0;
    fake_file_status_ = FILE_RECEIVING;
    break;

  case CMD_FILE_CHUNK:
    if (fake_file_status_ == FILE_RECEIVING) {
      if (fake_received_lines_ < 255)
        ++fake_received_lines_;
    }
    break;

  case CMD_END_FILE:
    if (fake_file_status_ == FILE_RECEIVING &&
        fake_received_lines_ == fake_expected_lines_) {
      fake_file_status_ = FILE_SUCCESS;
    } else {
      fake_file_status_ = FILE_ERROR;
    }
    break;

  case CMD_ABORT_FILE:
    fake_file_status_ = FILE_ERROR;
    fake_file_type_ = 0;
    fake_file_id_ = 0;
    fake_expected_lines_ = 0;
    fake_received_lines_ = 0;
    break;

  default:
    break;
  }
}
#endif

bool TeensyClient::write8(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                          uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7) {
  if (!on.load(std::memory_order_relaxed))
    return true;

  std::lock_guard<std::mutex> lk(mtx_);
  if (!ensureOpenLocked()) {
    std::cerr << "[i2c] write8: failed to auto-open bus\n";
    return false;
  }

#if UBUNTU == 1
  std::cerr << "[fake-i2c] write8: " << "0x" << std::hex << int(b0) << " "
            << int(b1) << " " << int(b2) << " " << int(b3) << " " << int(b4)
            << " " << int(b5) << " " << int(b6) << " " << int(b7) << std::dec
            << "\n";
  fakeHandleWriteLocked(b0, b1, b2, b3, b4, b5, b6, b7);
  return true;
#else
  uint8_t buf[8] = {b0, b1, b2, b3, b4, b5, b6, b7};
  const ssize_t w = ::write(fd_, buf, sizeof(buf));
  if (w != static_cast<ssize_t>(sizeof(buf))) {
    std::cerr << "[i2c] write8 short/err (" << w
              << "): " << std::strerror(errno) << "\n";
    return false;
  }
  return true;
#endif
}

bool TeensyClient::applyMaskedSingle(uint8_t channel, uint32_t mask24,
                                     uint8_t value) {
  if (channel > 2)
    return false;

  return write8(CMD_APPLY_MASK, channel, static_cast<uint8_t>(mask24 & 0xFF),
                static_cast<uint8_t>((mask24 >> 8) & 0xFF),
                static_cast<uint8_t>((mask24 >> 16) & 0xFF), value, 0x00, 0x00);
}

bool TeensyClient::applyMaskedRGB(uint32_t mask24, uint8_t r, uint8_t g,
                                  uint8_t b) {
  return write8(CMD_APPLY_MASK, 3, static_cast<uint8_t>(mask24 & 0xFF),
                static_cast<uint8_t>((mask24 >> 8) & 0xFF),
                static_cast<uint8_t>((mask24 >> 16) & 0xFF), r, g, b);
}

bool TeensyClient::applyThemePattern(uint8_t themeId, uint8_t patternId) {
  const uint8_t channel = static_cast<uint8_t>(4 + themeId);
  return write8(CMD_APPLY_MASK, channel, 0u, 0u, 0u, patternId, 0u, 0u);
}

bool TeensyClient::requestThenRead(uint8_t req_code, uint8_t *rx,
                                   size_t rx_len) {
  if (!on.load(std::memory_order_relaxed))
    return false;
  if (rx == nullptr || rx_len == 0)
    return false;

  std::lock_guard<std::mutex> lk(mtx_);
  if (!ensureOpenLocked()) {
    std::cerr << "[i2c] requestThenRead: failed to auto-open bus\n";
    return false;
  }

#if UBUNTU == 1
  std::memset(rx, 0, rx_len);

  switch (req_code) {
  case REQ_WAKE_READY:
    if (rx_len >= 1)
      rx[0] = 1;
    break;

  case REQ_LED_STATE: {
    const size_t n = std::min(rx_len, fake_led_state_.size());
    if (n > 0)
      std::memcpy(rx, fake_led_state_.data(), n);
    break;
  }

  case REQ_SHUTDOWN:
    if (rx_len >= 1)
      rx[0] = 1;
    break;

  case REQ_FILE_STATUS:
    if (rx_len >= 1)
      rx[0] = fake_file_status_;
    break;

  default:
    break;
  }

  std::cerr << "[fake-i2c] requestThenRead req=0x" << std::hex << int(req_code)
            << std::dec << " len=" << rx_len << "\n";
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
  msgs[1].len = static_cast<__u16>(rx_len);
  msgs[1].buf = rx;

  rdwr.msgs = msgs;
  rdwr.nmsgs = 2;

  const int rc = ::ioctl(fd_, I2C_RDWR, &rdwr);
  if (rc != 2) {
    std::cerr << "[i2c] I2C_RDWR failed (rc=" << rc
              << "): " << std::strerror(errno) << "\n";
    return false;
  }
  return true;
#endif
}

bool TeensyClient::readWakeReady(bool &ready) {
  ready = false;
  uint8_t b = 0;
  if (!requestThenRead(REQ_WAKE_READY, &b, 1))
    return false;
  ready = (b != 0);
  return true;
}

bool TeensyClient::readLedState(std::vector<uint8_t> &out_rgb) {
  const size_t len = 3 * NUM_OF_LEDS;
  out_rgb.assign(len, 0);
  return requestThenRead(REQ_LED_STATE, out_rgb.data(), len);
}

bool TeensyClient::readShutdownAck(bool &allOff) {
  allOff = false;
  uint8_t b = 0;
  if (!requestThenRead(REQ_SHUTDOWN, &b, 1))
    return false;
  allOff = (b != 0);
  return true;
}

bool TeensyClient::readFileStatus(uint8_t &status) {
  status = FILE_ERROR;
  uint8_t b = FILE_ERROR;
  if (!requestThenRead(REQ_FILE_STATUS, &b, 1))
    return false;
  status = b;
  return true;
}

bool TeensyClient::beginFile(uint8_t fileType, uint8_t fileId,
                             uint8_t lineCount, uint8_t version) {
  return write8(CMD_BEGIN_FILE, fileType, fileId, lineCount, version, 0u, 0u,
                0u);
}

bool TeensyClient::sendThemeColor(uint8_t r, uint8_t g, uint8_t b) {
  return write8(CMD_FILE_CHUNK, r, g, b, 0u, 0u, 0u, 0u);
}

bool TeensyClient::sendThemeColors(uint8_t themeId,
                                   const std::vector<RGB_Color> &colors) {
  if (colors.size() > 255)
    return false;

  if (!beginFile(FILE_THEME, themeId, static_cast<uint8_t>(colors.size()), 1))
    return false;

  for (const auto &c : colors) {
    if (!sendThemeColor(c.r, c.g, c.b)) {
      abortFile();
      return false;
    }
  }

  if (!endFile(static_cast<uint8_t>(colors.size()))) {
    abortFile();
    return false;
  }

  return true;
}

bool TeensyClient::sendPatternSpeed(uint8_t speed) {
  return write8(CMD_FILE_CHUNK, clampSpeed(speed), 0u, 0u, 0u, 0u, 0u, 0u);
}

bool TeensyClient::sendPatternSpeeds(uint8_t patternId,
                                     const std::vector<uint8_t> &speeds) {
  if (patternId == 0)
    return false;

  const bool isCombo = (patternId == 1);
  const size_t expected = isCombo ? 7u : 1u;

  if (speeds.size() != expected)
    return false;

  if (!beginFile(FILE_PATTERN, patternId, static_cast<uint8_t>(expected), 1))
    return false;

  for (uint8_t speed : speeds) {
    if (!sendPatternSpeed(speed)) {
      abortFile();
      return false;
    }
  }

  if (!endFile(static_cast<uint8_t>(expected))) {
    abortFile();
    return false;
  }

  return true;
}

bool TeensyClient::endFile(uint8_t expectedLines) {
  return write8(CMD_END_FILE, expectedLines, 0u, 0u, 0u, 0u, 0u, 0u);
}

bool TeensyClient::abortFile() {
  return write8(CMD_ABORT_FILE, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
}
