#include "colorwheelpicker.h"
#include <algorithm>
#include <cmath>
#include <sstream>

static constexpr double PI = 3.14159265358979323846;

// -------------------- ctor --------------------
ColorWheelPicker::ColorWheelPicker(std::string PATH,
                                   const std::string &headerText, int red,
                                   int green, int blue, int wheel_diameter,
                                   int bar_width, int keypad_pixel_size)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL),
      m_mainRow(Gtk::ORIENTATION_HORIZONTAL),
      m_leftCol(Gtk::ORIENTATION_VERTICAL),
      m_rightCol(Gtk::ORIENTATION_VERTICAL), m_lblR("Red"), m_lblG("Green"),
      m_lblB("Blue"), m_wheelDiameter(wheel_diameter), m_barWidth(bar_width),
      m_keypadPixelSize(keypad_pixel_size) {
  set_spacing(10);

  auto bold = Pango::FontDescription();
  bold.set_weight(Pango::WEIGHT_BOLD);
  bold.set_size(16 * Pango::SCALE);

  // -------------------- Header --------------------
  m_headerLbl.set_text(headerText);
  m_headerLbl.set_halign(Gtk::ALIGN_CENTER);
  m_headerLbl.set_valign(Gtk::ALIGN_START);

  auto headerFont = Pango::FontDescription();
  headerFont.set_weight(Pango::WEIGHT_BOLD);
  headerFont.set_size(22 * Pango::SCALE);
  m_headerLbl.override_font(headerFont);

  m_headerLbl.set_margin_top(10);
  m_headerLbl.set_margin_bottom(10);

  m_lblR.override_font(bold);
  m_lblG.override_font(bold);
  m_lblB.override_font(bold);

  // Make wheel/bar focusable so we can "bounce" focus back to them
  m_wheel.set_can_focus(true);
  m_bar.set_can_focus(true);

  // -------------------- build normal layout into m_pickerRoot
  // --------------------
  m_wheel.set_size_request(m_wheelDiameter, m_wheelDiameter);
  m_bar.set_size_request(m_wheelDiameter, m_barWidth);

  // Make them interactive
  m_wheel.add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK |
                     Gdk::POINTER_MOTION_MASK | Gdk::BUTTON1_MOTION_MASK);
  m_bar.add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK |
                   Gdk::POINTER_MOTION_MASK | Gdk::BUTTON1_MOTION_MASK);

  // LEFT column: wheel + bar
  m_leftCol.set_spacing(10);
  m_leftCol.pack_start(m_wheel, Gtk::PACK_SHRINK);
  m_leftCol.pack_start(m_bar, Gtk::PACK_SHRINK);

  // RIGHT column: RGB grid
  m_grid.set_row_spacing(8);
  m_grid.set_column_spacing(10);

  setup_entry(m_entryR);
  setup_entry(m_entryG);
  setup_entry(m_entryB);

  m_grid.attach(m_lblR, 0, 0, 1, 1);
  m_grid.attach(m_entryR, 1, 0, 1, 1);

  m_grid.attach(m_lblG, 0, 1, 1, 1);
  m_grid.attach(m_entryG, 1, 1, 1, 1);

  m_grid.attach(m_lblB, 0, 2, 1, 1);
  m_grid.attach(m_entryB, 1, 2, 1, 1);

  // main row: left + right
  m_mainRow.set_spacing(14);
  m_rightCol.set_orientation(Gtk::ORIENTATION_VERTICAL);
  m_rightCol.set_vexpand(true);
  m_rightCol.set_hexpand(false);
  m_grid.set_hexpand(false);

  auto spacerTop = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
  spacerTop->set_vexpand(true);
  auto spacerBottom = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
  spacerBottom->set_vexpand(true);

  m_rightCol.pack_start(*spacerTop, Gtk::PACK_EXPAND_WIDGET);
  m_rightCol.pack_start(m_grid, Gtk::PACK_SHRINK);
  m_rightCol.pack_start(*spacerBottom, Gtk::PACK_EXPAND_WIDGET);

  m_mainRow.pack_start(m_leftCol, Gtk::PACK_SHRINK);
  m_mainRow.pack_start(m_rightCol, Gtk::PACK_SHRINK); // <-- change this

  // --- Center the whole (wheel + entries) group as ONE unit ---
  auto hCenterRow = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  hCenterRow->set_halign(Gtk::ALIGN_FILL);
  hCenterRow->set_hexpand(true);

  // left + right spacers take equal expand, forcing center
  auto leftSpacer = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  auto rightSpacer = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  leftSpacer->set_hexpand(true);
  rightSpacer->set_hexpand(true);

  // mainRow should NOT expand, so its natural width is preserved
  m_mainRow.set_hexpand(false);
  m_mainRow.set_halign(Gtk::ALIGN_CENTER);

  hCenterRow->pack_start(*leftSpacer, Gtk::PACK_EXPAND_WIDGET);
  hCenterRow->pack_start(m_mainRow, Gtk::PACK_SHRINK);
  hCenterRow->pack_start(*rightSpacer, Gtk::PACK_EXPAND_WIDGET);

  // Header first, then the centered wheel+entries group
  m_pickerRoot.pack_start(m_headerLbl, Gtk::PACK_SHRINK);
  m_pickerRoot.pack_start(*hCenterRow, Gtk::PACK_SHRINK);

  // -------------------- Bottom OK button (inside picker page)
  // -------------------- push it to the bottom of the available space
  auto bottomSpacer = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
  bottomSpacer->set_vexpand(true);
  m_pickerRoot.pack_start(*bottomSpacer, Gtk::PACK_EXPAND_WIDGET);

  // -------------------- stack swapper --------------------
  m_stack.set_transition_type(Gtk::STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
  m_stack.set_transition_duration(120);

  m_stack.add(m_pickerRoot, "picker");
  m_stack.set_visible_child("picker");

  pack_start(m_stack, Gtk::PACK_EXPAND_WIDGET);

  // -------------------- connect focus-in/click to show keypad
  // --------------------
  const std::string path = PATH;
  auto hook_entry = [this, path](Gtk::Entry &ent) {
    ent.set_can_focus(true);
    ent.add_events(Gdk::FOCUS_CHANGE_MASK | Gdk::BUTTON_PRESS_MASK);

    ent.signal_button_press_event().connect(
        [this, path, &ent](GdkEventButton *e) -> bool {
          if (!e || e->button != 1)
            return false;
          if (m_updatingEntries)
            return false;
          show_keypad_for(path, &ent);
          return true; // consume click so cursor/IME doesn’t pop up
        },
        false);

    ent.signal_focus_in_event().connect(
        [this, path, &ent](GdkEventFocus *) -> bool {
          if (m_updatingEntries)
            return false;
          show_keypad_for(path, &ent);
          return false;
        },
        false);
  };

  hook_entry(m_entryR);
  hook_entry(m_entryG);
  hook_entry(m_entryB);

  // ---- DRAW SIGNALS ----
  m_wheel.signal_draw().connect(
      sigc::mem_fun(*this, &ColorWheelPicker::on_wheel_draw), false);
  m_bar.signal_draw().connect(
      sigc::mem_fun(*this, &ColorWheelPicker::on_bar_draw), false);

  // ---- POINTER EVENTS ----
  m_wheel.signal_button_press_event().connect(
      sigc::mem_fun(*this, &ColorWheelPicker::on_wheel_button_press), false);
  m_wheel.signal_button_release_event().connect(
      sigc::mem_fun(*this, &ColorWheelPicker::on_wheel_button_release), false);
  m_wheel.signal_motion_notify_event().connect(
      sigc::mem_fun(*this, &ColorWheelPicker::on_wheel_motion), false);

  m_bar.signal_button_press_event().connect(
      sigc::mem_fun(*this, &ColorWheelPicker::on_bar_button_press), false);
  m_bar.signal_button_release_event().connect(
      sigc::mem_fun(*this, &ColorWheelPicker::on_bar_button_release), false);
  m_bar.signal_motion_notify_event().connect(
      sigc::mem_fun(*this, &ColorWheelPicker::on_bar_motion), false);

  m_entryR.set_can_focus(true);
  m_entryG.set_can_focus(true);
  m_entryB.set_can_focus(true);

  set_rgb(red, green, blue);
  update_entries_from_rgb();
  show_all_children();
}

