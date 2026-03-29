#pragma once

#include "../tools/buttonimagemaker.h"
#include "../tools/logger.h"

#if (UBUNTU == 1)
#define ICON_PATH "/home/dev/.lightcontroller/icons"
#else
#define ICON_PATH "/home/lights/.lightcontroller/icons"
#endif

#include <gtkmm.h>
#include <iostream>
#include <string>

class ImageButton : public Gtk::Button {
public:
  // Original constructor
  ImageButton(const std::string &image_path, int pixel_size = 64,
              int margin = 6, int padding = 8)
      : m_pixel_size(pixel_size) {

    set_relief(Gtk::RELIEF_NONE);
    set_can_focus(false);

    set_border_width(padding);

    set_margin_top(margin);
    set_margin_bottom(margin);
    set_margin_start(margin);
    set_margin_end(margin);

    set_image_from_path(image_path);
  }

  // Toggle constructor
  ImageButton(const std::string &icon_path, const std::string &off_name,
              const std::string &on_name, int toggled, int pixel_size = 64,
              int margin = 6, int padding = 8)
      : m_icon_path(icon_path), m_off_name(off_name), m_on_name(on_name),
        m_toggled(toggled ? 1 : 0), m_pixel_size(pixel_size) {
    set_relief(Gtk::RELIEF_NONE);
    set_can_focus(false);

    set_border_width(padding);

    set_margin_top(margin);
    set_margin_bottom(margin);
    set_margin_start(margin);
    set_margin_end(margin);

    update_image();

    // Internal click handler
    signal_clicked().connect(
        sigc::mem_fun(*this, &ImageButton::on_internal_clicked));
  }

  void set_image_from_pixbuf(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf) {
    if (!pixbuf)
      return;

    auto scaled =
        pixbuf->scale_simple(m_pixel_size, m_pixel_size, Gdk::INTERP_BILINEAR);

    m_image.set(scaled);
    m_image.set_halign(Gtk::ALIGN_CENTER);
    m_image.set_valign(Gtk::ALIGN_CENTER);
    set_image(m_image);
    set_always_show_image(true);
  }

  // Pixbuf constructor (for generated images)
  ImageButton(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf, int pixel_size = 64,
              int margin = 6, int padding = 8)
      : m_pixel_size(pixel_size) {

    set_relief(Gtk::RELIEF_NONE);
    set_can_focus(false);
    get_style_context()->add_class("flat");
    set_border_width(0);
    set_margin_top(margin);
    set_margin_bottom(margin);
    set_margin_start(margin);
    set_margin_end(margin);
    set_size_request(m_pixel_size, m_pixel_size);
    set_image_from_pixbuf(pixbuf);
    m_image.set_halign(Gtk::ALIGN_CENTER);
    m_image.set_valign(Gtk::ALIGN_CENTER);
    get_style_context()->add_class("image-button-clean");
  }

  int get_toggled() const { return m_toggled; }

  void set_toggled(int toggled) {
    m_toggled = toggled ? 1 : 0;
    update_image();
  }

  bool is_toggled() const { return m_toggled == 1; }

private:
  Gtk::Image m_image;

  std::string m_icon_path;
  std::string m_off_name;
  std::string m_on_name;

  int m_toggled = 0;
  int m_pixel_size = 64;

  void on_internal_clicked() {
    // Only do toggle logic if this is a toggle-style button
    if (!m_off_name.empty() || !m_on_name.empty()) {
      m_toggled = !m_toggled;
      update_image();
    }
  }

  void update_image() {
    if (m_off_name.empty() && m_on_name.empty())
      return;

    std::string full_path;
    if (m_toggled == 0)
      full_path = m_icon_path + "/" + m_off_name + ".png";
    else
      full_path = m_icon_path + "/" + m_on_name + ".png";

    set_image_from_path(full_path);
  }

  void set_image_from_path(const std::string &image_path) {

    auto pixbuf = Gdk::Pixbuf::create_from_file(image_path);
    auto scaled =
        pixbuf->scale_simple(m_pixel_size, m_pixel_size, Gdk::INTERP_BILINEAR);

    m_image.set(scaled);
    set_image(m_image);
    set_always_show_image(true);
  }
};
