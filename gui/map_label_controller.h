/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_MAP_LABEL_CONTROLLER_H
#define GUI_MAP_LABEL_CONTROLLER_H


#include "../dataobj/environment.h"
#include "components/gui_button.h"
#include "components/gui_combobox.h"
#include "gui_frame.h"

#define MAX_WAYTYPES 8

/*
 * Class to generates the welcome screen with the scrolling
 * text to celebrate contributors.
 */
class map_label_controller_t : public gui_frame_t, action_listener_t
{
private:
	button_t
		bt_station_name_enable, bt_filter_station_cargo_type,
		bt_station_all_waytype,
		bt_station_filter_waytype[MAX_WAYTYPES],
		bt_station_filter_freight_type[3]; // pax,mail,freight
	gui_combobox_t cb_station_name_filter_player;

public:
	map_label_controller_t();

	bool has_sticky() const OVERRIDE { return false; }

	bool has_title() const OVERRIDE { return false; }

	/**
	* Window Title
	*/
	const char *get_name() const {return ""; }

	/**
	* get color information for the window title
	* -borders and -body background
	*/
	//FLAGGED_PIXVAL get_titlecolor() const OVERRIDE {return env_t::default_window_title_color; }

	void draw(scr_coord pos, scr_size size) OVERRIDE;

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;
};

#endif
