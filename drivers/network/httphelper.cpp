#include "httphelper.h"

#include <iostream>

namespace {
bool expectsApiSuccessFlag(const std::string &url) {
  return url.find("/lights_api/") != std::string::npos;
}
} // namespace

HttpHelper::HttpHelper() { curl_global_init(CURL_GLOBAL_DEFAULT); }

HttpHelper::~HttpHelper() { curl_global_cleanup(); }

json HttpHelper::toJson(const LEDData &led) {
  return {{"name", std::string(led.name)},
          {"led_name", std::string(led.name)},
          {"group", led.group},
          {"led_group", led.group},
          {"redPin", led.redPin},
          {"red_pin", led.redPin},
          {"pin_red", led.redPin},
          {"grnPin", led.grnPin},
          {"grn_pin", led.grnPin},
          {"pin_grn", led.grnPin},
          {"bluPin", led.bluPin},
          {"blu_pin", led.bluPin},
          {"pin_blu", led.bluPin},
          {"redVal", led.redVal},
          {"red_value", led.redVal},
          {"grnVal", led.grnVal},
          {"grn_value", led.grnVal},
          {"bluVal", led.bluVal},
          {"blu_value", led.bluVal}};
}

json HttpHelper::toJson(const Options &opt) {
  return {{"auto", opt.sensor},
          {"sensor", opt.sensor},
          {"on", opt.on},
          {"theme", opt.theme},
          {"pattern", opt.ptrn},
          {"ptrn", opt.ptrn},
          {"bluetooth", opt.bluetooth}};
}

json HttpHelper::toJson(const Schedule &sch) {
  return {{"name", sch.name},
          {"theme_name", sch.name},
          {"themeID", sch.themeID},
          {"theme_id", sch.themeID},
          {"enabled", sch.enabled},
          {"theme_enabled", sch.enabled},
          {"sDate", sch.sDate},
          {"start_date", sch.sDate},
          {"sTime", sch.sTime},
          {"start_time", sch.sTime},
          {"eDate", sch.eDate},
          {"end_date", sch.eDate},
          {"eTime", sch.eTime},
          {"end_time", sch.eTime}};
}

json HttpHelper::toJson(const Theme &theme) {
  json colors = json::array();
  for (int i = 0; i < static_cast<int>(theme.colors.size()); ++i) {
    const auto &c = theme.colors[i];
    colors.push_back({{"color_index", i}, {"r", c.r}, {"g", c.g}, {"b", c.b}});
  }
  return {{"theme_id", theme.id},
          {"id", theme.id},
          {"name", theme.name},
          {"fileName", theme.fileName},
          {"filename", theme.fileName},
          {"colors", colors}};
}

json HttpHelper::toJson(const Pattern &pattern) {
  return {{"id", pattern.id}, {"speed", pattern.speed}};
}

json HttpHelper::toJson(const TeamRecord &team) {
  json colors = json::array();
  for (const auto &color : team.colors) {
    colors.push_back({{"id", color.id},
                      {"team_id", color.teamId},
                      {"color_role", color.colorRole},
                      {"r", color.r},
                      {"g", color.g},
                      {"b", color.b},
                      {"display_order", color.displayOrder}});
  }
  return {{"id", team.id},
          {"name", team.name},
          {"league", team.league},
          {"team_code", team.teamCode},
          {"home_away", team.homeAway},
          {"next_game_url_template", team.nextGameUrlTemplate},
          {"next_game_parser", team.nextGameParser},
          {"live_game_url_template", team.liveGameUrlTemplate},
          {"live_game_parser", team.liveGameParser},
          {"api_team_id", team.apiTeamId},
          {"enabled", team.enabled},
          {"display_order", team.displayOrder},
          {"theme_name", team.themeName},
          {"theme_id", team.themeID},
          {"icon_path", team.iconPath},
          {"next_game_utc", team.nextGameUtc},
          {"last_home_score", team.lastHomeScore},
          {"last_away_score", team.lastAwayScore},
          {"score_animation_delay_seconds", team.scoreAnimationDelaySeconds},
          {"last_game_id", team.lastGameId},
          {"last_checked_utc", team.lastCheckedUtc},
          {"next_opponent_code", team.nextOpponentCode},
          {"next_opponent_name", team.nextOpponentName},
          {"colors", colors}};
}

json HttpHelper::toJson(const std::vector<LEDData> &leds) {
  json arr = json::array();
  for (const auto &led : leds) {
    arr.push_back(HttpHelper::toJson(led));
  }
  return arr;
}

json HttpHelper::toJson(const std::vector<Schedule> &schedules) {
  json arr = json::array();
  for (const auto &sch : schedules) {
    arr.push_back(HttpHelper::toJson(sch));
  }
  return arr;
}

json HttpHelper::toJson(const std::vector<Theme> &themes) {
  json arr = json::array();
  for (const auto &theme : themes) {
    arr.push_back(HttpHelper::toJson(theme));
  }
  return arr;
}

