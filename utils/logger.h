#pragma once

#include <chrono>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#define UBUNTU 1

#ifndef LOG_FILE_MSTR
#if (UBUNTU == 1)
#define LOG_FILE_MSTR "/home/dev/.lightcontroller/logs/log"
#else
#define LOG_FILE_MSTR "/home/lights/.config/lights/logs/log"
#endif
#endif

#define SCREEN 1

class Logger {
public:
  enum class Mode { StdOut, File, StdOutAndFile };

  enum class Level { Info, Warn, Error };

  struct DateOnly {
    std::tm tmValue{};
  };

  struct Time24Only {
    std::tm tmValue{};
  };

  struct Time12Only {
    std::tm tmValue{};
  };

  struct DateTime24 {
    std::tm tmValue{};
  };

  struct DateTime12 {
    std::tm tmValue{};
  };

private:
  struct Destination {
    std::string path;
    std::ofstream stream;
    std::deque<std::string> buffer;
  };

public:
  class Stream {
  public:
    template <typename T> Stream &operator<<(const T &value) {
      Logger::writeToMaster(value);
      return *this;
    }

    Stream &operator<<(std::ostream &(*manip)(std::ostream &)) {
      Logger::writeManipulatorToMaster(manip);
      return *this;
    }

    Stream &operator<<(std::ios &(*manip)(std::ios &)) {
      Logger::writeManipulatorToMaster(manip);
      return *this;
    }

    Stream &operator<<(std::ios_base &(*manip)(std::ios_base &)) {
      Logger::writeManipulatorToMaster(manip);
      return *this;
    }
  };

  class Line {
  public:
    explicit Line(Level level) : m_level(level) {}

    Line(const Line &) = delete;
    Line &operator=(const Line &) = delete;

    Line(Line &&other) noexcept
        : m_level(other.m_level), m_stream(std::move(other.m_stream)),
          m_hasContent(other.m_hasContent), m_finished(other.m_finished) {
      other.m_finished = true;
    }

    Line &operator=(Line &&other) noexcept {
      if (this != &other) {
        flushIfNeeded();
        m_level = other.m_level;
        m_stream.str(other.m_stream.str());
        m_stream.clear();
        m_hasContent = other.m_hasContent;
        m_finished = other.m_finished;
        other.m_finished = true;
      }
      return *this;
    }

    ~Line() { flushIfNeeded(); }

    template <typename T> Line &operator<<(const T &value) {
      m_stream << Logger::toStringValue(value);
      m_hasContent = true;
      return *this;
    }

    Line &operator<<(std::ostream &(*manip)(std::ostream &)) {
      if (isEndlLike(manip)) {
        m_stream << '\n';
        m_hasContent = true;
        flushNow();
        return *this;
      }

      std::ostringstream temp;
      manip(temp);
      m_stream << temp.str();
      m_hasContent = true;
      return *this;
    }

    Line &operator<<(std::ios &(*manip)(std::ios &)) {
      std::ostringstream temp;
      manip(temp);
      m_stream << temp.str();
      m_hasContent = true;
      return *this;
    }

    Line &operator<<(std::ios_base &(*manip)(std::ios_base &)) {
      std::ostringstream temp;
      manip(temp);
      m_stream << temp.str();
      m_hasContent = true;
      return *this;
    }

  private:
    Level m_level;
    std::ostringstream m_stream;
    bool m_hasContent = false;
    bool m_finished = false;

    static bool isEndlLike(std::ostream &(*manip)(std::ostream &)) {
      return manip ==
                 static_cast<std::ostream &(*)(std::ostream &)>(std::endl) ||
             manip ==
                 static_cast<std::ostream &(*)(std::ostream &)>(std::flush) ||
             manip == static_cast<std::ostream &(*)(std::ostream &)>(std::ends);
    }

    void flushNow() {
      if (m_finished || !m_hasContent) {
        m_finished = true;
        return;
      }

      Logger::writeLeveledLine(m_level, m_stream.str());
      m_stream.str("");
      m_stream.clear();
      m_hasContent = false;
      m_finished = true;
    }

