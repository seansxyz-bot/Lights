#pragma once

#include "../utils/buttonimagemaker.h"
#include "../utils/logger.h"

#if (UBUNTU == 1)
#define ICON_PATH "/home/dev/.lightcontroller/icons"
#else
#define ICON_PATH "/home/lights/.local/share/lights/icons"
#endif

#include <gtkmm.h>
#include <iostream>
#include <string>

class ImageButton : public Gtk::Button {
public:
  // Original constructor
  ImageButton(const std::string &image_path, int pixel_size = 64,
              int margin = 6, int padding = 8)
      : m_pixel_size(pixel_size), m_margin(margin), m_padding(padding),
        m_current_image_path(image_path) {

    common_init();
    set_image_from_path(image_path);
  }

  // Toggle constructor
  ImageButton(const std::string &icon_path, const std::string &off_name,
              const std::string &on_name, int toggled, int pixel_size = 64,
              int margin = 6, int padding = 8)
      : m_icon_path(icon_path), m_off_name(off_name), m_on_name(on_name),
        m_toggled(toggled ? 1 : 0), m_pixel_size(pixel_size), m_margin(margin),
        m_padding(padding) {

    common_init();
    update_image();

    signal_clicked().connect(
        sigc::mem_fun(*this, &ImageButton::on_internal_clicked));
  }

  // Pixbuf constructor
  ImageButton(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf, int pixel_size = 64,
              int margin = 6, int padding = 8)
      : m_pixel_size(pixel_size), m_margin(margin), m_padding(padding) {

    common_init();
    get_style_context()->add_class("image-button-clean");
    set_size_request(m_pixel_size, m_pixel_size);
    set_image_from_pixbuf(pixbuf);
  }

  int get_toggled() const { return m_toggled; }

  void set_toggled(int toggled) {
    m_toggled = toggled ? 1 : 0;
    update_image();
  }

  bool is_toggled() const { return m_toggled == 1; }

  void set_image_path(const std::string &image_path) {
    m_current_image_path = image_path;
    set_image_from_path(image_path);
  }

  const std::string &image_path() const { return m_current_image_path; }

  void set_pixel_size(int pixel_size) {
    if (pixel_size <= 0)
      return;

    m_pixel_size = pixel_size;
    set_size_request(m_pixel_size, m_pixel_size);

    if (!m_off_name.empty() || !m_on_name.empty()) {
      update_image();
    } else if (!m_current_image_path.empty()) {
      set_image_from_path(m_current_image_path);
    }
  }

  void set_image_from_pixbuf(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf) {
    if (!pixbuf)
      return;

    auto scaled =
        pixbuf->scale_simple(m_pixel_size, m_pixel_size, Gdk::INTERP_BILINEAR);

    if (!scaled)
      return;

    m_image.set(scaled);
    m_image.set_halign(Gtk::ALIGN_CENTER);
    m_image.set_valign(Gtk::ALIGN_CENTER);
    set_image(m_image);
    set_always_show_image(true);
  }

private:
  Gtk::Image m_image;

  std::string m_icon_path;
  std::string m_off_name;
  std::string m_on_name;
  std::string m_current_image_path;

  int m_toggled = 0;
  int m_pixel_size = 64;
  int m_margin = 6;
  int m_padding = 8;

  void common_init() {
    set_relief(Gtk::RELIEF_NONE);
    set_can_focus(false);

    set_border_width(m_padding);

    set_margin_top(m_margin);
    set_margin_bottom(m_margin);
    set_margin_start(m_margin);
    set_margin_end(m_margin);

    m_image.set_halign(Gtk::ALIGN_CENTER);
    m_image.set_valign(Gtk::ALIGN_CENTER);
  }

  void on_internal_clicked() {
    if (!m_off_name.empty() || !m_on_name.empty()) {
      m_toggled = !m_toggled;
      update_image();
    }
  }

  void update_image() {
    if (m_off_name.empty() && m_on_name.empty())
      return;

    if (m_toggled == 0)
      m_current_image_path = m_icon_path + "/" + m_off_name + ".png";
    else
      m_current_image_path = m_icon_path + "/" + m_on_name + ".png";

    set_image_from_path(m_current_image_path);
  }

  void set_image_from_path(const std::string &image_path) {
    try {
      auto pixbuf = Gdk::Pixbuf::create_from_file(image_path);
      if (!pixbuf)
        return;

      auto scaled = pixbuf->scale_simple(m_pixel_size, m_pixel_size,
                                         Gdk::INTERP_BILINEAR);
      if (!scaled)
        return;

      m_image.set(scaled);
      m_image.set_halign(Gtk::ALIGN_CENTER);
      m_image.set_valign(Gtk::ALIGN_CENTER);
      set_image(m_image);
      set_always_show_image(true);
      m_current_image_path = image_path;
    } catch (const Glib::Error &ex) {
      LOG_ERROR() << "ImageButton failed to load image: " << image_path
                  << " err=" << ex.what();
    } catch (...) {
      LOG_ERROR() << "ImageButton failed to load image: " << image_path;
    }
  }
};
