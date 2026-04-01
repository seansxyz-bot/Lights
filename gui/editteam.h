#pragma once

#include <gtkmm.h>
#include <iomanip>
#include <sigc++/sigc++.h>
#include <string>

#include "../tools/settingsrw.h"

class ImageButton;

class EditTeam : public Gtk::Box {
public:
  EditTeam(const std::string &iconPath, const std::string &teamsDbPath,
           const TeamRecord &team);
  virtual ~EditTeam() = default;

  sigc::signal<void, std::string> &signal_validation_failed();
  sigc::signal<void> &signal_saved();
  sigc::signal<void> &signal_cancel();

private:
  void on_save();

  static bool hexToRgb(const std::string &hex, int &r, int &g, int &b);
  static std::string normalizeTeamFileName(const std::string &name);
  static std::string normalizeParserFileName(const std::string &name);

  bool saveThemeColorsByThemeName(const std::string &themeName,
                                  const std::string &hex1,
                                  const std::string &hex2);
  bool pasteLogoFromClipboard();
  bool saveLogoPreviewToDisk();
  bool saveLogoFromUrl(const std::string &url);
  bool loadLogoPreviewFromUrl(const std::string &url);
  void onLogoUrlChanged();
  void updateLogoPreview(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf);
  void loadExistingLogoPreview();
  bool validateFields(std::string &message) const;

  void clearLogoPreview();
  bool onLogoAreaButtonPress(GdkEventButton *event);

  void setLogoStatus(const std::string &text);
  void showLogoLoading();
  void showLogoInvalid();
  void showLogoEmpty();

private:
  std::string m_iconPath;
  std::string m_teamsDbPath;
  TeamRecord m_team;

  Gtk::Box m_bodyBox{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box m_formBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Grid m_grid;
  Gtk::Box m_buttonBox{Gtk::ORIENTATION_HORIZONTAL};

  Gtk::Entry m_nameEntry;
  Gtk::Entry m_leagueEntry;
  Gtk::Entry m_teamCodeEntry;
  Gtk::Entry m_nextGameUrlEntry;
  Gtk::Entry m_nextGameParserEntry;
  Gtk::Entry m_liveGameUrlEntry;
  Gtk::Entry m_liveGameParserEntry;
  Gtk::Entry m_apiTeamIdEntry;
  Gtk::Entry m_themeNameEntry;

  Gtk::Entry m_color1Entry;
  Gtk::Entry m_color2Entry;
  Gtk::Entry m_logoUrlEntry;

  Gtk::Frame m_logoFrame{"LOGO"};
  Gtk::Box m_logoPreviewBox{Gtk::ORIENTATION_VERTICAL};

  Gtk::Frame m_logoPreviewFrame{"PASTE AREA"};
  Gtk::EventBox m_logoDropArea;
  Gtk::Box m_logoAreaBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Label m_logoStatusLabel{"No image selected"};
  Gtk::Image m_logoPreview;

  Gtk::Box m_logoActionBox{Gtk::ORIENTATION_HORIZONTAL};

  ImageButton *m_deleteLogoBtn = nullptr;

  Glib::RefPtr<Gdk::Pixbuf> m_logoPixbuf;

  ImageButton *m_okBtn = nullptr;
  ImageButton *m_cancelBtn = nullptr;

  sigc::signal<void> m_signalSaved;
  sigc::signal<void> m_signalCancel;
  sigc::signal<void, std::string> m_signalValidationFailed;
};
