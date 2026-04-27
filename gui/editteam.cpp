#include "editteam.h"

#include "../drivers/network/httphelper.h"
#include "../utils/logger.h"
#include "imgbutton.h"

#include <algorithm>
#include <cctype>
#include <gdkmm/pixbufloader.h>
#include <sstream>

namespace {
Gtk::Label *make_label(const std::string &text) {
  auto *lbl = Gtk::manage(new Gtk::Label(text));
  lbl->set_halign(Gtk::ALIGN_END);
  lbl->set_valign(Gtk::ALIGN_CENTER);
  return lbl;
}

Gtk::Box *make_hex_entry_row(Gtk::Entry &entry) {
  auto *box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  auto *hash = Gtk::manage(new Gtk::Label("#"));
  box->set_spacing(4);
  box->pack_start(*hash, Gtk::PACK_SHRINK);
  box->pack_start(entry, Gtk::PACK_EXPAND_WIDGET);
  return box;
}

void attach_row(Gtk::Grid &grid, int &row, const std::string &label,
                Gtk::Widget &widget) {
  grid.attach(*make_label(label), 0, row, 1, 1);
  grid.attach(widget, 1, row++, 1, 1);
}

std::string rgbToHex(uint8_t r, uint8_t g, uint8_t b) {
  std::ostringstream ss;
  ss << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
     << static_cast<int>(r) << std::setw(2) << static_cast<int>(g)
     << std::setw(2) << static_cast<int>(b);
  return ss.str();
}
} // namespace

EditTeam::EditTeam(const std::string &iconPath, const std::string &teamsDbPath,
                   const TeamRecord &team)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_iconPath(iconPath),
      m_teamsDbPath(teamsDbPath), m_team(team) {
  LOG_INFO() << "EditTeam ctor for team " << team.name;

  set_spacing(EDITTEAM_OUTER_SPACING);
  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_START);
  set_margin_top(EDITTEAM_TOP_MARGIN);

  m_grid.set_row_spacing(EDITTEAM_GRID_ROW_SPACING);
  m_grid.set_column_spacing(EDITTEAM_GRID_COL_SPACING);
  m_grid.set_halign(Gtk::ALIGN_CENTER);

  m_nameEntry.set_text(m_team.name);
  m_leagueEntry.set_text(m_team.league);
  m_teamCodeEntry.set_text(m_team.teamCode);
  m_homeAwayEntry.set_text(m_team.homeAway.empty() ? "home" : m_team.homeAway);
  m_nextGameUrlEntry.set_text(m_team.nextGameUrlTemplate);
  m_nextGameParserEntry.set_text(m_team.nextGameParser);
  m_liveGameUrlEntry.set_text(m_team.liveGameUrlTemplate);
  m_liveGameParserEntry.set_text(m_team.liveGameParser);
  m_apiTeamIdEntry.set_text(m_team.apiTeamId);
  m_themeNameEntry.set_text(m_team.themeName);
  m_themeIdEntry.set_text(std::to_string(m_team.themeID));
  m_iconPathEntry.set_text(m_team.iconPath);
  m_displayOrderEntry.set_text(std::to_string(m_team.displayOrder));
  m_enabledCheck.set_active(m_team.enabled != 0);
  m_nextGameUrlEntry.set_width_chars(28);
  m_liveGameUrlEntry.set_width_chars(28);

  if (isAddMode()) {
    buildAddWizardUi();
    return;
  }

  m_color1Entry.set_max_length(6);
  m_color2Entry.set_max_length(6);

  if (!m_team.themeName.empty()) {
    auto themes = readThemeColors(std::string(SETTINGS_PATH));

    auto it =
        std::find_if(themes.begin(), themes.end(), [this](const Theme &theme) {
          return theme.name == m_team.themeName;
        });

    if (it != themes.end()) {
      if (it->colors.size() > 0) {
        m_color1Entry.set_text(
            rgbToHex(it->colors[0].r, it->colors[0].g, it->colors[0].b));
      }
      if (it->colors.size() > 1) {
        m_color2Entry.set_text(
            rgbToHex(it->colors[1].r, it->colors[1].g, it->colors[1].b));
      }
    }
  }

  buildEditWizardUi();
}

void EditTeam::setLogoStatus(const std::string &text) {
  m_logoStatusLabel.set_text(text);
}

void EditTeam::showLogoLoading() { setLogoStatus("Loading image..."); }

void EditTeam::showLogoInvalid() { setLogoStatus("Invalid image link"); }

