#include "teensyclient.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <utility>

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
constexpr size_t kLedStatePageSize = 24;
constexpr size_t kTeamNameMaxLen = 36;
constexpr size_t kTeamNameChunkLen = 6;

uint8_t clampColor(int value) {
  return static_cast<uint8_t>(std::max(0, std::min(255, value)));
}

uint8_t roleIndex(const std::string &role) {
  if (role == "home_1")
    return 0;
  if (role == "home_2")
    return 1;
  if (role == "away_1")
    return 2;
  if (role == "away_2")
    return 3;
  return 255;
}
} // namespace

TeensyClient::TeensyClient(std::string bus, uint8_t addr)
    : bus_(std::move(bus)), addr_(addr) {
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

  if (!on.load(std::memory_order_relaxed)) {
    LOG_WARN() << "Teensy write skipped because lights/Teensy power is off";
    return false;
  }

  std::lock_guard<std::mutex> lk(mtx_);
  if (!ensureOpenLocked())
    return false;

#if UBUNTU == 1
  std::cerr << "[fake-i2c] " << int(b0) << " " << int(b1) << " " << int(b2)
            << " " << int(b3) << " " << int(b4) << " " << int(b5) << " "
            << int(b6) << " " << int(b7) << "\n";
  fakeHandleWriteLocked(b0, b1, b2, b3, b4, b5, b6, b7);
  return true;
#else
  uint8_t buf[8] = {b0, b1, b2, b3, b4, b5, b6, b7};
  ssize_t w = ::write(fd_, buf, 8);
  if (w != 8) {
    LOG_ERROR() << "[i2c] write failed cmd=0x" << std::hex << int(b0)
                << std::dec << " wrote=" << w
                << " errno=" << std::strerror(errno);
    return false;
  }
  return true;
#endif
}

#if UBUNTU == 1
void TeensyClient::fakeHandleWriteLocked(uint8_t b0, uint8_t b1, uint8_t b2,
                                         uint8_t b3, uint8_t b4, uint8_t b5,
                                         uint8_t b6, uint8_t b7) {
  switch (b0) {
  case CMD_APPLY_MASK: {
    const uint32_t mask24 = static_cast<uint32_t>(b2) |
                            (static_cast<uint32_t>(b3) << 8) |
                            (static_cast<uint32_t>(b4) << 16);

    for (uint8_t led = 0; led < fake_num_leds_; ++led) {
      if ((mask24 & (1u << led)) == 0)
        continue;

      const size_t offset = static_cast<size_t>(led) * 3;
      if (b1 <= 2) {
        fake_led_state_[offset + b1] = b5;
      } else if (b1 == 3) {
        fake_led_state_[offset + 0] = b5;
        fake_led_state_[offset + 1] = b6;
        fake_led_state_[offset + 2] = b7;
      }
    }
    break;
  }
  case CMD_LED_FRAME:
    if (b1 < fake_num_leds_) {
      const size_t offset = static_cast<size_t>(b1) * 3;
      fake_led_state_[offset + 0] = b2;
      fake_led_state_[offset + 1] = b3;
      fake_led_state_[offset + 2] = b4;
    }
    break;
  case CMD_BEGIN_FILE:
    fake_file_type_ = b1;
    fake_file_id_ = b2;
    fake_expected_lines_ = b3;
    fake_received_lines_ = 0;
    fake_file_status_ = (fake_expected_lines_ > 0) ? FILE_RECEIVING
                                                   : FILE_ERROR;
    break;
  case CMD_FILE_CHUNK:
    if (fake_file_status_ != FILE_RECEIVING) {
      fake_file_status_ = FILE_ERROR;
      break;
    }
    ++fake_received_lines_;
    if (fake_received_lines_ > fake_expected_lines_)
      fake_file_status_ = FILE_ERROR;
    break;
  case CMD_END_FILE:
    fake_file_status_ =
        (fake_file_status_ == FILE_RECEIVING && b1 == fake_expected_lines_ &&
         fake_received_lines_ == fake_expected_lines_)
            ? FILE_SUCCESS
            : FILE_ERROR;
    break;
	  case CMD_ABORT_FILE:
	    fake_file_status_ = FILE_IDLE;
    fake_file_type_ = 0;
    fake_file_id_ = 0;
    fake_expected_lines_ = 0;
    fake_received_lines_ = 0;
	    break;
  case CMD_BEGIN_TEAM:
    fake_file_status_ = FILE_RECEIVING;
    fake_expected_lines_ = b4;
    fake_received_lines_ = 0;
    break;
  case CMD_TEAM_NAME_CHUNK:
    break;
  case CMD_TEAM_COLOR:
    if (fake_file_status_ != FILE_RECEIVING || fake_received_lines_ >= fake_expected_lines_)
      fake_file_status_ = FILE_ERROR;
    else
      ++fake_received_lines_;
    break;
  case CMD_END_TEAM:
    fake_file_status_ =
        (fake_file_status_ == FILE_RECEIVING && fake_received_lines_ == fake_expected_lines_)
            ? FILE_SUCCESS
            : FILE_ERROR;
    break;
  case CMD_DELETE_TEAM:
  case CMD_ACTIVATE_TEAM:
    fake_file_status_ = FILE_SUCCESS;
    break;
  case CMD_HARDWARE_CONFIG:
    if (b1 >= 1 && b1 <= 32 && b2 >= 1 && b2 <= 16) {
      fake_num_leds_ = b1;
      fake_num_shift_regs_ = b2;
      fake_led_state_.assign(static_cast<size_t>(fake_num_leds_) * 3, 0);
      fake_file_status_ = FILE_SUCCESS;
    } else {
      fake_file_status_ = FILE_ERROR;
    }
    break;
	  default:
	    break;
	  }
}
#endif

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