// -------------------- focus-in handler --------------------
bool ColorWheelPicker::on_entry_focus_in(std::string PATH,
                                         GdkEventFocus * /*e*/,
                                         Gtk::Entry *which) {
  if (m_updatingEntries)
    return false;
  show_keypad_for(PATH, which);
  return false;
}

void ColorWheelPicker::show_keypad_for(std::string PATH, Gtk::Entry *which) {
  if (!which)
    return;

  m_activeEntry = which;

  if (!m_keypad) {
    int start_val = 0;
    try {
      start_val = std::stoi(which->get_text());
    } catch (...) {
      start_val = 0;
    }

    m_keypad = Gtk::manage(new KeyPad(PATH, start_val, m_keypadPixelSize));
    m_stack.add(*m_keypad, "keypad");
    m_keypad->show_all();
    m_stack.show_all_children();

    m_keypad->signal_ok_pressed().connect(
        [this](int v) { hide_keypad_and_apply(v); });
  } else {
    int start_val = 0;
    try {
      start_val = std::stoi(which->get_text());
    } catch (...) {
      start_val = 0;
    }
    m_keypad->set_value(start_val);
  }

  m_stack.set_visible_child("keypad");
}

void ColorWheelPicker::hide_keypad_and_apply(int v) {
  m_stack.set_visible_child("picker");

  if (m_activeEntry) {
    set_entry_value(*m_activeEntry, v);
    on_entry_changed();
  }

  m_activeEntry = nullptr;

  if (m_wheel.get_visible() && m_wheel.get_can_focus())
    m_wheel.grab_focus();
  else if (m_bar.get_visible() && m_bar.get_can_focus())
    m_bar.grab_focus();
}

