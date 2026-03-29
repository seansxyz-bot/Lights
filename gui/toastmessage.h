#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

class ToastMessage : public Gtk::Frame {
public:
  struct Action {
    std::string label;      // used if iconPath is empty
    std::string iconPath;   // optional image button
    int iconPixelSize = 72; // only used when iconPath is set
    int margin = 0;
    int padding = 0;
    std::string cssClass;      // optional css class
    sigc::slot<void> callback; // clicked action
  };

public:
  ToastMessage();
  virtual ~ToastMessage() = default;

  void showMessage(const std::string &text,
                   const std::vector<Action> &actions = {});
  void setText(const std::string &text);
  std::string text() const;

  void hideMessage();
  void clearActions();

private:
  Gtk::Box m_mainBox{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Label m_label;
  Gtk::Box m_actionBox{Gtk::ORIENTATION_HORIZONTAL};

  std::vector<Gtk::Widget *> m_actionWidgets;
};