bool TeensyClient::applyPatternSpeed(uint8_t patternId, uint8_t speed) {
  return write8(CMD_PATTERN_SPEED, patternId, clampSpeed(speed), 0, 0, 0, 0, 0);
}

bool TeensyClient::sendHardwareConfig(uint8_t numLeds,
                                      uint8_t numShiftRegs) {
  if (numLeds < 1 || numLeds > 32 || numShiftRegs < 1 ||
      numShiftRegs > 16) {
    LOG_WARN() << "Rejected invalid hardware config LEDs=" << int(numLeds)
               << " shift_regs=" << int(numShiftRegs);
    return false;
  }

  return write8(CMD_HARDWARE_CONFIG, numLeds, numShiftRegs, 0, 0, 0, 0, 0);
}

// ============================================================
// Reads
// ============================================================

bool TeensyClient::requestThenRead(uint8_t req_code, uint8_t *rx,
                                   size_t rx_len) {
  return requestThenRead(req_code, 0, rx, rx_len);
}

bool TeensyClient::requestThenRead(uint8_t req_code, uint8_t req_arg,
                                   uint8_t *rx, size_t rx_len) {

  if (!on.load(std::memory_order_relaxed))
    return false;

  std::lock_guard<std::mutex> lk(mtx_);
  if (!ensureOpenLocked())
    return false;

#if UBUNTU == 1
  memset(rx, 0, rx_len);
  if (req_code == REQ_WAKE_READY && rx_len >= 1) {
    rx[0] = fake_connected_ ? 1 : 0;
  } else if (req_code == REQ_FILE_STATUS && rx_len >= 1) {
    rx[0] = fake_file_status_;
  } else if (req_code == REQ_ALL_OFF_STATUS && rx_len >= 1) {
    bool allOff = true;
    for (uint8_t value : fake_led_state_) {
      if (value != 0) {
        allOff = false;
        break;
      }
    }
    rx[0] = allOff ? 1 : 0;
  } else if (req_code == REQ_LED_STATE_PAGE) {
    const size_t offset = static_cast<size_t>(req_arg) * kLedStatePageSize;
    if (offset < fake_led_state_.size()) {
      const size_t len =
          std::min(rx_len, fake_led_state_.size() - offset);
      std::memcpy(rx, fake_led_state_.data() + offset, len);
    }
  } else if (req_code == REQ_LED_STATE) {
    const size_t len = std::min(rx_len, fake_led_state_.size());
    std::memcpy(rx, fake_led_state_.data(), len);
  }
  return true;
#else
  struct i2c_rdwr_ioctl_data rdwr {};
  struct i2c_msg msgs[2];

  uint8_t req[2] = {req_code, req_arg};

  msgs[0].addr = addr_;
  msgs[0].flags = 0;
  msgs[0].len = (req_code == REQ_LED_STATE_PAGE || req_arg != 0) ? 2 : 1;
  msgs[0].buf = req;

  msgs[1].addr = addr_;
  msgs[1].flags = I2C_M_RD;
  msgs[1].len = rx_len;
  msgs[1].buf = rx;

  rdwr.msgs = msgs;
  rdwr.nmsgs = 2;

  if (::ioctl(fd_, I2C_RDWR, &rdwr) != 2) {
    LOG_ERROR() << "[i2c] request/read failed req=0x" << std::hex
                << int(req_code) << " arg=" << int(req_arg) << std::dec
                << " len=" << rx_len << " errno=" << std::strerror(errno);
    return false;
  }
  return true;
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
  constexpr size_t total = 3 * NUM_OF_LEDS;
  out.assign(total, 0);

  for (uint8_t page = 0; page * kLedStatePageSize < total; ++page) {
    const size_t offset = page * kLedStatePageSize;
    const size_t len = std::min(kLedStatePageSize, total - offset);
    if (!requestThenRead(REQ_LED_STATE_PAGE, page, out.data() + offset, len))
      return false;
  }

  return true;
}

