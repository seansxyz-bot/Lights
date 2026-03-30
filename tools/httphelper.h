#pragma once

#include "glibmm.h"
#include "gtkmm.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "logger.h"

#if (UBUNTU == 1)
#define DEVICE "dev-test"
#else
#define DEVICE "pi5-main"
#endif

using json = nlohmann::json;

struct LEDData {
  Glib::ustring name;
  int group;
  int redPin;
  int grnPin;
  int bluPin;
  int redVal;
  int grnVal;
  int bluVal;

  bool operator==(const LEDData &o) const {
    return name == o.name && group == o.group && redPin == o.redPin &&
           grnPin == o.grnPin && bluPin == o.bluPin && redVal == o.redVal &&
           grnVal == o.grnVal && bluVal == o.bluVal;
  }

  bool operator!=(const LEDData &o) const { return !(*this == o); }
};

struct Options {
  int sensor;
  int on;
  int theme;
  int ptrn;

  bool operator==(const Options &o) const {
    return sensor == o.sensor && on == o.on && theme == o.theme &&
           ptrn == o.ptrn;
  }

  bool operator!=(const Options &o) const { return !(*this == o); }
};

struct Schedule {
  std::string name;
  int themeID;
  int enabled;
  std::string sDate;
  std::string sTime;
  std::string eDate;
  std::string eTime;
  bool operator==(const Schedule &o) const {
    return name == o.name && themeID == o.themeID && enabled == o.enabled &&
           sDate == o.sDate && sTime == o.sTime && eDate == o.eDate &&
           eTime == o.eTime;
  }

  bool operator!=(const Schedule &o) const { return !(*this == o); }
};

class HttpHelper {
public:
  HttpHelper();
  ~HttpHelper();

  std::string get(const std::string &url) const;
  std::vector<unsigned char> getBytes(const std::string &url) const; // NEW
  bool downloadToFile(const std::string &url,
                      const std::string &filePath) const; // NEW

  std::string postJson(const std::string &url, const json &payload) const;

  static json toJson(const LEDData &led);
  static json toJson(const Options &opt);
  static json toJson(const Schedule &sch);

  static json toJson(const std::vector<LEDData> &leds);
  static json toJson(const std::vector<Schedule> &schedules);

  std::string sendLED(const std::string &url, const LEDData &led,
                      const std::string &device = "") const;

  std::string sendOptions(const std::string &url, const Options &opt,
                          const std::string &device = "") const;

  std::string sendSchedule(const std::string &url, const Schedule &sch,
                           const std::string &device = "") const;

  std::string sendLEDs(const std::string &url, const std::vector<LEDData> &leds,
                       const std::string &device = "") const;

  std::string sendSchedules(const std::string &url,
                            const std::vector<Schedule> &schedules,
                            const std::string &device = "") const;

  std::string sendAll(const std::string &url, const Options &opt,
                      const std::vector<LEDData> &leds,
                      const std::vector<Schedule> &schedules,
                      const std::string &device = "") const;

  void sendOptionsAsync(const std::string &url, Options opt,
                        std::string device = "") const;
  void sendLEDAsync(const std::string &url, LEDData led,
                    std::string device = "") const;
  void sendScheduleAsync(const std::string &url, Schedule sch,
                         std::string device = "") const;
  void sendLEDsAsync(const std::string &url, std::vector<LEDData> leds,
                     std::string device = "") const;
  void sendSchedulesAsync(const std::string &url,
                          std::vector<Schedule> schedules,
                          std::string device = "") const;
  void sendAllAsync(const std::string &url, Options opt,
                    std::vector<LEDData> leds, std::vector<Schedule> schedules,
                    std::string device = "") const;

private:
  static size_t writeCallback(void *contents, size_t size, size_t nmemb,
                              void *userp);

  static size_t writeVectorCallback(void *contents, size_t size, size_t nmemb,
                                    void *userp); // NEW
};
