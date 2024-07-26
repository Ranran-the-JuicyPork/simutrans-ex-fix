/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_SIGNALBOXLIST_FRAME_H
#define GUI_SIGNALBOXLIST_FRAME_H


#include "gui_frame.h"
#include "../simsignalbox.h"
#include "components/gui_scrolled_list.h"
#include "components/gui_label.h"
#include "components/gui_image.h"
#include "components/sortable_table.h"
#include "components/sortable_table_header.h"

class signalbox_t;

class signalbox_radius_cell_t : public value_cell_t
{
public:
	signalbox_radius_cell_t(uint32 radius);

	void set_value(sint64 value) OVERRIDE;
};


class signalboxlist_row_t : public gui_sort_table_row_t
{
public:
	enum sort_mode_t {
		SB_NAME,
		SB_CONNECTED,
		SB_CAPACITY,
		SB_RANGE,
		SB_COORD,
		SB_REGION,
		SB_CITY,
		SB_MAINTENANCE,
		SB_BUILT_DATE,
		MAX_COLS
	};
	static int sort_mode;
	static bool sortreverse;

private:
	signalbox_t* sb;
	// update flag
	uint32 old_connected;

public:
	signalboxlist_row_t(signalbox_t* sb);
	char const* get_text() const OVERRIDE { return sb->get_name(); }
	void show_info() const { sb->show_info(); }
	bool infowin_event(event_t const* ev) OVERRIDE;
	void draw(scr_coord offset) OVERRIDE;
	static bool compare(const gui_component_t* aa, const gui_component_t* bb);
};


class signalboxlist_frame_t : public gui_frame_t, private action_listener_t
{
private:
	static bool filter_has_vacant_slot;
	button_t filter_vacant_slot;

	gui_sort_table_header_t table_header;
	gui_aligned_container_t cont_sortable;
	gui_scrolled_list_t scrolly;
	gui_scrollpane_t scroll_sortable;

	uint32 last_signalbox_count;

	void fill_list();

	player_t *player;

public:
	signalboxlist_frame_t(player_t *player);

	const char *get_help_filename() const OVERRIDE {return "signalboxlist.txt"; }

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	void draw(scr_coord pos, scr_size size) OVERRIDE;
};

#endif