void ColorWheelPicker::set_entry_value(Gtk::Entry &entry, int v) {
  m_updatingEntries = true;
  entry.set_text(std::to_string(clampi(v, 0, 255)));
  m_updatingEntries = false;
}

// -------------------- entry setup + change --------------------
void ColorWheelPicker::setup_entry(Gtk::Entry &entry) {
  entry.set_width_chars(4);
  entry.set_max_length(3);
  entry.set_input_purpose(Gtk::INPUT_PURPOSE_DIGITS);

  entry.signal_changed().connect(
      sigc::mem_fun(*this, &ColorWheelPicker::on_entry_changed));
}

void ColorWheelPicker::on_entry_changed() {
  if (m_updatingEntries)
    return;

  auto parse255 = [](const Glib::ustring &s) -> int {
    int v = 0;
    try {
      v = std::stoi(s.raw());
    } catch (...) {
      v = 0;
    }
    if (v < 0)
      v = 0;
    if (v > 255)
      v = 255;
    return v;
  };

  m_r = parse255(m_entryR.get_text());
  m_g = parse255(m_entryG.get_text());
  m_b = parse255(m_entryB.get_text());

  // Coalesce entry edits too (cheap and keeps things consistent)
  request_apply_rgb(true);
}

ColorWheelPicker::RGB ColorWheelPicker::get_rgb() const {
  return {m_r, m_g, m_b};
}
int ColorWheelPicker::get_r() const { return m_r; }
int ColorWheelPicker::get_g() const { return m_g; }
int ColorWheelPicker::get_b() const { return m_b; }

Gdk::RGBA ColorWheelPicker::get_rgba() const {
  Gdk::RGBA c;
  c.set_rgba(m_r / 255.0, m_g / 255.0, m_b / 255.0, 1.0);
  return c;
}

void ColorWheelPicker::set_rgb(int r, int g, int b) {
  m_r = clampi(r, 0, 255);
  m_g = clampi(g, 0, 255);
  m_b = clampi(b, 0, 255);
  request_apply_rgb(true);
}
void ColorWheelPicker::set_rgb(const RGB &c) { set_rgb(c.r, c.g, c.b); }
void ColorWheelPicker::set_r(int r) { set_rgb(r, m_g, m_b); }
void ColorWheelPicker::set_g(int g) { set_rgb(m_r, g, m_b); }
void ColorWheelPicker::set_b(int b) { set_rgb(m_r, m_g, b); }

