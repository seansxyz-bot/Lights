// ImageButton.cpp
#include "imgbutton.h"
#include <stdexcept>

ImageButton::ImageButton(const std::string &image_path, int pixel_size,
                         ClickCb on_click)
    : m_path(image_path), m_pixel_size(pixel_size),
      m_on_click(std::move(on_click)) {
  set_relief(Gtk::RELIEF_NONE);
  set_can_focus(false); // optional: nicer for touch UIs
  set_always_show_image(true);

  refresh_image();
  set_image(m_img);

  signal_clicked().connect([this]() {
    if (m_on_click)
      m_on_click(m_path);
  });
}

void ImageButton::set_image_path(const std::string &image_path) {
  m_path = image_path;
  refresh_image();
}

void ImageButton::set_pixel_size(int pixel_size) {
  m_pixel_size = std::max(1, pixel_size);
  refresh_image();
}

void ImageButton::refresh_image() {
  // Load + scale from file path
  auto pb = Gdk::Pixbuf::create_from_file(m_path);

  // Scale while preserving aspect ratio
  const int w = pb->get_width();
  const int h = pb->get_height();
  const double scale = (w > h) ? (double)m_pixel_size / (double)w
                               : (double)m_pixel_size / (double)h;

  const int nw = std::max(1, (int)(w * scale));
  const int nh = std::max(1, (int)(h * scale));

  auto scaled = pb->scale_simple(nw, nh, Gdk::INTERP_BILINEAR);
  m_img.set(scaled);
}
