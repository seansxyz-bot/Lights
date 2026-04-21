#pragma once

#include <algorithm>
#include <gtkmm.h>
#include <string>
#include <vector>

#include <cairomm/context.h>
#include <librsvg/rsvg.h>
#include <pangomm.h>

class ButtonImageMaker {
public:
  static Glib::RefPtr<Gdk::Pixbuf> create(const std::string &settings_path,
                                          const std::string &text,
                                          int side_px) {
    if (side_px <= 0)
      return {};

    const std::string svg_path = settings_path + "/icons/blank.svg";

    GError *error = nullptr;
    RsvgHandle *handle = rsvg_handle_new_from_file(svg_path.c_str(), &error);
    if (!handle) {
      if (error) {
        g_warning("SVG load error: %s", error->message);
        g_error_free(error);
      }
      return {};
    }

    auto surface =
        Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, side_px, side_px);
    auto cr = Cairo::Context::create(surface);

    RsvgRectangle viewport = {0, 0, static_cast<double>(side_px),
                              static_cast<double>(side_px)};
    rsvg_handle_render_document(handle, cr->cobj(), &viewport, nullptr);
    g_object_unref(handle);

    auto layout = Pango::Layout::create(cr);
    layout->set_text(text);
    layout->set_alignment(Pango::ALIGN_CENTER);
    layout->set_wrap(Pango::WRAP_WORD_CHAR);

    const int padding_x = std::max(6, side_px * 8 / 100);
    const int padding_y = std::max(6, side_px * 12 / 100);
    const int content_w = std::max(1, side_px - padding_x * 2);
    const int content_h = std::max(1, side_px - padding_y * 2);

    layout->set_width(content_w * PANGO_SCALE);

    auto font_desc = Pango::FontDescription();
    font_desc.set_family("Sans");
    font_desc.set_weight(Pango::WEIGHT_BOLD);

    int font_px = std::max(8, side_px / 6);
    int text_w = 0;
    int text_h = 0;

    while (font_px > 8) {
      font_desc.set_absolute_size(font_px * PANGO_SCALE);
      layout->set_font_description(font_desc);
      layout->get_pixel_size(text_w, text_h);

      if (text_w <= content_w && text_h <= content_h)
        break;

      font_px -= 2;
    }

    const int x = padding_x;
    const int y = std::max(padding_y, (side_px - text_h) / 2);
    const int outline = (side_px >= 300) ? 2 : 1;

    cr->set_line_join(Cairo::LINE_JOIN_ROUND);

    for (int dx = -outline; dx <= outline; ++dx) {
      for (int dy = -outline; dy <= outline; ++dy) {
        if (dx == 0 && dy == 0)
          continue;

        cr->save();
        cr->set_source_rgb(0.0, 0.0, 0.0);
        cr->move_to(x + dx, y + dy);
        layout->show_in_cairo_context(cr);
        cr->restore();
      }
    }

    cr->set_source_rgb(1.0, 1.0, 1.0);
    cr->move_to(x, y);
    layout->show_in_cairo_context(cr);

    return Gdk::Pixbuf::create(surface, 0, 0, side_px, side_px);
  }
};
