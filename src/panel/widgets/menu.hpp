#ifndef WIDGETS_MENU_HPP
#define WIDGETS_MENU_HPP

#include "../widget.hpp"
#include <gtkmm/entry.h>
#include <gtkmm/image.h>
#include <gtkmm/popover.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/scrolledwindow.h>

class MenuItem
{
    public:
    Gtk::Button *button;
    Gtk::Image *image;
    Gtk::Label *label;
    Gtk::VBox *vbox;
    gchar *name, *exec;
};

class WayfireMenu : public WayfireWidget
{
    Gtk::Popover popover;
    Gtk::MenuButton menu_button;
    Gtk::Image main_image;
    Gtk::VBox box;
    Gtk::HBox hbox;
    Gtk::Entry search_box;
    Gtk::ScrolledWindow scrolled_window;

    void load_menu_item(std::string file);
    void load_menu_items(std::string path);
    void focus_lost(void);

    public:
    void init(Gtk::HBox *container, wayfire_config *config) override;
    std::vector<MenuItem *> items;
    Gtk::FlowBox flowbox;
    ~WayfireMenu();
};

#endif /* end of include guard: WIDGETS_MENU_HPP */
