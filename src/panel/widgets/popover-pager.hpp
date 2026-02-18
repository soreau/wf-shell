#pragma once

#include <gtkmm.h>

#include "../widget.hpp"
#include "wf-ipc.hpp"

class WayfirePopoverPager : public WayfireWidget, public IIPCSubscriber
{
    std::string output_name;
    void on_event(wf::json_t data) override;
    void process_workspaces(wf::json_t workspace_data);
    void clear_box();
    void get_wsets();

  public:
    Gtk::Grid grid;
    Gtk::Popover *popover;
    Gtk::Grid popover_grid;
    void init(Gtk::Box *container) override;
    WayfirePopoverPager(WayfireOutput *output);
    ~WayfirePopoverPager();
    int grid_width, grid_height;
    std::shared_ptr<IPCClient> ipc_client;
    int current_ws_x, current_ws_y;
    void on_grid_clicked(int count, double x, double y);
    WfOption<double> workspace_switcher_target_height{"panel/workspace_switcher_target_height"};
};

class WayfireWorkspaceBox : public Gtk::Overlay
{
    WayfirePopoverPager *switcher;

  public:
    int x_index, y_index;
    int output_id, output_width, output_height;
    double get_scaled_width();
    WayfireWorkspaceBox(WayfirePopoverPager *switcher)
    {
        this->switcher = switcher;
    }

    ~WayfireWorkspaceBox() override
    {}
    void on_popover_grid_clicked(int count, double x, double y);
};