bool TeensyClient::readAllOffStatus(bool &allOff) {
  uint8_t b = 0;
  if (!requestThenRead(REQ_ALL_OFF_STATUS, &b, 1))
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

bool TeensyClient::sendThemeColor(uint8_t r, uint8_t g, uint8_t b,
                                  uint8_t sequence) {
  return write8(CMD_FILE_CHUNK, r, g, b, sequence, 0, 0, 0);
}

bool TeensyClient::sendThemeColors(uint8_t themeId,
                                   const std::vector<RGB_Color> &colors) {

  if (!beginFile(FILE_THEME, themeId, colors.size(), 1))
    return false;

  for (uint8_t i = 0; i < colors.size(); ++i) {
    const auto &c = colors[i];
    if (!sendThemeColor(c.r, c.g, c.b, i)) {
      abortFile();
      return false;
    }
  }

  return endFile(colors.size());
}

// ============================================================
// Pattern speeds
// ============================================================

bool TeensyClient::sendPatternSpeed(uint8_t patternId, uint8_t speed) {
  const uint8_t sequence = (patternId >= 2 && patternId <= 8) ? patternId - 2 : 0;
  return write8(CMD_FILE_CHUNK, patternId, clampSpeed(speed), sequence, 0, 0, 0,
                0);
}

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

    if (!sendPatternSpeed(patternId, speed)) {
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

bool TeensyClient::syncTeamColors(uint16_t teamId, const std::string &teamName,
                                  const std::vector<TeamColor> &colors) {
  if (teamId == 0)
    return false;

  std::array<const TeamColor *, 4> ordered{nullptr, nullptr, nullptr, nullptr};
  for (const auto &color : colors) {
    const uint8_t idx = roleIndex(color.colorRole);
    if (idx < ordered.size())
      ordered[idx] = &color;
  }

  for (const TeamColor *color : ordered) {
    if (!color) {
      LOG_ERROR() << "Team color sync missing required color teamId=" << teamId;
      return false;
    }
  }

  std::string name = teamName.substr(0, kTeamNameMaxLen);
  const uint8_t nameLen = static_cast<uint8_t>(name.size());
  const uint8_t nameChunks =
      static_cast<uint8_t>((nameLen + kTeamNameChunkLen - 1) / kTeamNameChunkLen);

  if (!write8(CMD_BEGIN_TEAM, teamId & 0xFF, (teamId >> 8) & 0xFF, nameLen,
              4, nameChunks, 1, 0)) {
    return false;
  }

  for (uint8_t chunk = 0; chunk < nameChunks; ++chunk) {
    uint8_t bytes[kTeamNameChunkLen] = {0, 0, 0, 0, 0, 0};
    const size_t offset = static_cast<size_t>(chunk) * kTeamNameChunkLen;
    const size_t len = std::min(kTeamNameChunkLen, name.size() - offset);
    std::memcpy(bytes, name.data() + offset, len);
    if (!write8(CMD_TEAM_NAME_CHUNK, chunk, bytes[0], bytes[1], bytes[2],
                bytes[3], bytes[4], bytes[5])) {
      return false;
    }
  }

  for (uint8_t idx = 0; idx < ordered.size(); ++idx) {
    const TeamColor &color = *ordered[idx];
    if (!write8(CMD_TEAM_COLOR, idx, clampColor(color.r), clampColor(color.g),
                clampColor(color.b), static_cast<uint8_t>(color.displayOrder), 0,
                0)) {
      return false;
    }
  }

  return write8(CMD_END_TEAM, teamId & 0xFF, (teamId >> 8) & 0xFF, 4,
                nameChunks, 0, 0, 0);
}

bool TeensyClient::deleteTeamColors(uint16_t teamId) {
  if (teamId == 0)
    return false;
  return write8(CMD_DELETE_TEAM, teamId & 0xFF, (teamId >> 8) & 0xFF, 0, 0, 0,
                0, 0);
}

bool TeensyClient::activateSportsTeam(uint16_t teamId, bool isHome,
                                      uint8_t patternId) {
  if (teamId == 0)
    return false;
  return write8(CMD_ACTIVATE_TEAM, teamId & 0xFF, (teamId >> 8) & 0xFF,
                isHome ? 1 : 0, patternId, 0, 0, 0);
}

// ============================================================

bool TeensyClient::endFile(uint8_t expectedLines) {
  return write8(CMD_END_FILE, expectedLines, 0, 0, 0, 0, 0, 0);
}

bool TeensyClient::abortFile() {
  return write8(CMD_ABORT_FILE, 0, 0, 0, 0, 0, 0, 0);
}