void EditTeam::showLogoEmpty() { setLogoStatus("No image selected"); }

bool EditTeam::isAddMode() const { return m_team.id == 0; }

void EditTeam::buildEditWizardUi() {
  LOG_INFO() << "EditTeam edit wizard";

  auto setupGrid = [](Gtk::Grid &grid) {
    grid.set_row_spacing(EDITTEAM_GRID_ROW_SPACING);
    grid.set_column_spacing(EDITTEAM_GRID_COL_SPACING);
    grid.set_halign(Gtk::ALIGN_CENTER);
    grid.set_valign(Gtk::ALIGN_START);
  };

  setupGrid(m_editMainGrid);
  setupGrid(m_editDetailsGrid);

  m_gameDayAnimationsEntry.set_width_chars(28);
  m_homeScoreAnimationsEntry.set_width_chars(28);
  m_blowoutAnimationsEntry.set_width_chars(28);
  m_lopsidedAnimationsEntry.set_width_chars(28);

  int row = 0;
  attach_row(m_editMainGrid, row, "Name", m_nameEntry);
  attach_row(m_editMainGrid, row, "League", m_leagueEntry);
  attach_row(m_editMainGrid, row, "Team Code", m_teamCodeEntry);
  attach_row(m_editMainGrid, row, "Home/Away", m_homeAwayEntry);
  attach_row(m_editMainGrid, row, "API Team ID", m_apiTeamIdEntry);
  attach_row(m_editMainGrid, row, "Enabled", m_enabledCheck);
  attach_row(m_editMainGrid, row, "Display Order", m_displayOrderEntry);
  attach_row(m_editMainGrid, row, "Theme Name", m_themeNameEntry);
  attach_row(m_editMainGrid, row, "Theme ID", m_themeIdEntry);
  attach_row(m_editMainGrid, row, "Icon Path", m_iconPathEntry);

  row = 0;
  attach_row(m_editDetailsGrid, row, "Next Game URL", m_nextGameUrlEntry);
  attach_row(m_editDetailsGrid, row, "Next Parser", m_nextGameParserEntry);
  attach_row(m_editDetailsGrid, row, "Live Game URL", m_liveGameUrlEntry);
  attach_row(m_editDetailsGrid, row, "Live Parser", m_liveGameParserEntry);
  attach_row(m_editDetailsGrid, row, "Game Day Anim",
             m_gameDayAnimationsEntry);
  attach_row(m_editDetailsGrid, row, "Score Anim",
             m_homeScoreAnimationsEntry);
  attach_row(m_editDetailsGrid, row, "Blowout Anim",
             m_blowoutAnimationsEntry);
  attach_row(m_editDetailsGrid, row, "Lopsided Anim",
             m_lopsidedAnimationsEntry);
  attach_row(m_editDetailsGrid, row, "Color 1",
             *make_hex_entry_row(m_color1Entry));
  attach_row(m_editDetailsGrid, row, "Color 2",
             *make_hex_entry_row(m_color2Entry));

  auto logoUrlLabel = Gtk::manage(new Gtk::Label("Logo URL"));
  logoUrlLabel->set_halign(Gtk::ALIGN_CENTER);

  auto logoHint = Gtk::manage(
      new Gtk::Label("Type/paste image link, or right click paste area"));
  logoHint->set_halign(Gtk::ALIGN_CENTER);

  m_logoUrlEntry.set_hexpand(true);
  m_logoUrlEntry.set_width_chars(28);

  m_logoPreviewBox.set_spacing(10);
  m_logoPreviewBox.set_border_width(8);
  m_logoPreviewBox.set_halign(Gtk::ALIGN_CENTER);
  m_logoPreviewBox.set_valign(Gtk::ALIGN_START);

  m_logoStatusLabel.set_halign(Gtk::ALIGN_CENTER);
  m_logoStatusLabel.set_valign(Gtk::ALIGN_CENTER);

  m_logoPreview.set_size_request(EDITTEAM_LOGO_PREVIEW_SIZE,
                                 EDITTEAM_LOGO_PREVIEW_SIZE);
  m_logoPreview.set_halign(Gtk::ALIGN_CENTER);
  m_logoPreview.set_valign(Gtk::ALIGN_CENTER);

  m_logoAreaBox.set_spacing(8);
  m_logoAreaBox.set_border_width(8);
  m_logoAreaBox.set_halign(Gtk::ALIGN_CENTER);
  m_logoAreaBox.set_valign(Gtk::ALIGN_CENTER);
  m_logoAreaBox.pack_start(m_logoStatusLabel, Gtk::PACK_SHRINK);
  m_logoAreaBox.pack_start(m_logoPreview, Gtk::PACK_SHRINK);

  m_logoDropArea.set_visible_window(true);
  m_logoDropArea.set_above_child(false);
  m_logoDropArea.set_size_request(EDITTEAM_LOGO_DROP_SIZE);
  m_logoDropArea.add_events(Gdk::BUTTON_PRESS_MASK);
  m_logoDropArea.add(m_logoAreaBox);
  m_logoDropArea.signal_button_press_event().connect(
      sigc::mem_fun(*this, &EditTeam::onLogoAreaButtonPress), false);

  m_logoPreviewFrame.set_shadow_type(Gtk::SHADOW_IN);
  m_logoPreviewFrame.add(m_logoDropArea);

  m_deleteLogoBtn = Gtk::manage(
      new ImageButton(m_iconPath + "/trash.png", EDITTEAM_DELETE_LOGO_SIZE));

  m_logoActionBox.set_spacing(8);
  m_logoActionBox.set_halign(Gtk::ALIGN_CENTER);
  m_logoActionBox.pack_start(*m_deleteLogoBtn, Gtk::PACK_SHRINK);

  m_logoPreviewBox.pack_start(*logoUrlLabel, Gtk::PACK_SHRINK);
  m_logoPreviewBox.pack_start(m_logoUrlEntry, Gtk::PACK_SHRINK);
  m_logoPreviewBox.pack_start(*logoHint, Gtk::PACK_SHRINK);
  m_logoPreviewBox.pack_start(m_logoPreviewFrame, Gtk::PACK_SHRINK);
  m_logoPreviewBox.pack_start(m_logoActionBox, Gtk::PACK_SHRINK);

  m_logoFrame.add(m_logoPreviewBox);
  m_logoFrame.set_size_request(EDITTEAM_LOGO_FRAME_W, EDITTEAM_LOGO_FRAME_H);
  m_logoFrame.set_shadow_type(Gtk::SHADOW_ETCHED_IN);

  loadExistingLogoPreview();
  loadAnimationEntries();

  m_editPageMain.set_halign(Gtk::ALIGN_CENTER);
  m_editPageMain.set_valign(Gtk::ALIGN_START);
  m_editPageMain.pack_start(m_editMainGrid, Gtk::PACK_SHRINK);

  m_editPageDetails.set_spacing(EDITTEAM_BODY_SPACING);
  m_editPageDetails.set_halign(Gtk::ALIGN_CENTER);
  m_editPageDetails.set_valign(Gtk::ALIGN_START);
  m_editPageDetails.pack_start(m_editDetailsGrid, Gtk::PACK_SHRINK);
  m_editPageDetails.pack_start(m_logoFrame, Gtk::PACK_SHRINK);

  m_editStack.set_transition_type(Gtk::STACK_TRANSITION_TYPE_NONE);
  m_editStack.set_halign(Gtk::ALIGN_CENTER);
  m_editStack.set_valign(Gtk::ALIGN_START);
  m_editStack.add(m_editPageMain, "main");
  m_editStack.add(m_editPageDetails, "details");

  m_editPrevBtn.set_size_request(130, 72);
  m_editNextBtn.set_size_request(130, 72);
  m_editCancelBtn.set_size_request(130, 72);
  m_editSaveBtn.set_size_request(130, 72);
  m_editPrevBtn.set_can_focus(false);
  m_editNextBtn.set_can_focus(false);
  m_editCancelBtn.set_can_focus(false);
  m_editSaveBtn.set_can_focus(false);

  m_deleteBtn = Gtk::manage(
      new ImageButton(m_iconPath + "/trash.png", EDITTEAM_DELETE_LOGO_SIZE));

  m_editNavBox.set_spacing(EDITTEAM_BUTTON_SPACING);
  m_editNavBox.set_halign(Gtk::ALIGN_CENTER);
  m_editNavBox.pack_start(m_editPrevBtn, Gtk::PACK_SHRINK);
  m_editNavBox.pack_start(m_editNextBtn, Gtk::PACK_SHRINK);
  m_editNavBox.pack_start(m_editCancelBtn, Gtk::PACK_SHRINK);
  m_editNavBox.pack_start(m_editSaveBtn, Gtk::PACK_SHRINK);
  m_editNavBox.pack_start(*m_deleteBtn, Gtk::PACK_SHRINK);

  pack_start(m_editStack, Gtk::PACK_SHRINK);
  pack_start(m_editNavBox, Gtk::PACK_SHRINK);

  m_editPrevBtn.signal_clicked().connect([this]() { onEditPrev(); });
  m_editNextBtn.signal_clicked().connect([this]() { onEditNext(); });
  m_editCancelBtn.signal_clicked().connect([this]() { m_signalCancel.emit(); });
  m_editSaveBtn.signal_clicked().connect([this]() { on_save(); });
  m_deleteBtn->signal_clicked().connect([this]() {
    if (m_team.id > 0 && deleteTeam(m_teamsDbPath, m_team.id))
      m_signalDeleted.emit();
  });
  m_deleteLogoBtn->signal_clicked().connect([this]() { clearLogoPreview(); });
  m_logoUrlEntry.signal_changed().connect([this]() { onLogoUrlChanged(); });

  showEditStep(0);
  show_all_children();
}

