#include "toastmessage.h"

#include "imgbutton.h"

ToastMessage::ToastMessage() {
  set_no_show_all(true);
  set_shadow_type(Gtk::SHADOW_ETCHED_OUT);
  set_halign(Gtk::ALIGN_CENTER);
  set_valign(Gtk::ALIGN_END);
  set_margin_bottom(20);

  m_mainBox.set_spacing(10);
  m_mainBox.set_margin_top(8);
  m_mainBox.set_margin_bottom(8);
  m_mainBox.set_margin_start(12);
  m_mainBox.set_margin_end(12);

  m_label.set_halign(Gtk::ALIGN_START);
  m_label.set_valign(Gtk::ALIGN_CENTER);
  m_label.set_hexpand(true);

  m_actionBox.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
  m_actionBox.set_spacing(8);
  m_actionBox.set_halign(Gtk::ALIGN_END);
  m_actionBox.set_valign(Gtk::ALIGN_CENTER);

  m_mainBox.pack_start(m_label, true, true, 0);
  m_mainBox.pack_start(m_actionBox, false, false, 0);

  add(m_mainBox);
  hide();
}

void ToastMessage::showMessage(const std::string &text,
                               const std::vector<Action> &actions) {
  setText(text);
  clearActions();

  for (const auto &action : actions) {
    Gtk::Widget *widget = nullptr;

    if (!action.iconPath.empty()) {
      auto *btn =
          Gtk::manage(new ImageButton(action.iconPath, action.iconPixelSize,
                                      action.margin, action.padding));
      btn->set_can_focus(false);

      if (!action.cssClass.empty()) {
        btn->get_style_context()->add_class(action.cssClass);
      }

      btn->signal_clicked().connect(action.callback);
      widget = btn;
    } else {
      auto *btn = Gtk::manage(new Gtk::Button(action.label));
      btn->set_can_focus(false);

      if (!action.cssClass.empty()) {
        btn->get_style_context()->add_class(action.cssClass);
      }

      btn->signal_clicked().connect(action.callback);
      widget = btn;
    }

    if (widget) {
      m_actionBox.pack_start(*widget, false, false, 0);
      m_actionWidgets.push_back(widget);
    }
  }

  show();
  show_all_children();
  queue_draw();
}

void ToastMessage::setText(const std::string &text) { m_label.set_text(text); }

std::string ToastMessage::text() const { return m_label.get_text(); }

void ToastMessage::hideMessage() {
  hide();
  clearActions();
}

void ToastMessage::clearActions() {
  for (auto *widget : m_actionWidgets) {
    if (widget && widget->get_parent() == &m_actionBox) {
      m_actionBox.remove(*widget);
    }
  }

  m_actionWidgets.clear();
}
