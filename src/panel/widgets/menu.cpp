#include <sys/stat.h>
#include <wordexp.h>
#include <dirent.h>
#include <glibmm.h>
#include <giomm/icon.h>
#include <glibmm/spawn.h>
#include "menu.hpp"
#include "config.hpp"
#include "gtk-utils.hpp"

static void
on_item_click(GtkWidget *widget, gpointer data)
{
    std::string command = std::string((char *) data);
    Glib::spawn_command_line_async("/bin/bash -c \'" + command + "\'");
}

void WayfireMenu::load_menu_item(std::string file)
{
    GKeyFile *key_file = g_key_file_new();
    gchar *name, *icon, *exec;

    g_key_file_load_from_file(key_file, (const gchar *) file.c_str(), G_KEY_FILE_NONE, NULL);

    name = g_key_file_get_string(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME, NULL);
    icon = g_key_file_get_string(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
    exec = g_key_file_get_string(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);

    if (name && icon && exec)
    {
        for (uint i = 0; i < items.size(); i++)
        {
            if (!strcmp(name, items[i]->name) && !strcmp(exec, items[i]->exec))
            {
                free(exec);
                printf("Skipping menu item %s\n", name);
                goto skip;
            }
        }

        Gtk::VBox *button_box = new Gtk::VBox();
        Gtk::Button *button = new Gtk::Button();
        Gtk::Image *image = new Gtk::Image();
        Gtk::Label *label = new Gtk::Label();
        MenuItem *item = new MenuItem();

        item->button_box = button_box;
        item->button = button;
        item->image = image;
        item->label = label;
        item->name = strdup(name);
        item->exec = exec;
        items.push_back(item);

        auto gicon = Gio::Icon::create(icon);
        gtk_image_set_from_gicon(image->gobj(), gicon->gobj(),
            GTK_ICON_SIZE_LARGE_TOOLBAR);
        image->set_pixel_size(48);
        button->set_tooltip_text(name);
        if (strlen(name) > 12)
            strcpy(&name[10], "..");
        label->set_text(name);
        button_box->add(*image);
        button_box->add(*label);
        button->add(*button_box);
        button->set_size_request(100, 0);
        button->get_style_context()->add_class("flat");
        g_signal_connect(button->gobj(), "clicked", G_CALLBACK(on_item_click), exec);
        flowbox.add(*button);
    }
    else
    {
        printf("Skipping menu item: %s\n", name);
        free(exec);
    }
skip:
    free(name);
    free(icon);
}

void WayfireMenu::load_menu_items(std::string path)
{
    DIR *dir;
    struct dirent *file;
    wordexp_t exp;
    struct stat next;
    char fullpath[256];

    wordexp(path.c_str(), &exp, 0);

    if ((dir = opendir(exp.we_wordv[0])) == NULL)
        return;

    while ((file = readdir(dir)) != 0)
    {
        if (!strcmp(file->d_name, ".") || !strcmp(file->d_name, ".."))
            continue;

        strcpy(fullpath, exp.we_wordv[0]);
        strcat(fullpath, "/");
        strcat(fullpath, file->d_name);

        if (stat(fullpath, &next) == 0)
        {
            if (S_ISDIR(next.st_mode))
                load_menu_items(fullpath);
            else
                load_menu_item(std::string(fullpath));
        }
    }

    wordfree(&exp);
}

static void
remove_item(GtkWidget *widget, gpointer data)
{
    Gtk::FlowBox *flowbox = (Gtk::FlowBox *) data;
    gtk_container_remove(GTK_CONTAINER(flowbox->gobj()), widget);
}

static gboolean
on_search_keystroke(GtkWidget *widget, gpointer data)
{
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(widget));
    WayfireMenu *menu = (WayfireMenu *) data;

    gtk_container_foreach(GTK_CONTAINER(menu->flowbox.gobj()), remove_item, &menu->flowbox);
    if (strcmp(text, ""))
    {
        for (uint i = 0; i < menu->items.size(); i++)
        {
            if (!strncasecmp(menu->items[i]->name, text, strlen(text)))
                menu->flowbox.add(*menu->items[i]->button);
        }
    }
    else
    {
        for (uint i = 0; i < menu->items.size(); i++)
            menu->flowbox.add(*menu->items[i]->button);
    }

    return false;
}

static void
on_popover_shown(GtkWidget *widget, gpointer data)
{
    Gtk::FlowBox *flowbox = (Gtk::FlowBox *) data;

    flowbox->unselect_all();
}

void WayfireMenu::init(Gtk::HBox *container, wayfire_config *config)
{
    int32_t panel_size = *config->get_section("panel")->get_option("panel_thickness", "48");
    int32_t default_size = panel_size * 0.7;
    int32_t base_size = *config->get_section("panel")->get_option("launcher_size", std::to_string(default_size));
    base_size = std::min(base_size, panel_size);
    popover = Gtk::Popover(menu_button);
    menu_button.add(main_image);
    menu_button.set_direction(Gtk::ARROW_DOWN);
    menu_button.set_popover(popover);
    menu_button.get_style_context()->add_class("flat");
    menu_button.set_size_request(48, 0);

    popover.set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);
    g_signal_connect(popover.gobj(), "show", G_CALLBACK(on_popover_shown), &flowbox);

    auto ptr_pbuff = Gdk::Pixbuf::create_from_file(ICONDIR "/wayfire.png", base_size * main_image.get_scale_factor(), base_size * main_image.get_scale_factor());
    if (!ptr_pbuff)
        return;

    set_image_pixbuf(main_image, ptr_pbuff, main_image.get_scale_factor());

    container->pack_start(hbox, Gtk::PACK_SHRINK, 0);
    hbox.pack_start(menu_button, Gtk::PACK_SHRINK, 0);

    load_menu_items("~/.local/share/applications");
    load_menu_items("/usr/share/applications");

    flowbox.set_valign(Gtk::ALIGN_START);
    flowbox.set_homogeneous(true);

    flowbox_container.add(flowbox);
    flowbox_container.add(bottom_pad);

    scrolled_window.add(flowbox_container);
    scrolled_window.set_min_content_width(500);
    scrolled_window.set_min_content_height(500);

    g_object_set(search_box.gobj(), "margin", 20, NULL);
    gtk_entry_set_icon_from_icon_name(search_box.gobj(), GTK_ENTRY_ICON_SECONDARY, "search");
    g_signal_connect(search_box.gobj(), "changed", G_CALLBACK(on_search_keystroke), this);

    box.add(search_box);
    box.add(scrolled_window);
    box.show_all();

    popover.add(box);
}

void WayfireMenu::focus_lost()
{
    menu_button.set_active(false);
}

WayfireMenu::~WayfireMenu()
{
}