    void flushIfNeeded() {
      if (m_finished || !m_hasContent) {
        return;
      }

      std::string text = m_stream.str();
      if (text.empty() || text.back() != '\n') {
        text.push_back('\n');
      }
      Logger::writeLeveledLine(m_level, text);
      m_finished = true;
    }
  };

public:
  inline static Stream log{};

  static void setMode(Mode mode) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_mode = mode;

    if (writesToFileUnlocked()) {
      tryOpenDestinationUnlocked(s_master, s_appendMode);
      tryOpenDestinationUnlocked(s_info, s_appendMode);
      tryOpenDestinationUnlocked(s_warn, s_appendMode);
      tryOpenDestinationUnlocked(s_error, s_appendMode);

      flushDestinationBufferUnlocked(s_master);
      flushDestinationBufferUnlocked(s_info);
      flushDestinationBufferUnlocked(s_warn);
      flushDestinationBufferUnlocked(s_error);
    }
  }

  static void useStdOut() { setMode(Mode::StdOut); }

  static bool useFile(const std::string &path = LOG_FILE_MSTR,
                      bool append = true) {
    std::lock_guard<std::mutex> lock(s_mutex);

    s_appendMode = append;
    setPathsUnlocked(path);

    const bool masterOk = openDestinationUnlocked(s_master, append);
    openDestinationUnlocked(s_info, append);
    openDestinationUnlocked(s_warn, append);
    openDestinationUnlocked(s_error, append);

    if (masterOk) {
      s_mode = Mode::File;
      flushDestinationBufferUnlocked(s_master);
      flushDestinationBufferUnlocked(s_info);
      flushDestinationBufferUnlocked(s_warn);
      flushDestinationBufferUnlocked(s_error);
    }

    return masterOk;
  }

  static bool useStdOutAndFile(const std::string &path = LOG_FILE_MSTR,
                               bool append = true) {
    std::lock_guard<std::mutex> lock(s_mutex);

    s_appendMode = append;
    setPathsUnlocked(path);

    const bool masterOk = openDestinationUnlocked(s_master, append);
    openDestinationUnlocked(s_info, append);
    openDestinationUnlocked(s_warn, append);
    openDestinationUnlocked(s_error, append);

    if (masterOk) {
      s_mode = Mode::StdOutAndFile;
      flushDestinationBufferUnlocked(s_master);
      flushDestinationBufferUnlocked(s_info);
      flushDestinationBufferUnlocked(s_warn);
      flushDestinationBufferUnlocked(s_error);
    } else {
      s_mode = Mode::StdOut;
    }

    return masterOk;
  }

  static void setFilePath(const std::string &path) {
    std::lock_guard<std::mutex> lock(s_mutex);
    setPathsUnlocked(path);
  }

  static std::string filePath() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_master.path;
  }

  static std::string infoFilePath() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_info.path;
  }

  static std::string warnFilePath() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_warn.path;
  }

  static std::string errorFilePath() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_error.path;
  }

  static void flush() {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (writesToStdOutUnlocked()) {
      std::cout.flush();
    }

    if (!writesToFileUnlocked()) {
      return;
    }

    tryOpenDestinationUnlocked(s_master, true);
    tryOpenDestinationUnlocked(s_info, true);
    tryOpenDestinationUnlocked(s_warn, true);
    tryOpenDestinationUnlocked(s_error, true);

    flushDestinationBufferUnlocked(s_master);
    flushDestinationBufferUnlocked(s_info);
    flushDestinationBufferUnlocked(s_warn);
    flushDestinationBufferUnlocked(s_error);

    if (s_master.stream.is_open())
      s_master.stream.flush();
    if (s_info.stream.is_open())
      s_info.stream.flush();
    if (s_warn.stream.is_open())
      s_warn.stream.flush();
    if (s_error.stream.is_open())
      s_error.stream.flush();
  }

  static void closeFile() {
    std::lock_guard<std::mutex> lock(s_mutex);
    closeDestinationUnlocked(s_master);
    closeDestinationUnlocked(s_info);
    closeDestinationUnlocked(s_warn);
    closeDestinationUnlocked(s_error);
  }

  static bool reopenFile() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!writesToFileUnlocked()) {
      return false;
    }

    const bool masterOk = openDestinationUnlocked(s_master, true);
    openDestinationUnlocked(s_info, true);
    openDestinationUnlocked(s_warn, true);
    openDestinationUnlocked(s_error, true);

    flushDestinationBufferUnlocked(s_master);
    flushDestinationBufferUnlocked(s_info);
    flushDestinationBufferUnlocked(s_warn);
    flushDestinationBufferUnlocked(s_error);

    return masterOk;
  }

  static void setBufferEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_bufferEnabled = enabled;
    if (!s_bufferEnabled) {
      s_master.buffer.clear();
      s_info.buffer.clear();
      s_warn.buffer.clear();
      s_error.buffer.clear();
    }
  }

  static bool bufferEnabled() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_bufferEnabled;
  }

  static void setMaxBufferedEntries(std::size_t maxEntries) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_maxBufferedEntries = (maxEntries == 0 ? 1 : maxEntries);
    trimBufferUnlocked(s_master);
    trimBufferUnlocked(s_info);
    trimBufferUnlocked(s_warn);
    trimBufferUnlocked(s_error);
  }

  static std::size_t maxBufferedEntries() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_maxBufferedEntries;
  }

  static std::size_t bufferedEntryCount() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_master.buffer.size() + s_info.buffer.size() +
           s_warn.buffer.size() + s_error.buffer.size();
  }

  static std::size_t bufferedMasterCount() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_master.buffer.size();
  }

  static std::size_t bufferedInfoCount() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_info.buffer.size();
  }

  static std::size_t bufferedWarnCount() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_warn.buffer.size();
  }

  static std::size_t bufferedErrorCount() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_error.buffer.size();
  }

  static std::string bufferedText() {
    std::lock_guard<std::mutex> lock(s_mutex);
    std::ostringstream oss;
    appendBufferUnlocked(oss, s_master);
    appendBufferUnlocked(oss, s_info);
    appendBufferUnlocked(oss, s_warn);
    appendBufferUnlocked(oss, s_error);
    return oss.str();
  }

  static void clearBuffer() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_master.buffer.clear();
    s_info.buffer.clear();
    s_warn.buffer.clear();
    s_error.buffer.clear();
  }

  static bool flushBufferToFile() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!writesToFileUnlocked()) {
      return false;
    }

    tryOpenDestinationUnlocked(s_master, true);
    tryOpenDestinationUnlocked(s_info, true);
    tryOpenDestinationUnlocked(s_warn, true);
    tryOpenDestinationUnlocked(s_error, true);

    flushDestinationBufferUnlocked(s_master);
    flushDestinationBufferUnlocked(s_info);
    flushDestinationBufferUnlocked(s_warn);
    flushDestinationBufferUnlocked(s_error);

    return s_master.stream.is_open();
  }

  static DateOnly dateNow() { return DateOnly{localTime(nowTimeT())}; }
  static Time24Only time24Now() { return Time24Only{localTime(nowTimeT())}; }
  static Time12Only time12Now() { return Time12Only{localTime(nowTimeT())}; }

  static DateTime24 dateTime24Now() {
    return DateTime24{localTime(nowTimeT())};
  }

  static DateTime12 dateTime12Now() {
    return DateTime12{localTime(nowTimeT())};
  }

  static DateOnly date(const std::chrono::system_clock::time_point &tp) {
    return DateOnly{localTime(std::chrono::system_clock::to_time_t(tp))};
  }

  static Time24Only time24(const std::chrono::system_clock::time_point &tp) {
    return Time24Only{localTime(std::chrono::system_clock::to_time_t(tp))};
  }

  static Time12Only time12(const std::chrono::system_clock::time_point &tp) {
    return Time12Only{localTime(std::chrono::system_clock::to_time_t(tp))};
  }

  static DateTime24
  dateTime24(const std::chrono::system_clock::time_point &tp) {
    return DateTime24{localTime(std::chrono::system_clock::to_time_t(tp))};
  }

  static DateTime12
  dateTime12(const std::chrono::system_clock::time_point &tp) {
    return DateTime12{localTime(std::chrono::system_clock::to_time_t(tp))};
  }

  static DateOnly date(const std::tm &tmValue) { return DateOnly{tmValue}; }
  static Time24Only time24(const std::tm &tmValue) {
    return Time24Only{tmValue};
  }
  static Time12Only time12(const std::tm &tmValue) {
    return Time12Only{tmValue};
  }
  static DateTime24 dateTime24(const std::tm &tmValue) {
    return DateTime24{tmValue};
  }
  static DateTime12 dateTime12(const std::tm &tmValue) {
    return DateTime12{tmValue};
  }

  static Line info() { return Line(Level::Info); }
  static Line warn() { return Line(Level::Warn); }
  static Line error() { return Line(Level::Error); }