void EditTeam::buildAddWizardUi() {
  LOG_INFO() << "EditTeam add wizard";

  m_addStack.set_transition_type(Gtk::STACK_TRANSITION_TYPE_NONE);
  m_addStack.set_halign(Gtk::ALIGN_CENTER);
  m_addStack.set_valign(Gtk::ALIGN_START);

  auto setupGrid = [](Gtk::Grid &grid) {
    grid.set_row_spacing(EDITTEAM_GRID_ROW_SPACING + 4);
    grid.set_column_spacing(EDITTEAM_GRID_COL_SPACING);
    grid.set_halign(Gtk::ALIGN_CENTER);
    grid.set_valign(Gtk::ALIGN_START);
  };

  setupGrid(m_addBasicGrid);
  setupGrid(m_addApiGrid);
  setupGrid(m_addUrlsGrid);

  m_addPageBasic.set_spacing(EDITTEAM_FORM_SPACING);
  m_addPageApi.set_spacing(EDITTEAM_FORM_SPACING);
  m_addPageUrls.set_spacing(EDITTEAM_FORM_SPACING);
  m_addPageBasic.set_halign(Gtk::ALIGN_CENTER);
  m_addPageApi.set_halign(Gtk::ALIGN_CENTER);
  m_addPageUrls.set_halign(Gtk::ALIGN_CENTER);

  int row = 0;
  attach_row(m_addBasicGrid, row, "Name", m_nameEntry);
  attach_row(m_addBasicGrid, row, "League", m_leagueEntry);
  attach_row(m_addBasicGrid, row, "Team Code", m_teamCodeEntry);
  attach_row(m_addBasicGrid, row, "Home/Away", m_homeAwayEntry);
  attach_row(m_addBasicGrid, row, "Enabled", m_enabledCheck);
  attach_row(m_addBasicGrid, row, "Display Order", m_displayOrderEntry);

  row = 0;
  attach_row(m_addApiGrid, row, "API Team ID", m_apiTeamIdEntry);
  attach_row(m_addApiGrid, row, "Next Parser", m_nextGameParserEntry);
  attach_row(m_addApiGrid, row, "Live Parser", m_liveGameParserEntry);

  row = 0;
  attach_row(m_addUrlsGrid, row, "Next URL", m_nextGameUrlEntry);
  attach_row(m_addUrlsGrid, row, "Live URL", m_liveGameUrlEntry);
  attach_row(m_addUrlsGrid, row, "Theme Name", m_themeNameEntry);
  attach_row(m_addUrlsGrid, row, "Theme ID", m_themeIdEntry);
  attach_row(m_addUrlsGrid, row, "Icon Path", m_iconPathEntry);

  m_addPageBasic.pack_start(m_addBasicGrid, Gtk::PACK_SHRINK);
  m_addPageApi.pack_start(m_addApiGrid, Gtk::PACK_SHRINK);
  m_addPageUrls.pack_start(m_addUrlsGrid, Gtk::PACK_SHRINK);

  m_addStack.add(m_addPageBasic, "basic");
  m_addStack.add(m_addPageApi, "api");
  m_addStack.add(m_addPageUrls, "urls");

  m_addBackBtn.set_size_request(130, 72);
  m_addNextBtn.set_size_request(130, 72);
  m_addCancelBtn.set_size_request(130, 72);
  m_addSaveBtn.set_size_request(130, 72);
  m_addBackBtn.set_can_focus(false);
  m_addNextBtn.set_can_focus(false);
  m_addCancelBtn.set_can_focus(false);
  m_addSaveBtn.set_can_focus(false);

  m_addNavBox.set_spacing(EDITTEAM_BUTTON_SPACING);
  m_addNavBox.set_halign(Gtk::ALIGN_CENTER);
  m_addNavBox.pack_start(m_addBackBtn, Gtk::PACK_SHRINK);
  m_addNavBox.pack_start(m_addNextBtn, Gtk::PACK_SHRINK);
  m_addNavBox.pack_start(m_addCancelBtn, Gtk::PACK_SHRINK);
  m_addNavBox.pack_start(m_addSaveBtn, Gtk::PACK_SHRINK);

  pack_start(m_addStack, Gtk::PACK_SHRINK);
  pack_start(m_addNavBox, Gtk::PACK_SHRINK);

  m_addBackBtn.signal_clicked().connect([this]() { onAddBack(); });
  m_addNextBtn.signal_clicked().connect([this]() { onAddNext(); });
  m_addCancelBtn.signal_clicked().connect([this]() { m_signalCancel.emit(); });
  m_addSaveBtn.signal_clicked().connect([this]() { on_save(); });

  showAddStep(0);
  show_all_children();
}