json HttpHelper::toJson(const std::vector<Pattern> &patterns) {
  json arr = json::array();
  for (const auto &pattern : patterns) {
    arr.push_back(HttpHelper::toJson(pattern));
  }
  return arr;
}

json HttpHelper::toJson(const std::vector<TeamRecord> &teams) {
  json arr = json::array();
  for (const auto &team : teams) {
    arr.push_back(HttpHelper::toJson(team));
  }
  return arr;
}

size_t HttpHelper::writeCallback(void *contents, size_t size, size_t nmemb,
                                 void *userp) {
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}

std::string HttpHelper::get(const std::string &url) const {
  CURL *curl = curl_easy_init();
  std::string response;

  if (!curl) {
    std::cerr << "curl_easy_init() failed\n";
    return response;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpHelper::writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "LightingControl/1.0");
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

  CURLcode res = curl_easy_perform(curl);
  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

  if (res != CURLE_OK) {
    LOG_ERROR() << "HTTP GET failed url=" << url
                << " error=" << curl_easy_strerror(res);
    response.clear();
  } else if (httpCode != 200) {
    LOG_ERROR() << "HTTP GET failed url=" << url << " status=" << httpCode;
    response.clear();
  } else if (expectsApiSuccessFlag(url)) {
    try {
      const json parsed = json::parse(response);
      if (!parsed.is_object() || !parsed.value("success", false)) {
        LOG_ERROR() << "HTTP GET rejected by API url=" << url
                    << " body=" << response;
        response.clear();
      }
    } catch (const std::exception &e) {
      LOG_ERROR() << "HTTP GET returned invalid JSON url=" << url
                  << " error=" << e.what() << " body=" << response;
      response.clear();
    }
  }

  curl_easy_cleanup(curl);
  return response;
}

std::string HttpHelper::postJson(const std::string &url,
                                 const json &payload) const {
  CURL *curl = curl_easy_init();
  std::string response;

  if (!curl) {
    std::cerr << "curl_easy_init() failed\n";
    return response;
  }

  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  std::string body = payload.dump();

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpHelper::writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "LightingControl/1.0");
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
  char errbuf[CURL_ERROR_SIZE] = {0};
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  CURLcode res = curl_easy_perform(curl);
  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
  if (res != CURLE_OK) {
    LOG_ERROR() << "HTTP POST failed url=" << url
                << " error=" << curl_easy_strerror(res)
                << (errbuf[0] != '\0' ? std::string(" detail=") + errbuf
                                      : std::string());
    response.clear();
  } else if (httpCode != 200) {
    LOG_ERROR() << "HTTP POST failed url=" << url << " status=" << httpCode
                << " body=" << response;
    response.clear();
  } else {
    try {
      const json parsed = json::parse(response);
      if (!parsed.is_object() || !parsed.value("success", false)) {
        LOG_ERROR() << "HTTP POST rejected by API url=" << url
                    << " body=" << response;
        response.clear();
      }
    } catch (const std::exception &e) {
      LOG_ERROR() << "HTTP POST returned invalid JSON url=" << url
                  << " error=" << e.what() << " body=" << response;
      response.clear();
    }
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return response;
}

std::string HttpHelper::sendLED(const std::string &url, const LEDData &led,
                                const std::string &device) const {
  json payload;
  if (!device.empty()) {
    payload["device"] = device;
  }
  payload["leds"] = json::array({HttpHelper::toJson(led)});
  return postJson(url, payload);
}

std::string HttpHelper::sendOptions(const std::string &url, const Options &opt,
                                    const std::string &device) const {
  json payload;
  if (!device.empty()) {
    payload["device"] = device;
  }
  payload["options"] = HttpHelper::toJson(opt);
  payload["data"] = json::array(
      {{{"name", "auto"}, {"value", opt.sensor}},
       {{"name", "on"}, {"value", opt.on}},
       {{"name", "theme"}, {"value", opt.theme}},
       {{"name", "ptrn"}, {"value", opt.ptrn}},
       {{"name", "bluetooth"}, {"value", opt.bluetooth}}});
  return postJson(url, payload);
}

std::string HttpHelper::sendSchedule(const std::string &url,
                                     const Schedule &sch,
                                     const std::string &device) const {
  json payload;
  if (!device.empty()) {
    payload["device"] = device;
  }
  payload["schedules"] = json::array({HttpHelper::toJson(sch)});
  return postJson(url, payload);
}

std::string HttpHelper::sendLEDs(const std::string &url,
                                 const std::vector<LEDData> &leds,
                                 const std::string &device) const {
  json payload;
  if (!device.empty()) {
    payload["device"] = device;
  }
  payload["leds"] = HttpHelper::toJson(leds);
  return postJson(url, payload);
}

std::string HttpHelper::sendSchedules(const std::string &url,
                                      const std::vector<Schedule> &schedules,
                                      const std::string &device) const {
  json payload;
  if (!device.empty()) {
    payload["device"] = device;
  }
  payload["schedules"] = HttpHelper::toJson(schedules);
  return postJson(url, payload);
}