private:
  inline static std::mutex s_mutex{};
  inline static Mode s_mode = Mode::StdOut;
  inline static bool s_appendMode = true;
  inline static bool s_bufferEnabled = true;
  inline static std::size_t s_maxBufferedEntries = 2000;

  inline static Destination s_master{LOG_FILE_MSTR};
  inline static Destination s_info{std::string(LOG_FILE_MSTR) + "_info"};
  inline static Destination s_warn{std::string(LOG_FILE_MSTR) + "_warn"};
  inline static Destination s_error{std::string(LOG_FILE_MSTR) + "_error"};

private:
  static bool writesToStdOutUnlocked() {
    return s_mode == Mode::StdOut || s_mode == Mode::StdOutAndFile;
  }

  static bool writesToFileUnlocked() {
    return s_mode == Mode::File || s_mode == Mode::StdOutAndFile;
  }

  static void setPathsUnlocked(const std::string &masterPath) {
    s_master.path = masterPath;
    s_info.path = masterPath + "_info";
    s_warn.path = masterPath + "_warn";
    s_error.path = masterPath + "_error";
  }

  static std::time_t nowTimeT() {
    return std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
  }

  static std::tm localTime(std::time_t t) {
    std::tm out{};
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
  }

  static void closeDestinationUnlocked(Destination &dest) {
    if (dest.stream.is_open()) {
      dest.stream.flush();
      dest.stream.close();
    }
  }

  static bool openDestinationUnlocked(Destination &dest, bool append) {
    closeDestinationUnlocked(dest);
    dest.stream.open(dest.path, append ? std::ios::app : std::ios::trunc);
    return dest.stream.is_open();
  }

  static bool tryOpenDestinationUnlocked(Destination &dest, bool append) {
    if (dest.stream.is_open()) {
      return true;
    }
    return openDestinationUnlocked(dest, append);
  }

  static void trimBufferUnlocked(Destination &dest) {
    while (dest.buffer.size() > s_maxBufferedEntries) {
      dest.buffer.pop_front();
    }
  }

  static void bufferWriteUnlocked(Destination &dest, const std::string &text) {
    if (!s_bufferEnabled) {
      return;
    }

    dest.buffer.push_back(text);
    trimBufferUnlocked(dest);
  }

  static void appendBufferUnlocked(std::ostringstream &oss,
                                   const Destination &dest) {
    for (const auto &entry : dest.buffer) {
      oss << entry;
    }
  }

  static void flushDestinationBufferUnlocked(Destination &dest) {
    if (!dest.stream.is_open()) {
      return;
    }

    while (!dest.buffer.empty()) {
      dest.stream << dest.buffer.front();
      dest.buffer.pop_front();
    }

    dest.stream.flush();
  }

  template <typename T> static std::string toStringValue(const T &value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
  }

  static std::string toStringValue(const DateOnly &value) {
    std::ostringstream oss;
    oss << std::put_time(&value.tmValue, "%m/%d/%Y");
    return oss.str();
  }

  static std::string toStringValue(const Time24Only &value) {
    std::ostringstream oss;
    oss << std::put_time(&value.tmValue, "%H:%M:%S");
    return oss.str();
  }

  static std::string toStringValue(const Time12Only &value) {
    std::ostringstream oss;
    oss << std::put_time(&value.tmValue, "%I:%M:%S %p");
    return oss.str();
  }

  static std::string toStringValue(const DateTime24 &value) {
    std::ostringstream oss;
    oss << std::put_time(&value.tmValue, "%m/%d/%Y %H:%M:%S");
    return oss.str();
  }

  static std::string toStringValue(const DateTime12 &value) {
    std::ostringstream oss;
    oss << std::put_time(&value.tmValue, "%m/%d/%Y %I:%M:%S %p");
    return oss.str();
  }

  template <typename T>
  static void writeToDestinationUnlocked(Destination &dest, const T &value) {
    if (!tryOpenDestinationUnlocked(dest, s_appendMode)) {
      bufferWriteUnlocked(dest, toStringValue(value));
      return;
    }

    flushDestinationBufferUnlocked(dest);
    dest.stream << value;
  }

  static void writeToDestinationUnlocked(Destination &dest,
                                         const DateOnly &value) {
    if (!tryOpenDestinationUnlocked(dest, s_appendMode)) {
      bufferWriteUnlocked(dest, toStringValue(value));
      return;
    }

    flushDestinationBufferUnlocked(dest);
    dest.stream << std::put_time(&value.tmValue, "%m/%d/%Y");
  }

  static void writeToDestinationUnlocked(Destination &dest,
                                         const Time24Only &value) {
    if (!tryOpenDestinationUnlocked(dest, s_appendMode)) {
      bufferWriteUnlocked(dest, toStringValue(value));
      return;
    }

    flushDestinationBufferUnlocked(dest);
    dest.stream << std::put_time(&value.tmValue, "%H:%M:%S");
  }

  static void writeToDestinationUnlocked(Destination &dest,
                                         const Time12Only &value) {
    if (!tryOpenDestinationUnlocked(dest, s_appendMode)) {
      bufferWriteUnlocked(dest, toStringValue(value));
      return;
    }

    flushDestinationBufferUnlocked(dest);
    dest.stream << std::put_time(&value.tmValue, "%I:%M:%S %p");
  }

  static void writeToDestinationUnlocked(Destination &dest,
                                         const DateTime24 &value) {
    if (!tryOpenDestinationUnlocked(dest, s_appendMode)) {
      bufferWriteUnlocked(dest, toStringValue(value));
      return;
    }

    flushDestinationBufferUnlocked(dest);
    dest.stream << std::put_time(&value.tmValue, "%m/%d/%Y %H:%M:%S");
  }

  static void writeToDestinationUnlocked(Destination &dest,
                                         const DateTime12 &value) {
    if (!tryOpenDestinationUnlocked(dest, s_appendMode)) {
      bufferWriteUnlocked(dest, toStringValue(value));
      return;
    }

    flushDestinationBufferUnlocked(dest);
    dest.stream << std::put_time(&value.tmValue, "%m/%d/%Y %I:%M:%S %p");
  }

  template <typename T> static void writeToMaster(const T &value) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (writesToStdOutUnlocked()) {
      std::cout << value;
    }

    if (writesToFileUnlocked()) {
      writeToDestinationUnlocked(s_master, value);
    }
  }

  static void writeToMaster(const DateOnly &value) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (writesToStdOutUnlocked()) {
      std::cout << std::put_time(&value.tmValue, "%m/%d/%Y");
    }

    if (writesToFileUnlocked()) {
      writeToDestinationUnlocked(s_master, value);
    }
  }

  static void writeToMaster(const Time24Only &value) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (writesToStdOutUnlocked()) {
      std::cout << std::put_time(&value.tmValue, "%H:%M:%S");
    }

    if (writesToFileUnlocked()) {
      writeToDestinationUnlocked(s_master, value);
    }
  }

  static void writeToMaster(const Time12Only &value) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (writesToStdOutUnlocked()) {
      std::cout << std::put_time(&value.tmValue, "%I:%M:%S %p");
    }

    if (writesToFileUnlocked()) {
      writeToDestinationUnlocked(s_master, value);
    }
  }

  static void writeToMaster(const DateTime24 &value) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (writesToStdOutUnlocked()) {
      std::cout << std::put_time(&value.tmValue, "%m/%d/%Y %H:%M:%S");
    }

    if (writesToFileUnlocked()) {
      writeToDestinationUnlocked(s_master, value);
    }
  }

  static void writeToMaster(const DateTime12 &value) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (writesToStdOutUnlocked()) {
      std::cout << std::put_time(&value.tmValue, "%m/%d/%Y %I:%M:%S %p");
    }

    if (writesToFileUnlocked()) {
      writeToDestinationUnlocked(s_master, value);
    }
  }

  static void writeManipulatorToMaster(std::ostream &(*manip)(std::ostream &)) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (writesToStdOutUnlocked()) {
      manip(std::cout);
    }

    if (!writesToFileUnlocked()) {
      return;
    }

    if (!tryOpenDestinationUnlocked(s_master, s_appendMode)) {
      if (manip == static_cast<std::ostream &(*)(std::ostream &)>(std::endl) ||
          manip == static_cast<std::ostream &(*)(std::ostream &)>(std::flush) ||
          manip == static_cast<std::ostream &(*)(std::ostream &)>(std::ends)) {
        bufferWriteUnlocked(s_master, "\n");
      }
      return;
    }

    flushDestinationBufferUnlocked(s_master);
    manip(s_master.stream);
  }

  static void writeManipulatorToMaster(std::ios &(*manip)(std::ios &)) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (writesToStdOutUnlocked()) {
      manip(std::cout);
    }

    if (!writesToFileUnlocked()) {
      return;
    }

    if (!tryOpenDestinationUnlocked(s_master, s_appendMode)) {
      return;
    }

    flushDestinationBufferUnlocked(s_master);
    manip(s_master.stream);
  }

  static void
  writeManipulatorToMaster(std::ios_base &(*manip)(std::ios_base &)) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (writesToStdOutUnlocked()) {
      manip(std::cout);
    }

    if (!writesToFileUnlocked()) {
      return;
    }

    if (!tryOpenDestinationUnlocked(s_master, s_appendMode)) {
      return;
    }

    flushDestinationBufferUnlocked(s_master);
    manip(s_master.stream);
  }

  static Destination &destinationForLevelUnlocked(Level level) {
    switch (level) {
    case Level::Info:
      return s_info;
    case Level::Warn:
      return s_warn;
    case Level::Error:
      return s_error;
    }
    return s_info;
  }

  static const char *levelTag(Level level) {
    switch (level) {
    case Level::Info:
      return "[INFO] ";
    case Level::Warn:
      return "[WARN] ";
    case Level::Error:
      return "[ERROR] ";
    }
    return "";
  }

  static void writeLeveledLine(Level level, const std::string &text) {
    std::lock_guard<std::mutex> lock(s_mutex);

    const std::string finalText = std::string(levelTag(level)) + text;

    if (writesToStdOutUnlocked()) {
      std::cout << finalText;
    }

    if (writesToFileUnlocked()) {
      writeRawTextUnlocked(s_master, finalText);
      writeRawTextUnlocked(destinationForLevelUnlocked(level), finalText);
    }
  }

  static void writeRawTextUnlocked(Destination &dest, const std::string &text) {
    if (!tryOpenDestinationUnlocked(dest, s_appendMode)) {
      bufferWriteUnlocked(dest, text);
      return;
    }

    flushDestinationBufferUnlocked(dest);
    dest.stream << text;
    dest.stream.flush();
  }
};

#define LOG_INFO() Logger::info()
#define LOG_WARN() Logger::warn()
#define LOG_ERROR() Logger::error()