void EditTeam::showAddStep(int step) {
  m_addStep = std::clamp(step, 0, 2);

  if (m_addStep == 0)
    m_addStack.set_visible_child("basic");
  else if (m_addStep == 1)
    m_addStack.set_visible_child("api");
  else
    m_addStack.set_visible_child("urls");

  updateAddNavButtons();
}

void EditTeam::updateAddNavButtons() {
  m_addBackBtn.set_sensitive(m_addStep > 0);
  m_addNextBtn.set_visible(m_addStep < 2);
  m_addSaveBtn.set_visible(m_addStep == 2);
}

void EditTeam::onAddBack() { showAddStep(m_addStep - 1); }

void EditTeam::onAddNext() {
  std::string message;
  if (!validateAddStep(m_addStep, message)) {
    m_signalValidationFailed.emit(message);
    return;
  }

  showAddStep(m_addStep + 1);
}

void EditTeam::showEditStep(int step) {
  m_editStep = std::clamp(step, 0, 1);

  if (m_editStep == 0)
    m_editStack.set_visible_child("main");
  else
    m_editStack.set_visible_child("details");

  updateEditNavButtons();
}

void EditTeam::updateEditNavButtons() {
  m_editPrevBtn.set_sensitive(m_editStep > 0);
  m_editNextBtn.set_visible(m_editStep == 0);
  m_editSaveBtn.set_visible(m_editStep == 1);
  if (m_deleteBtn)
    m_deleteBtn->set_visible(m_editStep == 1);
}

