#pragma once

#include <gtkmm.h>

#include "../widget.hpp"
#include "wf-ipc.hpp"

class WayfireWorkspaceBox;
class WayfireWorkspaceWindow : public Gtk::Widget
{
  public:
    int x = 0, y = 0, w = 10, h = 10;
    int x_index, y_index;
    int id, output_id;
    WayfireWorkspaceBox *ws;
    WayfireWorkspaceWindow()
    {}
    ~WayfireWorkspaceWindow() override
    {}
};

class WayfirePopoverPager : public WayfireWidget, public IIPCSubscriber
{
    std::string output_name;
    void on_event(wf::json_t data) override;
    void process_workspaces(wf::json_t workspace_data);
    void render_views(wf::json_t views_data);
    void add_view(wf::json_t view_data);
    void remove_view(wf::json_t view_data);
    void clear_box();
    void get_wsets();
    bool on_get_child_position(Gtk::Widget *widget, Gdk::Rectangle& allocation);

  public:
    Gtk::Grid grid;
    Gtk::Popover *popover;
    Gtk::Grid popover_grid;
    Gtk::Overlay overlay;
    double get_scaled_width();
    int output_width, output_height;
    void init(Gtk::Box *container) override;
    WayfirePopoverPager(WayfireOutput *output);
    ~WayfirePopoverPager();
    int grid_width, grid_height;
    std::shared_ptr<IPCClient> ipc_client;
    std::pair<int, int> get_workspace(WayfireWorkspaceWindow *w);
    int current_ws_x, current_ws_y;
    void on_grid_clicked(int count, double x, double y);
    std::vector<WayfireWorkspaceWindow*> windows;
    WfOption<double> workspace_switcher_target_height{"panel/workspace_switcher_target_height"};
};

class WayfireWorkspaceBox : public Gtk::Overlay
{
    WayfirePopoverPager *switcher;

  public:
    int x_index, y_index;
    int output_id;
    WayfireWorkspaceBox(WayfirePopoverPager *switcher)
    {
        this->switcher = switcher;
    }

    ~WayfireWorkspaceBox() override
    {}
    void on_popover_grid_clicked(int count, double x, double y);
};