sigc::signal<void, int, int, int> &ColorWheelPicker::signal_color_changed() {
  return m_signalColorChanged;
}

// ---------------- Rendering ----------------
void ColorWheelPicker::ensure_wheel_surface() {
  if (!m_wheelDirty && m_wheelSurface)
    return;

  const int D = m_wheelDiameter;
  m_wheelSurface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, D, D);

  unsigned char *data = m_wheelSurface->get_data();
  const int stride = m_wheelSurface->get_stride();

  const double cx = (D - 1) / 2.0;
  const double cy = (D - 1) / 2.0;
  const double R = (D - 2) / 2.0;

  for (int y = 0; y < D; ++y) {
    for (int x = 0; x < D; ++x) {
      const double dx = x - cx;
      const double dy = y - cy;
      const double dist = std::sqrt(dx * dx + dy * dy);

      unsigned char *px = data + y * stride + x * 4;

      if (dist > R) {
        px[0] = px[1] = px[2] = px[3] = 0;
        continue;
      }

      double ang = std::atan2(dy, dx);
      double h = ang * 180.0 / PI;
      if (h < 0)
        h += 360.0;

      double s = clampd(dist / R, 0.0, 1.0);
      int rr, gg, bb;
      hsv_to_rgb(h, s, 1.0, rr, gg, bb);

      px[0] = static_cast<unsigned char>(bb);
      px[1] = static_cast<unsigned char>(gg);
      px[2] = static_cast<unsigned char>(rr);
      px[3] = 255;
    }
  }

  m_wheelSurface->mark_dirty();
  m_wheelDirty = false;
}

bool ColorWheelPicker::on_wheel_draw(const Cairo::RefPtr<Cairo::Context> &cr) {
  ensure_wheel_surface();

  cr->set_source(m_wheelSurface, 0, 0);
  cr->paint();

  const int D = m_wheelDiameter;
  const double cx = (D - 1) / 2.0;
  const double cy = (D - 1) / 2.0;
  const double R = (D - 2) / 2.0;

  const double ang = m_h * PI / 180.0;
  const double rad = m_s * R;

  const double x = cx + std::cos(ang) * rad;
  const double y = cy + std::sin(ang) * rad;

  cr->set_line_width(2.0);
  cr->set_source_rgb(1, 1, 1);
  cr->arc(x, y, 6.0, 0, 2 * PI);
  cr->stroke();

  cr->set_line_width(1.0);
  cr->set_source_rgb(0, 0, 0);
  cr->arc(x, y, 7.5, 0, 2 * PI);
  cr->stroke();

  return true;
}

bool ColorWheelPicker::on_bar_draw(const Cairo::RefPtr<Cairo::Context> &cr) {
  const auto alloc = m_bar.get_allocation();
  const int w = alloc.get_width();
  const int h = alloc.get_height();

  int brightR, brightG, brightB;
  hsv_to_rgb(m_h, m_s, 1.0, brightR, brightG, brightB);

  auto grad = Cairo::LinearGradient::create(0, 0, w, 0);
  grad->add_color_stop_rgb(0.0, 0.0, 0.0, 0.0);
  grad->add_color_stop_rgb(1.0, brightR / 255.0, brightG / 255.0,
                           brightB / 255.0);

  cr->rectangle(0, 0, w, h);
  cr->set_source(grad);
  cr->fill();

  cr->set_line_width(1.0);
  cr->set_source_rgb(1, 1, 1);
  cr->rectangle(0.5, 0.5, w - 1.0, h - 1.0);
  cr->stroke();

  const double x = m_v * (w - 1);

  cr->set_line_width(2.0);
  cr->set_source_rgb(1, 1, 1);
  cr->move_to(x, 0);
  cr->line_to(x, h);
  cr->stroke();

  cr->set_line_width(1.0);
  cr->set_source_rgb(0, 0, 0);
  cr->move_to(x + 2, 0);
  cr->line_to(x + 2, h);
  cr->stroke();

  return true;
}

