#pragma once

#include <gdkmm/pixbuf.h>
#include <gdkmm/rgba.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <string>
#include <vector>

struct RGB_Color {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

struct Theme {
  std::string name;
  int id = 0;
  std::vector<RGB_Color> colors;
};

struct TeamRecord {
  int id = 0;
  std::string name;
  std::string league;
  std::string teamCode;
  std::string nextGameUrlTemplate;
  std::string nextGameParser;
  std::string liveGameUrlTemplate;
  std::string liveGameParser;
  std::string apiTeamId;
  int enabled = 1;
  int displayOrder = 0;
  std::string themeName;
  std::string nextGameUtc;
  Glib::RefPtr<Gdk::Pixbuf> logo;
  Gdk::RGBA c1;
  Gdk::RGBA c2;
};

struct TeamStats {
  int ranking = 0;
  int divisionRank = 0;
  int conferenceRank = 0;
  int wins = 0;
  int losses = 0;
  int draws = 0;
  int overtimeLosses = 0;
  int points = 0;
  double winPct = 0.0;
  std::string conference;
  std::string division;
  std::string streak;
  std::string seasonLabel;
  std::string recordText;
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
  std::string venue;
  std::string scoreHome;
  std::string scoreAway;

  bool isHomeGame = false;
  bool isPlayoffGame = false;

  std::string opponentCode;
  std::string opponentName;
};