void EditTeam::onEditPrev() { showEditStep(m_editStep - 1); }

void EditTeam::onEditNext() {
  std::string message;
  if (!validateFields(message)) {
    m_signalValidationFailed.emit(message);
    return;
  }

  showEditStep(m_editStep + 1);
}

bool EditTeam::validateAddStep(int step, std::string &message) const {
  auto trim = [](std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
      return std::string{};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
  };

  if (step == 0) {
    if (trim(m_nameEntry.get_text()).empty()) {
      message = "Name needs to be filled in.";
      return false;
    }
    if (trim(m_leagueEntry.get_text()).empty()) {
      message = "League needs to be filled in.";
      return false;
    }
    if (trim(m_teamCodeEntry.get_text()).empty()) {
      message = "Team Code needs to be filled in.";
      return false;
    }
  }

  return true;
}

void EditTeam::onLogoUrlChanged() {
  const std::string url = m_logoUrlEntry.get_text();

  if (url.empty()) {
    if (!m_logoPixbuf)
      showLogoEmpty();
    return;
  }

  if (!loadLogoPreviewFromUrl(url)) {
    m_signalValidationFailed.emit("Invalid image link.");
  }
}

bool EditTeam::loadLogoPreviewFromUrl(const std::string &url) {
  if (url.empty()) {
    showLogoEmpty();
    return false;
  }

  showLogoLoading();

  HttpHelper http;
  const auto imageData = http.getBytes(url);
  if (imageData.empty()) {
    showLogoInvalid();
    LOG_WARN() << "Failed to fetch image bytes from " << url;
    return false;
  }

  try {
    auto loader = Gdk::PixbufLoader::create();
    loader->write(imageData.data(), imageData.size());
    loader->close();

    auto pixbuf = loader->get_pixbuf();
    if (!pixbuf) {
      showLogoInvalid();
      LOG_WARN() << "Failed to decode image from " << url;
      return false;
    }

    updateLogoPreview(pixbuf);
    LOG_INFO() << "Loaded logo preview from URL " << url;
    return true;
  } catch (...) {
    showLogoInvalid();
    LOG_WARN() << "Exception decoding image from URL " << url;
    return false;
  }
}

