#include "calandar.h"

#include "imgbutton.h"

#include <ctime>
#include <iomanip>
#include <sstream>

Calandar::Calandar()
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL),
      m_headerBox(Gtk::ORIENTATION_HORIZONTAL), m_btnPrev("<"), m_btnNext(">") {
  std::time_t t = std::time(nullptr);
  std::tm *now = std::localtime(&t);
  if (now) {
    m_year = now->tm_year + 1900;
    m_month = now->tm_mon + 1;
    m_day = now->tm_mday;
  }

  build_ui();
  refresh();
}

void Calandar::build_ui() {
  set_spacing(CALANDAR_SPACING);

  m_css = Gtk::CssProvider::create();
  std::ostringstream css;
  css << ".calandar-root { padding: " << CALANDAR_SPACING << "px; }"
      << ".cal-header-btn { min-width: " << CALANDAR_HEADER_BTN_SIZE
      << "px; min-height: " << CALANDAR_HEADER_BTN_SIZE << "px; padding: 0; }"
      << ".cal-header-label { font-weight: bold; font-size: "
      << CALANDAR_HEADER_FONT_PX << "px; }"
      << ".cal-dow { font-weight: bold; padding-top: 4px; padding-bottom: 4px; "
         "}"
      << ".cal-day-btn { min-width: " << CALANDAR_CELL_SIZE
      << "px; min-height: " << CALANDAR_CELL_SIZE
      << "px; padding: 0; border-radius: " << CALANDAR_CELL_RADIUS
      << "px; border: none; background: transparent; box-shadow: none; }"
      << ".cal-day-btn:hover { background: alpha(#000000, 0.06); }"
      << ".cal-day-btn:disabled { background: transparent; box-shadow: none; }"
      << ".cal-day-label { color: #111111; }"
      << ".cal-day-label-other { color: #9a9a9a; }"
      << ".cal-day-btn-selected { background-color: #ff8c2a; border-radius: "
      << CALANDAR_CELL_RADIUS << "px; }"
      << ".cal-day-btn-selected label { color: #ffffff; font-weight: bold; }";
  m_css->load_from_data(css.str());

  auto screen = Gdk::Screen::get_default();
  if (screen) {
    Gtk::StyleContext::add_provider_for_screen(
        screen, m_css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
  get_style_context()->add_class("calandar-root");

  m_headerBox.set_spacing(CALANDAR_HEADER_SPACING);
  m_headerBox.set_halign(Gtk::ALIGN_CENTER);
  m_btnPrev.get_style_context()->add_class("cal-header-btn");
  m_btnNext.get_style_context()->add_class("cal-header-btn");
  m_lblHeader.get_style_context()->add_class("cal-header-label");
  m_lblHeader.set_halign(Gtk::ALIGN_CENTER);
  m_lblHeader.set_hexpand(true);
  m_lblHeader.set_justify(Gtk::JUSTIFY_CENTER);
  m_btnPrev.signal_clicked().connect(
      sigc::mem_fun(*this, &Calandar::on_prev_month));
  m_btnNext.signal_clicked().connect(
      sigc::mem_fun(*this, &Calandar::on_next_month));
  m_headerBox.pack_start(m_btnPrev, Gtk::PACK_SHRINK);
  m_headerBox.pack_start(m_lblHeader, Gtk::PACK_EXPAND_WIDGET);
  m_headerBox.pack_start(m_btnNext, Gtk::PACK_SHRINK);
  pack_start(m_headerBox, Gtk::PACK_SHRINK);

  m_grid.set_row_spacing(CALANDAR_GRID_SPACING);
  m_grid.set_column_spacing(CALANDAR_GRID_SPACING);
  m_grid.set_halign(Gtk::ALIGN_CENTER);

  static const char *days[7] = {"Sun", "Mon", "Tue", "Wed",
                                "Thu", "Fri", "Sat"};
  for (int col = 0; col < 7; ++col) {
    auto *lbl = Gtk::manage(new Gtk::Label(days[col]));
    lbl->set_halign(Gtk::ALIGN_CENTER);
    lbl->set_valign(Gtk::ALIGN_CENTER);
    lbl->get_style_context()->add_class("cal-dow");
    m_dowLabels[col] = lbl;
    m_grid.attach(*lbl, col, 0, 1, 1);
  }

  for (int i = 0; i < 42; ++i) {
    auto *btn = Gtk::manage(new Gtk::Button());
    auto *overlay = Gtk::manage(new Gtk::Overlay());
    overlay->set_halign(Gtk::ALIGN_CENTER);
    overlay->set_valign(Gtk::ALIGN_CENTER);
    overlay->set_hexpand(false);
    overlay->set_vexpand(false);
    overlay->set_size_request(CALANDAR_CELL_SIZE, CALANDAR_CELL_SIZE);

    auto *lbl = Gtk::manage(new Gtk::Label(""));
    auto *img =
        Gtk::manage(new Gtk::Image(std::string(ICON_PATH) + "/selbg.png"));
    auto pix =
        Gdk::Pixbuf::create_from_file(std::string(ICON_PATH) + "/selbg.png");
    auto scaled = pix->scale_simple(CALANDAR_CELL_SIZE, CALANDAR_CELL_SIZE,
                                    Gdk::INTERP_BILINEAR);
    img->set(scaled);

    btn->set_relief(Gtk::RELIEF_NONE);
    btn->set_can_focus(false);
    btn->set_size_request(CALANDAR_CELL_SIZE, CALANDAR_CELL_SIZE);
    btn->get_style_context()->add_class("cal-day-btn");

    img->set_halign(Gtk::ALIGN_CENTER);
    img->set_valign(Gtk::ALIGN_CENTER);
    img->set_no_show_all(true);
    img->hide();

    lbl->set_halign(Gtk::ALIGN_CENTER);
    lbl->set_valign(Gtk::ALIGN_CENTER);
    lbl->get_style_context()->add_class("cal-day-label");

    overlay->add(*img);
    overlay->add_overlay(*lbl);
    overlay->set_overlay_pass_through(*img, true);
    overlay->set_overlay_pass_through(*lbl, true);
    btn->add(*overlay);

    const int row = 1 + (i / 7);
    const int col = i % 7;
    m_cells[i].button = btn;
    m_cells[i].label = lbl;
    m_cells[i].selbg = img;

    btn->signal_clicked().connect(
        sigc::bind(sigc::mem_fun(*this, &Calandar::on_day_clicked), i));
    m_grid.attach(*btn, col, row, 1, 1);
  }

  pack_start(m_grid, Gtk::PACK_SHRINK);
  show_all_children();
}

void Calandar::select_month(guint month, guint year) {
  if (month > 11)
    month = 11;
  m_month = static_cast<int>(month) + 1;
  m_year = static_cast<int>(year);
  int dim = days_in_month(m_year, m_month);
  if (m_day > dim)
    m_day = dim;
  refresh();
}

void Calandar::select_day(guint day) {
  int dim = days_in_month(m_year, m_month);
  if (day < 1)
    day = 1;
  if (static_cast<int>(day) > dim)
    day = dim;
  m_day = static_cast<int>(day);
  refresh();
}

void Calandar::get_date(guint &year, guint &month, guint &day) const {
  year = static_cast<guint>(m_year);
  month = static_cast<guint>(m_month - 1);
  day = static_cast<guint>(m_day);
}

sigc::signal<void> &Calandar::signal_day_selected() {
  return m_signalDaySelected;
}

void Calandar::refresh() {
  refresh_header();
  refresh_grid();
}

void Calandar::refresh_header() {
  m_lblHeader.set_text(month_name(m_month) + " " + std::to_string(m_year));
}

void Calandar::refresh_grid() {
  const int first_wday = first_weekday_of_month(m_year, m_month);
  const int dim = days_in_month(m_year, m_month);

  int prev_month = m_month - 1;
  int prev_year = m_year;
  if (prev_month < 1) {
    prev_month = 12;
    prev_year--;
  }

  int next_month = m_month + 1;
  int next_year = m_year;
  if (next_month > 12) {
    next_month = 1;
    next_year++;
  }

  const int prev_dim = days_in_month(prev_year, prev_month);

  for (int i = 0; i < 42; ++i) {
    auto &cell = m_cells[i];
    auto *btn = cell.button;
    auto *lbl = cell.label;

    cell.selbg->hide();
    lbl->get_style_context()->remove_class("cal-day-label-other");

    int cell_day = 0;
    int cell_month = m_month;
    int cell_year = m_year;
    bool in_current = false;

    if (i < first_wday) {
      cell_day = prev_dim - first_wday + i + 1;
      cell_month = prev_month;
      cell_year = prev_year;
      in_current = false;
    } else if (i < first_wday + dim) {
      cell_day = i - first_wday + 1;
      cell_month = m_month;
      cell_year = m_year;
      in_current = true;
    } else {
      cell_day = i - (first_wday + dim) + 1;
      cell_month = next_month;
      cell_year = next_year;
      in_current = false;
    }

    cell.year = cell_year;
    cell.month = cell_month;
    cell.day = cell_day;
    cell.in_current_month = in_current;

    lbl->set_text(std::to_string(cell_day));

    if (in_current) {
      btn->set_sensitive(true);
      if (cell_day == m_day) {
        cell.selbg->show();
      }
    } else {
      btn->set_sensitive(false);
      lbl->get_style_context()->add_class("cal-day-label-other");
    }
  }

  show_all_children();
}

void Calandar::on_prev_month() {
  if (m_month == 1) {
    m_month = 12;
    --m_year;
  } else {
    --m_month;
  }
  int dim = days_in_month(m_year, m_month);
  if (m_day > dim)
    m_day = dim;
  refresh();
}

void Calandar::on_next_month() {
  if (m_month == 12) {
    m_month = 1;
    ++m_year;
  } else {
    ++m_month;
  }
  int dim = days_in_month(m_year, m_month);
  if (m_day > dim)
    m_day = dim;
  refresh();
}

void Calandar::on_day_clicked(int index) {
  if (index < 0 || index >= static_cast<int>(m_cells.size()))
    return;

  auto &cell = m_cells[index];
  if (!cell.in_current_month)
    return;

  m_year = cell.year;
  m_month = cell.month;
  m_day = cell.day;
  refresh();
  m_signalDaySelected.emit();
}

bool Calandar::is_leap_year(int year) {
  if (year % 400 == 0)
    return true;
  if (year % 100 == 0)
    return false;
  return (year % 4 == 0);
}

int Calandar::days_in_month(int year, int month) {
  switch (month) {
  case 1:
    return 31;
  case 2:
    return is_leap_year(year) ? 29 : 28;
  case 3:
    return 31;
  case 4:
    return 30;
  case 5:
    return 31;
  case 6:
    return 30;
  case 7:
    return 31;
  case 8:
    return 31;
  case 9:
    return 30;
  case 10:
    return 31;
  case 11:
    return 30;
  case 12:
    return 31;
  default:
    return 30;
  }
}

int Calandar::first_weekday_of_month(int year, int month) {
  std::tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = 1;
  t.tm_hour = 12;
  std::mktime(&t);
  return t.tm_wday;
}

std::string Calandar::month_name(int month) {
  static const char *names[12] = {
      "January", "February", "March",     "April",   "May",      "June",
      "July",    "August",   "September", "October", "November", "December"};
  if (month < 1 || month > 12)
    return "Unknown";
  return names[month - 1];
}
