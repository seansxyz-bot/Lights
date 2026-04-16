#include "iostream"
#include "mainwindow.h"
#include <gtkmm/application.h>

void load_css() {
  if (!gtk_init_check(nullptr, nullptr)) {
    std::cerr << "GTK not initialized properly.\n";
    return;
  }

  GtkCssProvider *provider = gtk_css_provider_new();
  GdkDisplay *display = gdk_display_get_default();
  if (!GDK_IS_DISPLAY(display)) {
    std::cerr << "Invalid GDK display.\n";
    return;
  }

  GdkScreen *screen = gdk_display_get_default_screen(display);
  if (!GDK_IS_SCREEN(screen)) {
    std::cerr << "Invalid GDK screen.\n";
    return;
  }

  gtk_style_context_add_provider_for_screen(
      screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

  // Build full path to ~/.lightcontroller/css/cust_but.css
  std::string css_path =
      std::string(HOME_DIR) + "/.lightcontroller/css/cust_but.css";

  GError *error = NULL;
  gtk_css_provider_load_from_path(provider, css_path.c_str(), &error);

  if (error) {
    g_warning("Failed to load CSS: %s", error->message);
    g_clear_error(&error);
  }

  g_object_unref(provider);
}

int main(int argc, char *argv[]) {

  load_css();
  auto app =
      Gtk::Application::create(argc, argv, "com.sfdevelopment.lightcontroller");

  MainWindow lightController;
  // Shows the window and returns when it is closed.
  return app->run(lightController);
}
