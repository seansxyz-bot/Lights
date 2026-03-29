#pragma once

#include "../threads/clockthread.h"
#include "../tools/logger.h"
#include <functional>
#include <gtkmm.h>
#include <mutex>
#include <string>

class ClockScreen : public Gtk::EventBox {
public:
  ClockScreen();
  virtual ~ClockScreen();

  void start();
  void stop();

  void setTempHumidity(float temp_f, float humidity);
  void setEnvProvider(const std::function<bool(float &, float &)> &provider);

  void setColors(const Gdk::RGBA &bg, const Gdk::RGBA &fg);

  sigc::signal<void> &signal_dismiss_requested();

protected:
  bool on_button_press_event(GdkEventButton *event) override;
  void on_size_allocate(Gtk::Allocation &allocation) override;

private:
  sigc::connection m_clockTickConn;
  bool m_started{false};
  // ---------- Root / movable container ----------
  Gtk::Fixed m_rootFixed;
  Gtk::Grid m_grid;
  Gtk::Fixed m_clockCell;

  // ---------- UI ----------
  Gtk::Label m_lblDate;
  Gtk::Label m_lblEnv;
  Gtk::Label m_lblClock;

  // ---------- Per-widget CSS ----------
  Glib::RefPtr<Gtk::CssProvider> m_cssDate;
  Glib::RefPtr<Gtk::CssProvider> m_cssEnv;
  Glib::RefPtr<Gtk::CssProvider> m_cssClock;

  // ---------- Worker / dispatch ----------
  Glib::Dispatcher m_dispatcher;

  std::mutex m_dataMutex;
  std::string m_pendingDate;
  std::string m_pendingEnv;
  std::string m_pendingClock;

  // ---------- Timers ----------
  sigc::connection m_shiftVertTimerConn;
  sigc::connection m_shiftHorizTimerConn;
  sigc::connection m_colorTimerConn;
  sigc::connection m_envTimerConn;

  // ---------- External sensor callback ----------
  std::function<bool(float &, float &)> m_envProvider;

  // ---------- Colors ----------
  Gdk::RGBA m_bgColor;
  Gdk::RGBA m_fgColor;
  bool m_inverted{false};

  // ---------- Burn-in drift ----------
  int m_offsetX{0};
  int m_offsetY{0};
  bool m_moveDown{true};
  bool m_moveRight{true};

  // ---------- Manual font sizes ----------
  int m_dateFontPx{56};
  int m_envFontPx{56};
  int m_clockFontPx{450};
  int m_clockNudgeY{-40};

  // ---------- Signals ----------
  sigc::signal<void> m_signalDismissRequested;

private:
  bool getWidgetTopBottom(const Gtk::Widget &widget, int &top,
                          int &bottom) const;
  bool getWidgetLeftRight(const Gtk::Widget &widget, int &left,
                          int &right) const;

  void onClockDispatcher();
  void applyPendingText();

  void refreshEnvFromProvider();

  std::string formatEnv(float temp_f, float humidity) const;

  void layoutGrid();
  bool shiftVertical();
  bool shiftHorizontal();

  void applyColors();
  void setLabelCss(Gtk::Label &label,
                   const Glib::RefPtr<Gtk::CssProvider> &provider, int px,
                   int weight);
};