// ---------------- Events ----------------
bool ColorWheelPicker::on_wheel_button_press(GdkEventButton *e) {
  if (!e || e->button != 1)
    return false;
  m_dragWheel = true;
  set_from_wheel_point(e->x, e->y, true);
  return true;
}

bool ColorWheelPicker::on_wheel_button_release(GdkEventButton *e) {
  if (!e || e->button != 1)
    return false;
  m_dragWheel = false;
  flush_pending_now(); // ensure final value is applied immediately
  return true;
}

bool ColorWheelPicker::on_wheel_motion(GdkEventMotion *e) {
  if (!m_dragWheel || !e)
    return false;
  set_from_wheel_point(e->x, e->y, true);
  return true;
}

bool ColorWheelPicker::on_bar_button_press(GdkEventButton *e) {
  if (!e || e->button != 1)
    return false;
  m_dragBar = true;
  set_from_bar_point(e->x, true);
  return true;
}

bool ColorWheelPicker::on_bar_button_release(GdkEventButton *e) {
  if (!e || e->button != 1)
    return false;
  m_dragBar = false;
  flush_pending_now();
  return true;
}

bool ColorWheelPicker::on_bar_motion(GdkEventMotion *e) {
  if (!m_dragBar || !e)
    return false;
  set_from_bar_point(e->x, true);
  return true;
}

// ---------------- Text handling ----------------
void ColorWheelPicker::update_entries_from_rgb() {
  m_updatingEntries = true;
  m_entryR.set_text(std::to_string(m_r));
  m_entryG.set_text(std::to_string(m_g));
  m_entryB.set_text(std::to_string(m_b));
  m_updatingEntries = false;
}

// ---------------- Color math ----------------
int ColorWheelPicker::clampi(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

double ColorWheelPicker::clampd(double v, double lo, double hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

void ColorWheelPicker::hsv_to_rgb(double h, double s, double v, int &r, int &g,
                                  int &b) {
  h = std::fmod(h, 360.0);
  if (h < 0)
    h += 360.0;
  s = clampd(s, 0.0, 1.0);
  v = clampd(v, 0.0, 1.0);

  double c = v * s;
  double x = c * (1.0 - std::fabs(std::fmod(h / 60.0, 2.0) - 1.0));
  double m = v - c;

  double rp = 0, gp = 0, bp = 0;
  if (h < 60) {
    rp = c;
    gp = x;
    bp = 0;
  } else if (h < 120) {
    rp = x;
    gp = c;
    bp = 0;
  } else if (h < 180) {
    rp = 0;
    gp = c;
    bp = x;
  } else if (h < 240) {
    rp = 0;
    gp = x;
    bp = c;
  } else if (h < 300) {
    rp = x;
    gp = 0;
    bp = c;
  } else {
    rp = c;
    gp = 0;
    bp = x;
  }

  r = clampi(static_cast<int>(std::round((rp + m) * 255.0)), 0, 255);
  g = clampi(static_cast<int>(std::round((gp + m) * 255.0)), 0, 255);
  b = clampi(static_cast<int>(std::round((bp + m) * 255.0)), 0, 255);
}

void ColorWheelPicker::rgb_to_hsv(int r, int g, int b, double &h, double &s,
                                  double &v) {
  double rf = clampi(r, 0, 255) / 255.0;
  double gf = clampi(g, 0, 255) / 255.0;
  double bf = clampi(b, 0, 255) / 255.0;

  double cmax = std::max({rf, gf, bf});
  double cmin = std::min({rf, gf, bf});
  double delta = cmax - cmin;

  if (delta == 0)
    h = 0;
  else if (cmax == rf)
    h = 60.0 * std::fmod(((gf - bf) / delta), 6.0);
  else if (cmax == gf)
    h = 60.0 * (((bf - rf) / delta) + 2.0);
  else
    h = 60.0 * (((rf - gf) / delta) + 4.0);
  if (h < 0)
    h += 360.0;

  s = (cmax == 0) ? 0 : (delta / cmax);
  v = cmax;
}

// ============================================================
// Throttling / coalescing implementation
// ============================================================
void ColorWheelPicker::cancel_throttle() {
  if (m_throttleConn.connected())
    m_throttleConn.disconnect();
}

void ColorWheelPicker::request_apply_hsv(bool emit_signal) {
  m_pendingType = PendingType::HSV;
  m_pH = m_h;
  m_pS = m_s;
  m_pV = m_v;
  m_pendingEmit = m_pendingEmit || emit_signal;

  if (!m_throttleConn.connected()) {
    m_throttleConn = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &ColorWheelPicker::on_throttle_tick),
        m_throttleMs);
  }
}