bool EditTeam::hexToRgb(const std::string &hexIn, int &r, int &g, int &b) {
  std::string hex = hexIn;
  if (!hex.empty() && hex[0] == '#')
    hex.erase(0, 1);

  if (hex.size() != 6)
    return false;

  for (char c : hex) {
    if (!std::isxdigit(static_cast<unsigned char>(c)))
      return false;
  }

  r = std::stoi(hex.substr(0, 2), nullptr, 16);
  g = std::stoi(hex.substr(2, 2), nullptr, 16);
  b = std::stoi(hex.substr(4, 2), nullptr, 16);
  return true;
}

std::string EditTeam::normalizeTeamFileName(const std::string &name) {
  std::string s = name;

  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (s.rfind("the ", 0) == 0)
    s.erase(0, 4);

  for (char &c : s) {
    if (c == ' ')
      c = '_';
  }

  return s + "_logo.png";
}

std::string EditTeam::normalizeParserFileName(const std::string &name) {
  std::string s = name;

  // trim
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";

  const auto last = s.find_last_not_of(" \t\r\n");
  s = s.substr(first, last - first + 1);

  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  for (char &c : s) {
    if (c == ' ')
      c = '_';
  }

  return s;
}

void EditTeam::updateLogoPreview(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf) {
  m_logoPixbuf = pixbuf;

  if (!m_logoPixbuf) {
    m_logoPreview.clear();
    showLogoEmpty();
    return;
  }

  auto scaled = m_logoPixbuf->scale_simple(180, 180, Gdk::INTERP_BILINEAR);
  m_logoPreview.set(scaled);
  setLogoStatus("Image loaded");
}

void EditTeam::clearLogoPreview() {
  m_logoPixbuf.reset();
  m_logoPreview.clear();
  m_logoUrlEntry.set_text("");
  showLogoEmpty();
  LOG_INFO() << "Cleared logo preview";
}

bool EditTeam::onLogoAreaButtonPress(GdkEventButton *event) {
  if (!event)
    return false;

  if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
    pasteLogoFromClipboard();
    return true;
  }

  return false;
}

bool EditTeam::pasteLogoFromClipboard() {
  auto clipboard = Gtk::Clipboard::get();
  auto pixbuf = clipboard->wait_for_image();

  if (!pixbuf) {
    showLogoInvalid();
    LOG_WARN() << "Clipboard does not contain an image";
    return false;
  }

  updateLogoPreview(pixbuf);
  LOG_INFO() << "Pasted logo image into preview";
  return true;
}

bool EditTeam::saveLogoPreviewToDisk() {
  if (!m_logoPixbuf)
    return true;

  const std::string filePath = std::string(ICON_PATH) + "/" +
                               normalizeTeamFileName(m_nameEntry.get_text());

  try {
    m_logoPixbuf->save(filePath, "png");
    LOG_INFO() << "Saved logo preview to " << filePath;
    return true;
  } catch (...) {
    LOG_ERROR() << "Failed to save logo preview to " << filePath;
    return false;
  }
}

bool EditTeam::saveThemeColorsByThemeName(const std::string &themeName,
                                          const std::string &hex1,
                                          const std::string &hex2) {
  auto themes = readThemeColors(std::string(SETTINGS_PATH));

  auto it = std::find_if(
      themes.begin(), themes.end(),
      [&themeName](const Theme &theme) { return theme.name == themeName; });

  if (it == themes.end()) {
    LOG_ERROR() << "Theme name not found: " << themeName;
    return false;
  }

  int r1 = 0, g1 = 0, b1 = 0;
  int r2 = 0, g2 = 0, b2 = 0;

  if (!hexToRgb(hex1, r1, g1, b1) || !hexToRgb(hex2, r2, g2, b2)) {
    LOG_ERROR() << "Invalid hex color input";
    return false;
  }

  if (it->colors.size() < 2)
    it->colors.resize(2);

  it->colors[0] = RGB_Color{static_cast<uint8_t>(r1), static_cast<uint8_t>(g1),
                            static_cast<uint8_t>(b1)};

  it->colors[1] = RGB_Color{static_cast<uint8_t>(r2), static_cast<uint8_t>(g2),
                            static_cast<uint8_t>(b2)};

  writeThemeColors(std::string(SETTINGS_PATH), themes);
  LOG_INFO() << "Saved colors to theme " << themeName;

  return true;
}

