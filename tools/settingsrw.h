#pragma once

#include "httphelper.h"

#if (UBUNTU == 1)
#define HOME_DIR "/home/dev"
#define SETTINGS_PATH "/home/dev/.lightcontroller"
#else
#define HOME_DIR "/home/lights"
#define SETTINGS_PATH "/home/lights/.lightcontroller"
#endif

#define KRAKEN_COLOR_1 "#001628"
#define KRAKEN_COLOR_2 "#99D9D9"
#define SEAHAWKS_COLOR_1 "#002244"
#define SEAHAWKS_COLOR_2 "#69BE28"
#define MARINERS_COLOR_1 "#0C2C56"
#define MARINERS_COLOR_2 "#005C5C"

#define KRAKEN_FILE "/kraken"
#define MARINERS_FILE "/mariners"
#define SEAHAWKS_FILE "/seahawks"

#define NUM_OF_LEDS 19

#include "fstream"
#include "iostream"
#include <atomic>
#include <gdk/gdk.h>
#include <sstream>
#include <vector>

extern HttpHelper http;
extern std::atomic<bool> writeToServer;

struct RGB_Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct Theme {
  std::string name;
  int id;
  std::vector<RGB_Color> colors;
};

struct TeamInfo {
  std::string name;
  Glib::RefPtr<Gdk::Pixbuf> logo;
  Gdk::RGBA c1;
  Gdk::RGBA c2;

  TeamInfo(std::string path, std::string fileName) {
    std::string fullPath = path + fileName;
    // Attempt to open the file for reading
    std::vector<TeamInfo> data;
    std::ifstream file(fullPath);
    file.is_open();
    // Read the file line by line
    logo =
        Gdk::Pixbuf::create_from_file(path + "/icons" + fileName + "_logo.png");
    if (fileName == KRAKEN_FILE) {
      name = "KRAKEN";
      c1.set(KRAKEN_COLOR_1);
      c2.set(KRAKEN_COLOR_2);
    } else if (fileName == "/seahawks") {
      name = "SEAHAWKS";
      c1.set(SEAHAWKS_COLOR_1);
      c2.set(SEAHAWKS_COLOR_2);
    } else {
      name = "MARINERS";
      c1.set(MARINERS_COLOR_1);
      c2.set(MARINERS_COLOR_2);
    }

    std::cout << "LOGO: " << path + "/icons" + fileName + "_logo.png"
              << std::endl;
    std::cout << "color1: " << c1.get_red() * 255 << ", "
              << c1.get_green() * 255 << "," << c1.get_blue() * 255
              << std::endl;
    std::cout << "color2: " << c2.get_red() * 255 << ", "
              << c2.get_green() * 255 << "," << c2.get_blue() * 255
              << std::endl;
  }
};

struct GameInfo {
  int id;
  std::string gameState;

  std::string home;
  std::string away;
  std::string militaryTime;      // 24 Hour
  std::string standardTime;      // 12 Hour
  std::string displayedDateTime; // MM-DD @ HH:MM am/pm
  std::string dateTimeUTC;       // orignal time string
  std::string scheduledDate;     // MM/DD

  std::string scoreHome;
  std::string scoreAway;
};

std::vector<LEDData> readLEDInfo(std::string path);
int writeLEDInfo(std::string path, std::vector<LEDData> data);

Options readOptions(std::string path);
int writeOptions(std::string path, Options data);

std::vector<Schedule> readSchedule(std::string path);
int writeSchedule(std::string path, std::vector<Schedule> data);

GameInfo readNextGame(std::string path, std::string fileName);
int writeNextGame(std::string path, std::string fileName, GameInfo data);

std::vector<Theme> readThemeColors(const std::string &dbPath);
int writeThemeColors(const std::string &dbPath,
                     const std::vector<Theme> &themes);