std::string HttpHelper::sendThemes(const std::string &url,
                                   const std::vector<Theme> &themes,
                                   const std::string &device) const {
  json payload;
  if (!device.empty()) {
    payload["device"] = device;
  }
  payload["themes"] = HttpHelper::toJson(themes);
  return postJson(url, payload);
}

std::string HttpHelper::sendPatterns(const std::string &url,
                                     const std::vector<Pattern> &patterns,
                                     const std::string &device) const {
  json payload;
  if (!device.empty()) {
    payload["device"] = device;
  }
  payload["patterns"] = HttpHelper::toJson(patterns);
  return postJson(url, payload);
}

std::string HttpHelper::sendTeams(const std::string &url,
                                  const std::vector<TeamRecord> &teams,
                                  const std::string &device) const {
  json payload;
  if (!device.empty()) {
    payload["device"] = device;
  }
  payload["teams"] = HttpHelper::toJson(teams);
  return postJson(url, payload);
}

std::string HttpHelper::sendAll(const std::string &url, const Options &opt,
                                const std::vector<LEDData> &leds,
                                const std::vector<Schedule> &schedules,
                                const std::string &device) const {
  json payload;
  if (!device.empty()) {
    payload["device"] = device;
  }
  payload["options"] = HttpHelper::toJson(opt);
  payload["leds"] = HttpHelper::toJson(leds);
  payload["schedules"] = HttpHelper::toJson(schedules);

  return postJson(url, payload);
}
void HttpHelper::sendOptionsAsync(const std::string &url, Options opt,
                                  std::string device) const {
  std::thread([url, opt, device]() {
    HttpHelper http;
    auto response = http.sendOptions(url, opt, device);
    LOG_INFO() << "SYNC RESPONSE: " << response;
  }).detach();
}

void HttpHelper::sendLEDAsync(const std::string &url, LEDData led,
                              std::string device) const {
  std::thread([url, led, device]() {
    HttpHelper http;
    auto response = http.sendLED(url, led, device);
    LOG_INFO() << "SYNC RESPONSE: " << response;
  }).detach();
}

void HttpHelper::sendScheduleAsync(const std::string &url, Schedule sch,
                                   std::string device) const {
  std::thread([url, sch, device]() {
    HttpHelper http;
    auto response = http.sendSchedule(url, sch, device);
    LOG_INFO() << "SYNC RESPONSE: " << response;
  }).detach();
}

void HttpHelper::sendLEDsAsync(const std::string &url,
                               std::vector<LEDData> leds,
                               std::string device) const {
  std::thread([url, leds, device]() {
    HttpHelper http;
    auto response = http.sendLEDs(url, leds, device);
    LOG_INFO() << "SYNC RESPONSE: " << response;
  }).detach();
}

void HttpHelper::sendSchedulesAsync(const std::string &url,
                                    std::vector<Schedule> schedules,
                                    std::string device) const {
  std::thread([url, schedules, device]() {
    HttpHelper http;
    auto response = http.sendSchedules(url, schedules, device);
    LOG_INFO() << "SYNC RESPONSE: " << response;
  }).detach();
}

void HttpHelper::sendAllAsync(const std::string &url, Options opt,
                              std::vector<LEDData> leds,
                              std::vector<Schedule> schedules,
                              std::string device) const {
  std::thread([url, opt, leds, schedules, device]() {
    HttpHelper http;
    auto response = http.sendAll(url, opt, leds, schedules, device);
    LOG_INFO() << "SYNC RESPONSE: " << response;
  }).detach();
}

size_t HttpHelper::writeVectorCallback(void *contents, size_t size,
                                       size_t nmemb, void *userp) {
  const size_t totalSize = size * nmemb;
  auto *buffer = static_cast<std::vector<unsigned char> *>(userp);
  auto *data = static_cast<unsigned char *>(contents);
  buffer->insert(buffer->end(), data, data + totalSize);
  return totalSize;
}

std::vector<unsigned char> HttpHelper::getBytes(const std::string &url) const {
  CURL *curl = curl_easy_init();
  std::vector<unsigned char> response;

  if (!curl) {
    std::cerr << "curl_easy_init() failed\n";
    return response;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                   HttpHelper::writeVectorCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "LightingControl/1.0");
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res)
              << std::endl;
    response.clear();
  } else {
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (httpCode < 200 || httpCode >= 300) {
      std::cerr << "HTTP GET failed with code " << httpCode << std::endl;
      response.clear();
    }
  }

  curl_easy_cleanup(curl);
  return response;
}

bool HttpHelper::downloadToFile(const std::string &url,
                                const std::string &filePath) const {
  const auto data = getBytes(url);
  if (data.empty())
    return false;

  std::ofstream out(filePath, std::ios::binary);
  if (!out.is_open()) {
    std::cerr << "Failed to open file for writing: " << filePath << std::endl;
    return false;
  }

  out.write(reinterpret_cast<const char *>(data.data()),
            static_cast<std::streamsize>(data.size()));

  const bool ok = out.good();
  out.close();
  return ok;
}