void ColorWheelPicker::request_apply_rgb(bool emit_signal) {
  m_pendingType = PendingType::RGB;
  m_pR = m_r;
  m_pG = m_g;
  m_pB = m_b;
  m_pendingEmit = m_pendingEmit || emit_signal;

  if (!m_throttleConn.connected()) {
    m_throttleConn = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &ColorWheelPicker::on_throttle_tick),
        m_throttleMs);
  }
}

bool ColorWheelPicker::on_throttle_tick() {
  // One-shot: apply once, then stop the timer
  flush_pending();
  return false;
}

void ColorWheelPicker::flush_pending_now() {
  cancel_throttle();
  flush_pending();
}

void ColorWheelPicker::flush_pending() {
  if (m_pendingType == PendingType::None)
    return;

  const bool emit = m_pendingEmit;

  if (m_pendingType == PendingType::HSV) {
    m_h = m_pH;
    m_s = m_pS;
    m_v = m_pV;
    apply_hsv_now(emit);
  } else if (m_pendingType == PendingType::RGB) {
    m_r = m_pR;
    m_g = m_pG;
    m_b = m_pB;
    apply_rgb_now(emit);
  }

  m_pendingType = PendingType::None;
  m_pendingEmit = false;

  // If something queued again while we were applying, it will have
  // reconnected the timeout.
  if (m_throttleConn.connected())
    m_throttleConn.disconnect();
}

// "Now" versions: do all work immediately
void ColorWheelPicker::apply_hsv_now(bool emit_signal) {
  hsv_to_rgb(m_h, m_s, m_v, m_r, m_g, m_b);
  update_entries_from_rgb();
  m_bar.queue_draw();
  m_wheel.queue_draw();
  notify_change(emit_signal);
}

void ColorWheelPicker::apply_rgb_now(bool emit_signal) {
  rgb_to_hsv(m_r, m_g, m_b, m_h, m_s, m_v);
  update_entries_from_rgb();
  m_bar.queue_draw();
  m_wheel.queue_draw();
  notify_change(emit_signal);
}

// ============================================================

void ColorWheelPicker::notify_change(bool emit_signal) {
  if (emit_signal)
    m_signalColorChanged.emit(m_r, m_g, m_b);
}

// --- picking helpers ---
void ColorWheelPicker::set_from_wheel_point(double x, double y,
                                            bool emit_signal) {
  const int D = m_wheelDiameter;
  const double cx = (D - 1) / 2.0;
  const double cy = (D - 1) / 2.0;
  const double R = (D - 2) / 2.0;

  double dx = x - cx;
  double dy = y - cy;
  double dist = std::sqrt(dx * dx + dy * dy);

  if (dist > R) {
    dx *= (R / dist);
    dy *= (R / dist);
    dist = R;
  }

  double ang = std::atan2(dy, dx);
  double h = ang * 180.0 / PI;
  if (h < 0)
    h += 360.0;

  m_h = h;
  m_s = clampd(dist / R, 0.0, 1.0);

  // Coalesced/throttled apply
  request_apply_hsv(emit_signal);
}

void ColorWheelPicker::set_header_text(const std::string &text) {
  m_headerLbl.set_text(text);
}

void ColorWheelPicker::commit_pending() { flush_pending_now(); }

void ColorWheelPicker::set_from_bar_point(double x, bool emit_signal) {
  const auto alloc = m_bar.get_allocation();
  const int w = alloc.get_width();
  if (w <= 1)
    return;

  double v = x / (w - 1.0);
  m_v = clampd(v, 0.0, 1.0);

  request_apply_hsv(emit_signal);
}
