#include <iostream>
#include <gtkmm.h>
#include <glibmm.h>
#include "popover-pager.hpp"

void WayfirePopoverPager::init(Gtk::Box *container)
{
    grid.add_css_class("workspace-switcher");
    grid.add_css_class("flat");

    ipc_client->subscribe(this, {"view-mapped"});
    ipc_client->subscribe(this, {"view-unmapped"});
    ipc_client->subscribe(this, {"view-set-output"});
    ipc_client->subscribe(this, {"view-geometry-changed"});
    ipc_client->subscribe(this, {"output-layout-changed"});
    ipc_client->subscribe(this, {"wset-workspace-changed"});
    workspace_switcher_target_height.set_callback([=] ()
    {
        get_wsets();
    });
    get_wsets();

    grid.add_css_class("workspace-switcher");
    container->append(grid);
}

void WayfirePopoverPager::get_wsets()
{
    ipc_client->send("{\"method\":\"window-rules/list-wsets\"}", [=] (wf::json_t data)
    {
        if (data.serialize().find("error") != std::string::npos)
        {
            std::cerr << data.serialize() << std::endl;
            std::cerr << "Error getting wsets list for workspace-switcher widget!" << std::endl;
            return;
        }

        process_workspaces(data);
    });
}

void WayfirePopoverPager::clear_box()
{
    for (auto child : grid.get_children())
    {
        grid.remove(*child);
    }
}

void WayfireWorkspaceBox::on_popover_grid_clicked(int count, double x, double y)
{
    wf::json_t workspace_switch_request;
    workspace_switch_request["method"] = "vswitch/set-workspace";
    wf::json_t workspace;
    workspace["x"] = this->switcher->current_ws_x = this->x_index;
    workspace["y"] = this->switcher->current_ws_y = this->y_index;
    workspace["output-id"] = this->output_id;
    workspace_switch_request["data"] = workspace;
    this->switcher->ipc_client->send(workspace_switch_request.serialize(), [=] (wf::json_t data)
    {
        if (data.serialize().find("error") != std::string::npos)
        {
            std::cerr << data.serialize() << std::endl;
            std::cerr << "Error switching workspaces. Is vswitch plugin enabled?" << std::endl;
        }
    });
    for (auto widget : this->switcher->grid.get_children())
    {
        WayfireWorkspaceBox *ws = (WayfireWorkspaceBox*)widget;
        if ((ws->x_index == this->x_index) && (ws->y_index == this->y_index))
        {
            ws->remove_css_class("inactive");
            ws->add_css_class("active");
        } else
        {
            ws->add_css_class("inactive");
            ws->remove_css_class("active");
        }
    }
}

void WayfirePopoverPager::on_grid_clicked(int count, double x, double y)
{
    this->popover->popup();
}

double WayfireWorkspaceBox::get_scaled_width()
{
    return this->switcher->workspace_switcher_target_height *
           (this->output_width / float(this->output_height));
}

void WayfirePopoverPager::process_workspaces(wf::json_t workspace_data)
{
    size_t i = 0;

    this->grid_width  = workspace_data[i]["workspace"]["grid_width"].as_int();
    this->grid_height = workspace_data[i]["workspace"]["grid_height"].as_int();

    for (i = 0; i < workspace_data.size(); i++)
    {
        wf::json_t output_info_request;
        output_info_request["method"] = "window-rules/output-info";
        wf::json_t output_id;
        output_id["id"] = workspace_data[i]["output-id"].as_int();
        output_info_request["data"] = output_id;
        ipc_client->send(output_info_request.serialize(), [=] (wf::json_t output_data)
        {
            if (output_data.serialize().find("error") != std::string::npos)
            {
                std::cerr << output_data.serialize() << std::endl;
                std::cerr << "Error getting output information!" << std::endl;
                return;
            }

            auto output_id     = output_data["id"].as_int();
            auto output_width  = output_data["geometry"]["width"].as_int();
            auto output_height = output_data["geometry"]["height"].as_int();
            if (this->output_name == output_data["name"].as_string())
            {
                clear_box();
                popover = Gtk::make_managed<Gtk::Popover>();
                popover->set_parent(grid);
                popover->set_child(popover_grid);
                auto click_gesture = Gtk::GestureClick::create();
                click_gesture->set_button(0);
                click_gesture->signal_released().connect(sigc::mem_fun(*this, &WayfirePopoverPager::on_grid_clicked));
                grid.add_controller(click_gesture);
                for (int j = 0; j < this->grid_height; j++)
                {
                    for (int k = 0; k < this->grid_width; k++)
                    {
                        auto ws = Gtk::make_managed<WayfireWorkspaceBox>(this);
                        ws->set_can_target(false);
                        ws->output_width = output_width;
                        ws->output_height = output_height;
                        auto ws_width = ws->get_scaled_width() / this->grid_width;
                        auto ws_height = this->workspace_switcher_target_height / this->grid_height;
                        ws->set_size_request(ws_width, ws_height);
                        ws->add_css_class("workspace");
                        if (workspace_data[i]["workspace"]["x"].as_int() == k &&
                            workspace_data[i]["workspace"]["y"].as_int() == j)
                        {
                            ws->add_css_class("active");
                            this->current_ws_x = k;
                            this->current_ws_y = j;
                        } else
                        {
                            ws->add_css_class("inactive");
                        }
                        ws->x_index = k;
                        ws->y_index = j;
                        grid.attach(*ws, ws->x_index, ws->y_index, 1, 1);

                        ws = Gtk::make_managed<WayfireWorkspaceBox>(this);
                        ws->output_width = output_width;
                        ws->output_height = output_height;
                        ws->set_size_request(ws->get_scaled_width(), this->workspace_switcher_target_height);
                        ws->add_css_class("workspace");
                        if (workspace_data[i]["workspace"]["x"].as_int() == k &&
                            workspace_data[i]["workspace"]["y"].as_int() == j)
                        {
                            ws->add_css_class("active");
                            this->current_ws_x = k;
                            this->current_ws_y = j;
                        } else
                        {
                            ws->add_css_class("inactive");
                        }
                        ws->x_index = k;
                        ws->y_index = j;
                        ws->output_id = output_id;
                        auto popover_click_gesture = Gtk::GestureClick::create();
                        popover_click_gesture->set_button(0);
                        popover_click_gesture->signal_released().connect(sigc::mem_fun(*ws, &WayfireWorkspaceBox::on_popover_grid_clicked));
                        ws->add_controller(popover_click_gesture);
                        popover_grid.attach(*ws, ws->x_index, ws->y_index, 1, 1);
                    }
                }
            }
        });
    }
}

void WayfirePopoverPager::on_event(wf::json_t data)
{
    if ((data["event"].as_string() == "output-layout-changed") ||
        (data["event"].as_string() == "wset-workspace-changed"))
    {
        get_wsets();
    }
}

WayfirePopoverPager::WayfirePopoverPager(WayfireOutput *output)
{
    this->output_name = output->monitor->get_connector();
    ipc_client = WayfireIPC::get_instance()->create_client();
    grid.set_vexpand(false);
    grid.set_valign(Gtk::Align::CENTER);
}

WayfirePopoverPager::~WayfirePopoverPager()
{
    ipc_client->unsubscribe(this);
    clear_box();
}
