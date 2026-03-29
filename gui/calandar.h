#pragma once

#include <array>
#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <tuple>

class Calandar : public Gtk::Box {
public:
  std::tuple<int, int, int> getEasterDate(int year) {
    int a = year % 19;
    int b = year / 100;
    int c = year % 100;
    int d = b / 4;
    int e = b % 4;
    int f = (b + 8) / 25;
    int g = (b - f + 1) / 3;
    int h = (19 * a + b - d - g + 15) % 30;
    int i = c / 4;
    int k = c % 4;
    int l = (32 + 2 * e + 2 * i - h - k) % 7;
    int m = (a + 11 * h + 22 * l) / 451;

    int month = (h + l - 7 * m + 114) / 31; // 3 = March, 4 = April
    int day = ((h + l - 7 * m + 114) % 31) + 1;

    return {year, month, day};
  }

  Calandar();

  // Match the bits of Gtk::Calendar you already use
  void select_month(guint month, guint year); // month = 0..11
  void select_day(guint day);                 // day = 1..31
  void get_date(guint &year, guint &month, guint &day) const;

  sigc::signal<void> &signal_day_selected();

private:
  struct DayCell {
    Gtk::Button *button = nullptr;
    Gtk::Label *label = nullptr;
    Gtk::Image *selbg = nullptr;

    int year = 0;
    int month = 0;
    int day = 0;
    bool in_current_month = false;
  };

private:
  void build_ui();
  void refresh();
  void refresh_header();
  void refresh_grid();

  void on_prev_month();
  void on_next_month();
  void on_day_clicked(int index);

  static bool is_leap_year(int year);
  static int days_in_month(int year, int month);          // month = 1..12
  static int first_weekday_of_month(int year, int month); // 0=Sun .. 6=Sat
  static std::string month_name(int month);               // month = 1..12

private:
  sigc::signal<void> m_signalDaySelected;

  Gtk::Box m_headerBox;
  Gtk::Button m_btnPrev;
  Gtk::Label m_lblHeader;
  Gtk::Button m_btnNext;

  Gtk::Grid m_grid;

  std::array<Gtk::Label *, 7> m_dowLabels{};
  std::array<DayCell, 42> m_cells{};

  int m_year = 2026;
  int m_month = 1; // 1..12
  int m_day = 1;   // selected day

  Glib::RefPtr<Gtk::CssProvider> m_css;
};