bool EditTeam::saveLogoFromUrl(const std::string &url) {
  if (url.empty())
    return true;

  showLogoLoading();

  HttpHelper http;
  const auto imageData = http.getBytes(url);
  if (imageData.empty()) {
    showLogoInvalid();
    LOG_WARN() << "Failed to fetch logo from URL " << url;
    return false;
  }

  try {
    auto loader = Gdk::PixbufLoader::create();
    loader->write(imageData.data(), imageData.size());
    loader->close();

    auto pixbuf = loader->get_pixbuf();
    if (!pixbuf) {
      showLogoInvalid();
      LOG_WARN() << "Failed to decode logo from URL " << url;
      return false;
    }

    const std::string filePath = std::string(ICON_PATH) + "/" +
                                 normalizeTeamFileName(m_nameEntry.get_text());

    pixbuf->save(filePath, "png");
    updateLogoPreview(pixbuf);

    LOG_INFO() << "Saved logo from URL to " << filePath;
    return true;
  } catch (...) {
    showLogoInvalid();
    LOG_ERROR() << "Failed saving logo from URL " << url;
    return false;
  }
}

void EditTeam::on_save() {
  std::string validationMessage;
  if (!validateFields(validationMessage)) {
    m_signalValidationFailed.emit(validationMessage);
    return;
  }

  m_team.name = m_nameEntry.get_text();
  m_team.league = m_leagueEntry.get_text();
  m_team.teamCode = m_teamCodeEntry.get_text();
  m_team.homeAway = m_homeAwayEntry.get_text();
  m_team.nextGameUrlTemplate = m_nextGameUrlEntry.get_text();
  m_team.nextGameParser =
      normalizeParserFileName(m_nextGameParserEntry.get_text());
  m_team.liveGameUrlTemplate = m_liveGameUrlEntry.get_text();
  m_team.liveGameParser =
      normalizeParserFileName(m_liveGameParserEntry.get_text());
  m_team.apiTeamId = m_apiTeamIdEntry.get_text();
  m_team.themeName = m_themeNameEntry.get_text();
  m_team.iconPath = m_iconPathEntry.get_text();
  m_team.enabled = m_enabledCheck.get_active() ? 1 : 0;

  try {
    m_team.themeID = std::stoi(m_themeIdEntry.get_text());
  } catch (...) {
    m_team.themeID = 0;
  }

  try {
    m_team.displayOrder = std::stoi(m_displayOrderEntry.get_text());
  } catch (...) {
    m_team.displayOrder = 0;
  }

  const bool teamOk = writeTeam(m_teamsDbPath, m_team);
  if (!teamOk) {
    LOG_ERROR() << "Failed to save team " << m_team.name;
    m_signalValidationFailed.emit("Team save failed.");
    return;
  }

  const bool animOk = teamOk && writeTeamAnimations(m_teamsDbPath, m_team.id,
                                                    collectAnimations());
  const bool hasThemeColors = !m_team.themeName.empty() &&
                              !m_color1Entry.get_text().empty() &&
                              !m_color2Entry.get_text().empty();
  const bool colorsOk =
      hasThemeColors ? saveThemeColorsByThemeName(m_team.themeName,
                                                  m_color1Entry.get_text(),
                                                  m_color2Entry.get_text())
                     : true;
  const bool logoOk = !m_logoUrlEntry.get_text().empty()
                          ? saveLogoFromUrl(m_logoUrlEntry.get_text())
                          : saveLogoPreviewToDisk();

  if (!animOk) {
    LOG_ERROR() << "Failed to save team animations for " << m_team.name;
    m_signalValidationFailed.emit("Team saved, but animations failed.");
    return;
  }

  if (!colorsOk) {
    m_signalValidationFailed.emit("Team saved, but theme colors failed.");
    return;
  }

  if (!logoOk) {
    m_signalValidationFailed.emit("Team saved, but logo failed.");
    return;
  }

  if (teamOk && animOk && colorsOk && logoOk) {
    if (!m_logoUrlEntry.get_text().empty() && !logoOk) {
      LOG_WARN()
          << "Team saved, colors saved, preview saved, logo URL save failed";
    }
    m_signalSaved.emit();
  }
}

