#pragma once

#include <cairomm/cairomm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "keypad.h"

class ColorWheelPicker : public Gtk::Box {
public:
  struct RGB {
    int r{255}, g{255}, b{255};
  };
  void set_header_text(const std::string &text);
  void commit_pending();
  ColorWheelPicker(std::string PATH, const std::string &headerText, int red,
                   int green, int blue, int wheel_diameter = 220,
                   int bar_width = 26,
                   int keypad_pixel_size = KEY_PAD_PIXEL_SIZE);
  ~ColorWheelPicker() override = default;

  RGB get_rgb() const;
  int get_r() const;
  int get_g() const;
  int get_b() const;

  Gdk::RGBA get_rgba() const;

  void set_rgb(int r, int g, int b);
  void set_rgb(const RGB &c);
  void set_r(int r);
  void set_g(int g);
  void set_b(int b);

  sigc::signal<void, int, int, int> &signal_color_changed();

private:
  Gtk::Label m_headerLbl;
  Gtk::Stack m_stack;
  Gtk::Box m_pickerRoot{Gtk::ORIENTATION_VERTICAL};
  KeyPad *m_keypad{nullptr};

  int m_keypadPixelSize{KEY_PAD_PIXEL_SIZE};
  Gtk::Entry *m_activeEntry{nullptr};

  Gtk::Box m_mainRow;
  Gtk::Box m_leftCol;
  Gtk::DrawingArea m_wheel;
  Gtk::DrawingArea m_bar;

  Gtk::Box m_rightCol;
  Gtk::Grid m_grid;
  Gtk::Label m_lblR, m_lblG, m_lblB;
  Gtk::Entry m_entryR, m_entryG, m_entryB;

  int m_wheelDiameter;
  int m_barWidth;

  Cairo::RefPtr<Cairo::ImageSurface> m_wheelSurface;
  bool m_wheelDirty{true};

  double m_h{0.0};
  double m_s{0.0};
  double m_v{1.0};

  int m_r{255}, m_g{255}, m_b{255};

  bool m_dragWheel{false};
  bool m_dragBar{false};
  bool m_updatingEntries{false};

  sigc::signal<void, int, int, int> m_signalColorChanged;

  bool on_entry_focus_in(std::string PATH, GdkEventFocus *e, Gtk::Entry *which);
  void show_keypad_for(std::string PATH, Gtk::Entry *which);
  void hide_keypad_and_apply(int v);

  void set_entry_value(Gtk::Entry &entry, int v);

  void ensure_wheel_surface();
  bool on_wheel_draw(const Cairo::RefPtr<Cairo::Context> &cr);
  bool on_bar_draw(const Cairo::RefPtr<Cairo::Context> &cr);

  bool on_wheel_button_press(GdkEventButton *e);
  bool on_wheel_button_release(GdkEventButton *e);
  bool on_wheel_motion(GdkEventMotion *e);

  bool on_bar_button_press(GdkEventButton *e);
  bool on_bar_button_release(GdkEventButton *e);
  bool on_bar_motion(GdkEventMotion *e);

  void setup_entry(Gtk::Entry &entry);
  void update_entries_from_rgb();
  void on_entry_changed();

  static int clampi(int v, int lo, int hi);
  static double clampd(double v, double lo, double hi);

  static void hsv_to_rgb(double h, double s, double v, int &r, int &g, int &b);
  static void rgb_to_hsv(int r, int g, int b, double &h, double &s, double &v);

  enum class PendingType { None, HSV, RGB };

  int m_throttleMs{16};
  sigc::connection m_throttleConn;
  PendingType m_pendingType{PendingType::None};
  bool m_pendingEmit{false};

  double m_pH{0.0}, m_pS{0.0}, m_pV{1.0};
  int m_pR{255}, m_pG{255}, m_pB{255};

  void request_apply_hsv(bool emit_signal);
  void request_apply_rgb(bool emit_signal);

  bool on_throttle_tick();
  void flush_pending();
  void flush_pending_now();
  void cancel_throttle();

  void apply_hsv_now(bool emit_signal);
  void apply_rgb_now(bool emit_signal);

  void set_from_wheel_point(double x, double y, bool emit_signal);
  void set_from_bar_point(double x, bool emit_signal);

  void notify_change(bool emit_signal);
};
