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
    auto click_gesture = Gtk::GestureClick::create();
    click_gesture->set_button(0);
    click_gesture->signal_released().connect(sigc::mem_fun(*this, &WayfirePopoverPager::on_grid_clicked));
    grid.add_controller(click_gesture);
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
    for (auto child : popover_grid.get_children())
    {
        popover_grid.remove(*child);
    }
}

std::pair<int, int> WayfirePopoverPager::get_workspace(WayfireWorkspaceWindow *w)
{
    std::pair<int, int> workspace;
    double scaled_output_width  = this->get_scaled_width();
    double scaled_output_height = workspace_switcher_target_height;
    workspace.first  = std::floor((w->x + (w->w / 2)) / scaled_output_width) + this->current_ws_x;
    workspace.second = std::floor((w->y + (w->h / 2)) / scaled_output_height) + this->current_ws_y;
    return workspace;
}

bool WayfirePopoverPager::on_get_child_position(Gtk::Widget *widget, Gdk::Rectangle& allocation)
{
    if (auto w = static_cast<WayfireWorkspaceWindow*>(widget))
    {
        allocation.set_x(w->x + this->current_ws_x * this->get_scaled_width());
        allocation.set_y(w->y + this->current_ws_y * this->workspace_switcher_target_height);
        allocation.set_width(w->w);
        allocation.set_height(w->h);
        return true;
    }

    return false;
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

double WayfirePopoverPager::get_scaled_width()
{
    return this->workspace_switcher_target_height *
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

            this->output_width  = output_data["geometry"]["width"].as_int();
            this->output_height = output_data["geometry"]["height"].as_int();
            if (this->output_name == output_data["name"].as_string())
            {
                clear_box();
                popover = Gtk::make_managed<Gtk::Popover>();
                popover->set_parent(grid);
                popover->set_child(overlay);
                overlay.set_child(popover_grid);
                overlay.signal_get_child_position().connect(sigc::mem_fun(*this, &WayfirePopoverPager::on_get_child_position), false);
                for (int j = 0; j < this->grid_height; j++)
                {
                    for (int k = 0; k < this->grid_width; k++)
                    {
                        auto ws = Gtk::make_managed<WayfireWorkspaceBox>(this);
                        ws->output_id = output_data["id"].as_int();
                        ws->set_can_target(false);
                        auto ws_width = this->get_scaled_width() / this->grid_width;
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
                        ws->output_id = output_data["id"].as_int();
                        ws->set_size_request(this->get_scaled_width(), this->workspace_switcher_target_height);
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
                        auto popover_click_gesture = Gtk::GestureClick::create();
                        popover_click_gesture->set_button(0);
                        popover_click_gesture->signal_released().connect(sigc::mem_fun(*ws, &WayfireWorkspaceBox::on_popover_grid_clicked));
                        ws->add_controller(popover_click_gesture);
                        popover_grid.attach(*ws, ws->x_index, ws->y_index, 1, 1);
                        if (j == this->grid_height - 1 && k == this->grid_width - 1)
                        {
                            ipc_client->send("{\"method\":\"window-rules/list-views\"}", [=] (wf::json_t data)
                            {
                                if (data.serialize().find("error") != std::string::npos)
                                {
                                    std::cerr << data.serialize() << std::endl;
                                    std::cerr << "Error getting views list for workspace-switcher widget!" << std::endl;
                                    return;
                                }
                                render_views(data);
                            });
                        }
                    }
                }
            }
        });
    }
}

void WayfirePopoverPager::add_view(wf::json_t view_data)
{
    if (view_data["type"].as_string() != "toplevel")
    {
        return;
    }

    if (view_data["output-name"].as_string() != this->output_name)
    {
        return;
    }

    auto v = Gtk::make_managed<WayfireWorkspaceWindow>();
    v->add_css_class("view");
    v->add_css_class(view_data["app-id"].as_string());
    v->id = view_data["id"].as_int();
    v->output_id = view_data["output-id"].as_int();
    auto x = view_data["geometry"]["x"].as_int();
    auto y = view_data["geometry"]["y"].as_int();
    auto w = view_data["geometry"]["width"].as_int();
    auto h = view_data["geometry"]["height"].as_int();
    for (auto widget : overlay.get_children())
    {
        WayfireWorkspaceWindow *window = (WayfireWorkspaceWindow*)widget;
        if (window->id == v->id)
        {
            return;
        }

        double width  = this->get_scaled_width();
        double height = workspace_switcher_target_height;

        //v->ws = ws;
        v->x  = x * (width / float(this->output_width));
        v->y  = y * (height / float(this->output_height));
        v->w  = (w / float(this->output_width)) * width;
        v->h  = (h / float(this->output_height)) * height;
        std::pair<int, int> workspace;
        workspace  = get_workspace(v);
        v->x_index = workspace.first;
        v->y_index = workspace.second;
        v->set_can_target(false);
        // add to workspace box
        overlay.add_overlay(*v);
        windows.push_back(v);
        return;
    }
}

void WayfirePopoverPager::remove_view(wf::json_t view_data)
{
    for (auto w : this->windows)
    {
        if (w->id == view_data["id"].as_int())
        {
            for (auto widget : overlay.get_children())
            {
                WayfireWorkspaceWindow *w = (WayfireWorkspaceWindow*)widget;
                overlay.remove_overlay(*w);
                auto elem = std::remove(windows.begin(), windows.end(), w);
                windows.erase(elem, windows.end());
                return;
            }
        }
    }
}

void WayfirePopoverPager::render_views(wf::json_t views_data)
{
    for (size_t i = 0; i < views_data.size(); i++)
    {
        add_view(views_data[i]);
    }
}

void WayfirePopoverPager::on_event(wf::json_t data)
{
    if (data["event"].as_string() == "view-geometry-changed")
    {
        for (auto widget : overlay.get_children())
        {
            WayfireWorkspaceWindow *w = (WayfireWorkspaceWindow*)widget;
            if (w->id == data["view"]["id"].as_int())
            {
                overlay.remove_overlay(*w);
                auto elem = std::remove(windows.begin(), windows.end(), w);
                windows.erase(elem, windows.end());
            }
        }

        add_view(data["view"]);
    } else if (data["event"].as_string() == "view-mapped")
    {
        add_view(data["view"]);
    } else if (data["event"].as_string() == "view-unmapped")
    {
        remove_view(data["view"]);
    } else if (data["event"].as_string() == "view-set-output")
    {
        add_view(data["view"]);
    } else if ((data["event"].as_string() == "output-layout-changed") ||
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