void EditTeam::loadExistingLogoPreview() {
  if (m_team.name.empty()) {
    showLogoEmpty();
    return;
  }

  const std::string filePath =
      std::string(ICON_PATH) + "/" + normalizeTeamFileName(m_team.name);

  try {
    auto pixbuf = Gdk::Pixbuf::create_from_file(filePath);
    if (pixbuf) {
      updateLogoPreview(pixbuf);
      LOG_INFO() << "Loaded existing logo preview from " << filePath;
      return;
    }
  } catch (...) {
  }

  showLogoEmpty();
  LOG_INFO() << "No existing logo found for " << m_team.name << " at "
             << filePath;
}

bool EditTeam::validateFields(std::string &message) const {
  auto trim = [](std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
      return std::string{};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
  };

  if (trim(m_nameEntry.get_text()).empty()) {
    message = "Name needs to be filled in.";
    return false;
  }

  if (trim(m_leagueEntry.get_text()).empty()) {
    message = "League needs to be filled in.";
    return false;
  }

  if (trim(m_teamCodeEntry.get_text()).empty()) {
    message = "Team Code needs to be filled in.";
    return false;
  }

  int dummyR = 0, dummyG = 0, dummyB = 0;

  if (!trim(m_color1Entry.get_text()).empty() &&
      !hexToRgb(m_color1Entry.get_text(), dummyR, dummyG, dummyB)) {
    message = "Color 1 must be a valid 6-digit hex value.";
    return false;
  }

  if (!trim(m_color2Entry.get_text()).empty() &&
      !hexToRgb(m_color2Entry.get_text(), dummyR, dummyG, dummyB)) {
    message = "Color 2 must be a valid 6-digit hex value.";
    return false;
  }

  return true;
}

std::vector<std::string>
EditTeam::splitAnimationPaths(const std::string &text) {
  std::vector<std::string> out;
  std::stringstream ss(text);
  std::string item;

  while (std::getline(ss, item, ';')) {
    const auto first = item.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
      continue;

    const auto last = item.find_last_not_of(" \t\r\n");
    out.push_back(item.substr(first, last - first + 1));
  }

  return out;
}

std::string EditTeam::joinAnimationPaths(
    const std::vector<TeamAnimation> &items, const std::string &type) {
  std::string out;
  for (const auto &a : items) {
    if (a.animationType != type)
      continue;

    if (!out.empty())
      out += "; ";
    out += a.filePath;
  }
  return out;
}

std::vector<TeamAnimation> EditTeam::collectAnimations() const {
  std::vector<TeamAnimation> out;

  const auto addType = [&](const std::string &type, const std::string &text) {
    const auto paths = splitAnimationPaths(text);
    int order = 0;
    for (const auto &path : paths) {
      TeamAnimation a;
      a.teamId = m_team.id;
      a.animationType = type;
      a.filePath = path;
      a.enabled = 1;
      a.displayOrder = order++;
      out.push_back(a);
    }
  };

  addType("game_day_hourly", m_gameDayAnimationsEntry.get_text());
  addType("home_score", m_homeScoreAnimationsEntry.get_text());
  addType("blowout", m_blowoutAnimationsEntry.get_text());
  addType("lopsided", m_lopsidedAnimationsEntry.get_text());
  return out;
}

void EditTeam::loadAnimationEntries() {
  if (m_team.id <= 0)
    return;

  const auto animations = readTeamAnimations(m_teamsDbPath, m_team.id);
  m_gameDayAnimationsEntry.set_text(
      joinAnimationPaths(animations, "game_day_hourly"));
  m_homeScoreAnimationsEntry.set_text(
      joinAnimationPaths(animations, "home_score"));
  m_blowoutAnimationsEntry.set_text(joinAnimationPaths(animations, "blowout"));
  m_lopsidedAnimationsEntry.set_text(
      joinAnimationPaths(animations, "lopsided"));
}

sigc::signal<void, std::string> &EditTeam::signal_validation_failed() {
  return m_signalValidationFailed;
}

sigc::signal<void> &EditTeam::signal_saved() { return m_signalSaved; }

sigc::signal<void> &EditTeam::signal_deleted() { return m_signalDeleted; }

sigc::signal<void> &EditTeam::signal_cancel() { return m_signalCancel; }
