#pragma once

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "../../models/types.h"
#include "../../utils/logger.h"

#if (UBUNTU == 1)
#define DEVICE "dev-test"
#else
#define DEVICE "pi5-main"
#endif

using json = nlohmann::json;

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
