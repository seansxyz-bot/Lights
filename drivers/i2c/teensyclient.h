#pragma once

#include "../../models/types.h"
#include "../../utils/logger.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#define NUM_OF_LEDS 19
#define NUM_OF_SHIFT_REGS 8
#define BITMASK_LEN (NUM_OF_SHIFT_REGS * 3)
#define NUM_OF_PADDED_0S (BITMASK_LEN - NUM_OF_LEDS)

class TeensyClient {
public:
  std::atomic<bool> on{false};

  enum : uint8_t {
    CMD_APPLY_MASK = 0x15,
    CMD_PATTERN_SPEED = 0x16,
    CMD_LED_FRAME = 0x17,

    CMD_BEGIN_FILE = 0x20,
    CMD_FILE_CHUNK = 0x21,
    CMD_END_FILE = 0x22,
    CMD_ABORT_FILE = 0x23,

    REQ_WAKE_READY = 0xF0,
    REQ_LED_STATE = 0xF1,
    REQ_SHUTDOWN = 0xF2,
    REQ_FILE_STATUS = 0xF3,
  };

  enum : uint8_t {
    FILE_THEME = 0x01,
    FILE_PATTERN = 0x02,
  };

  enum : uint8_t {
    FILE_IDLE = 0,
    FILE_RECEIVING = 1,
    FILE_SUCCESS = 2,
    FILE_ERROR = 3,
  };

  TeensyClient();
  ~TeensyClient();

  bool openBus();
  void closeBus();

  // --- live LED writes ---
  bool applyMaskedSingle(uint8_t channel, uint32_t mask24, uint8_t value);
  bool applyMaskedRGB(uint32_t mask24, uint8_t r, uint8_t g, uint8_t b);
  bool applyThemePattern(uint8_t themeId, uint8_t patternId);

  // Live preview only. Does NOT save on Teensy.
  bool applyPatternSpeed(uint8_t patternId, uint8_t speed);

  // --- read requests ---
  bool readWakeReady(bool &ready);
  bool readLedState(std::vector<uint8_t> &out_rgb);
  bool readShutdownAck(bool &allOff);
  bool readFileStatus(uint8_t &status);

  // --- file transfer ---
  bool beginFile(uint8_t fileType, uint8_t fileId, uint8_t lineCount,
                 uint8_t version = 1);

  bool sendThemeColor(uint8_t r, uint8_t g, uint8_t b);
  bool sendThemeColors(uint8_t themeId, const std::vector<RGB_Color> &colors);

  bool sendPatternSpeed(uint8_t speed);
  bool sendPatternSpeeds(const std::vector<Pattern> &patterns);

  bool sendLedFrame(const std::vector<std::array<uint8_t, 3>> &frame);

  bool endFile(uint8_t expectedLines);
  bool abortFile();

  static inline uint32_t mask24FromBitString(std::string bitStr) {
    bitStr = std::string(NUM_OF_PADDED_0S, '0') + bitStr;

    std::string s;
    s.reserve(bitStr.size());
    for (char c : bitStr) {
      if (c == '0' || c == '1')
        s.push_back(c);
    }

    uint32_t mask = 0;
    int len = static_cast<int>(s.size());
    if (len > 24)
      len = 24;

    for (int i = 0; i < len; ++i) {
      char c = s[len - 1 - i];
      if (c == '1')
        mask |= (1u << i);
    }
    return mask;
  }

  int fd() const { return fd_; }
  bool requestThenRead(uint8_t req_code, uint8_t *rx, size_t rx_len);

private:
  bool ensureOpenLocked();
  bool ensureOpen();

  bool write8(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
              uint8_t b5, uint8_t b6, uint8_t b7);

#if UBUNTU == 1
  void fakeHandleWriteLocked(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                             uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7);

  bool fake_connected_ = false;
  uint8_t fake_file_status_ = FILE_IDLE;
  std::vector<uint8_t> fake_led_state_;

  uint8_t fake_file_type_ = 0;
  uint8_t fake_file_id_ = 0;
  uint8_t fake_expected_lines_ = 0;
  uint8_t fake_received_lines_ = 0;
#endif

  mutable std::mutex mtx_;
  std::string bus_;
  uint8_t addr_;
  int fd_ = -1;
};
