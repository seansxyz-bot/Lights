#pragma once

#include "imgbutton.h"
#include <functional>
#include <gtkmm.h>
#include <string>

class KeyPad : public Gtk::Box {
public:
  explicit KeyPad(std::string PATH, int start_value = 0, int pixel_size = 96,
                  int max_value = 255)
      : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_path(std::move(PATH)),
        m_pixel_size(pixel_size), m_maxValue(max_value) {

    set_spacing(10);

    m_display.set_halign(Gtk::ALIGN_CENTER);
    m_display.set_valign(Gtk::ALIGN_CENTER);
    m_display.set_margin_bottom(6);

    Pango::FontDescription fd;
    fd.set_family("Sans");
    fd.set_weight(Pango::WEIGHT_BOLD);
    fd.set_size(36 * PANGO_SCALE);
    m_display.override_font(fd);

    pack_start(m_display, Gtk::PACK_SHRINK);

    set_value(start_value);

    add_digit_row({1, 2, 3});
    add_digit_row({4, 5, 6});
    add_digit_row({7, 8, 9});

    auto rowBottom = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    rowBottom->set_spacing(10);
    rowBottom->set_halign(Gtk::ALIGN_CENTER);

    rowBottom->pack_start(
        *make_action_button("bs", [this]() { on_backspace(); }),
        Gtk::PACK_SHRINK);

    rowBottom->pack_start(*make_digit_button(0), Gtk::PACK_SHRINK);

    rowBottom->pack_start(*make_action_button("ok", [this]() { on_ok(); }),
                          Gtk::PACK_SHRINK);

    pack_start(*rowBottom, Gtk::PACK_SHRINK);

    show_all_children();
    set_halign(Gtk::ALIGN_CENTER);
    set_valign(Gtk::ALIGN_CENTER);
  }

  sigc::signal<void, int> &signal_ok_pressed() { return m_signal_ok_pressed; }

  int value() const { return m_value; }

  void set_max_value(int max_value) {
    if (max_value < 0)
      max_value = 0;
    m_maxValue = max_value;
    set_value(m_value);
  }

  int max_value() const { return m_maxValue; }

  void set_value(int v) {
    m_value = clamp_to_max(v);
    m_buffer = std::to_string(m_value);
    update_display();
  }

  void set_on_digit(std::function<void(int)> cb) { m_onDigit = std::move(cb); }
  void set_on_backspace(std::function<void()> cb) {
    m_onBackspace = std::move(cb);
  }
  void set_on_ok(std::function<void(int)> cb) { m_onOk = std::move(cb); }

private:
  static constexpr size_t MAX_DIGITS = 3;

  std::string m_path;
  int m_pixel_size = 96;
  int m_maxValue = 255;

  Gtk::Label m_display;
  std::string m_buffer;
  int m_value = 0;

  std::function<void(int)> m_onDigit;
  std::function<void()> m_onBackspace;
  std::function<void(int)> m_onOk;

  sigc::signal<void, int> m_signal_ok_pressed;

  int clamp_to_max(int v) const {
    if (v < 0)
      return 0;
    if (v > m_maxValue)
      return m_maxValue;
    return v;
  }

  void update_display() {
    m_display.set_text(m_buffer.empty() ? "0" : m_buffer);
  }

  void apply_buffer_as_value() {
    if (m_buffer.empty()) {
      m_buffer = "0";
      m_value = 0;
      return;
    }

    int v = 0;
    try {
      v = std::stoi(m_buffer);
    } catch (...) {
      v = 0;
    }

    m_value = clamp_to_max(v);
    m_buffer = std::to_string(m_value);
  }

  int preview_append_digit(int digit) const {
    std::string tmp = m_buffer;

    if (tmp == "0")
      tmp.clear();
    if (tmp.size() >= MAX_DIGITS)
      return 9999;

    tmp.push_back(static_cast<char>('0' + digit));

    int v = 0;
    try {
      v = std::stoi(tmp);
    } catch (...) {
      v = 0;
    }

    return v;
  }

  void on_digit_pressed(int digit) {
    if (m_onDigit)
      m_onDigit(digit);

    if (m_buffer.size() >= MAX_DIGITS)
      return;

    const int candidate = preview_append_digit(digit);
    if (candidate > m_maxValue)
      return;

    if (m_buffer == "0")
      m_buffer.clear();

    m_buffer.push_back(static_cast<char>('0' + digit));

    apply_buffer_as_value();
    update_display();
  }

  void on_backspace() {
    if (m_onBackspace)
      m_onBackspace();

    if (!m_buffer.empty())
      m_buffer.pop_back();
    if (m_buffer.empty())
      m_buffer = "0";

    apply_buffer_as_value();
    update_display();
  }

  void on_ok() {
    apply_buffer_as_value();
    update_display();

    m_signal_ok_pressed.emit(m_value);

    if (m_onOk)
      m_onOk(m_value);
  }

  void add_digit_row(std::initializer_list<int> digits) {
    auto row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    row->set_spacing(10);
    row->set_halign(Gtk::ALIGN_CENTER);

    for (int d : digits)
      row->pack_start(*make_digit_button(d), Gtk::PACK_SHRINK);

    pack_start(*row, Gtk::PACK_SHRINK);
  }

  ImageButton *make_digit_button(int digit) {
    return make_button(std::to_string(digit),
                       [this, digit]() { on_digit_pressed(digit); });
  }

  ImageButton *make_action_button(const std::string &name_no_ext,
                                  std::function<void()> on_click) {
    return make_button(name_no_ext, std::move(on_click));
  }

  ImageButton *make_button(const std::string &name_no_ext,
                           std::function<void()> on_click) {
    const std::string path = m_path + "/" + name_no_ext + ".png";
    auto btn = Gtk::manage(new ImageButton(path, m_pixel_size));

    if (on_click) {
      btn->signal_clicked().connect(
          [cb = std::move(on_click)]() mutable { cb(); });
    }

    return btn;
  }
};
