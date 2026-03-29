#pragma once

#include <algorithm>
#include <gtkmm.h>
#include <sstream>
#include <string>
#include <vector>

#include <cairomm/context.h>
#include <librsvg/rsvg.h>
#include <pangomm.h>

class ButtonImageMaker {
public:
  static Glib::RefPtr<Gdk::Pixbuf> create(std::string settings_path,
                                          const std::string &text, int size) {
    const std::string svgPath = std::string(settings_path) + "/icons/blank.svg";

    // --- Load SVG ---
    GError *error = nullptr;
    RsvgHandle *handle = rsvg_handle_new_from_file(svgPath.c_str(), &error);
    if (!handle) {
      if (error) {
        g_warning("SVG load error: %s", error->message);
        g_error_free(error);
      }
      return {};
    }

    // --- Create Cairo surface ---
    auto surface =
        Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
    auto cr = Cairo::Context::create(surface);

    // --- Render SVG scaled ---
    RsvgRectangle viewport = {0, 0, (double)size, (double)size};
    rsvg_handle_render_document(handle, cr->cobj(), &viewport, nullptr);

    g_object_unref(handle);

    // --- Prepare text layout ---
    auto layout = Pango::Layout::create(cr);

    std::vector<std::string> lines = wrapText(text, 11, 3);

    std::string finalText;
    for (size_t i = 0; i < lines.size(); ++i) {
      finalText += lines[i];
      if (i != lines.size() - 1)
        finalText += "\n";
    }

    layout->set_text(finalText);
    layout->set_alignment(Pango::ALIGN_CENTER);

    // Helps center multi-line text better
    layout->set_width((size - int(size * 0.16)) * PANGO_SCALE);

    // --- Auto font sizing ---
    int fontSize = size / 6;
    int textWidth = 0, textHeight = 0;

    auto fontDesc = Pango::FontDescription();
    fontDesc.set_family("Sans");
    fontDesc.set_weight(Pango::WEIGHT_BOLD);

    int paddingX = size * 0.08;
    int paddingY = size * 0.12;

    while (fontSize > 8) {
      fontDesc.set_absolute_size(fontSize * PANGO_SCALE);
      layout->set_font_description(fontDesc);

      layout->get_pixel_size(textWidth, textHeight);

      if (textWidth <= (size - paddingX * 2) &&
          textHeight <= (size - paddingY * 2)) {
        break;
      }

      fontSize -= 2;
    }

    // --- Center text ---
    int x = paddingX;
    int y = (size - textHeight) / 2;

    // --- Thin black outline ---
    // 1 px for smaller buttons, 2 px for bigger ones
    int outline = (size >= 300) ? 2 : 1;

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

    // --- White text on top ---
    cr->set_source_rgb(1.0, 1.0, 1.0);
    cr->move_to(x, y);
    layout->show_in_cairo_context(cr);

    // --- Convert to Pixbuf ---
    return Gdk::Pixbuf::create(surface, 0, 0, size, size);
  }

private:
  static std::vector<std::string> wrapText(const std::string &text,
                                           size_t maxChars, size_t maxLines) {
    std::istringstream iss(text);
    std::string word;

    std::vector<std::string> lines;
    std::string current;

    while (iss >> word) {
      size_t extra = current.empty() ? 0 : 1;

      if (current.size() + extra + word.size() <= maxChars) {
        if (!current.empty())
          current += " ";
        current += word;
      } else {
        if (!current.empty())
          lines.push_back(current);

        current = word;

        if (lines.size() == maxLines - 1)
          break;
      }
    }

    if (!current.empty() && lines.size() < maxLines)
      lines.push_back(current);

    return lines;
  }
};
