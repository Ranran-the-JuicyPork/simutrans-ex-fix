/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_CHAT_FRAME_H
#define GUI_CHAT_FRAME_H


#include "simwin.h"

#include "gui_frame.h"
#include "components/gui_button.h"
#include "components/gui_combobox.h"
#include "components/gui_flowtext.h"
#include "components/gui_label.h"
#include "components/gui_scrollpane.h"
#include "components/gui_tab_panel.h"
#include "components/gui_textinput.h"

#include "components/action_listener.h"

#include "../player/simplay.h"
#include "../simmesg.h"
#include "../simworld.h"

#define MAX_CHAT_TABS (3)


class gui_chat_flowtext_t : public gui_flowtext_t
{
	scr_coord_val min_width;
	scr_coord_val min_height;
	scr_coord_val max_height;

public:
	gui_chat_flowtext_t(const char* text);
	void draw(scr_coord offset) OVERRIDE;

	// Add 2px paddings to upper and bottom
	scr_size get_size() const OVERRIDE { return gui_flowtext_t::get_size()+scr_size(0,4); }

	scr_size get_min_size() const OVERRIDE { return scr_size(min_width, min_height); }
	scr_size get_max_size() const OVERRIDE { return scr_size(scr_size::inf.w, max_height); }
};


class gui_chat_baloon_t : public gui_aligned_container_t, public gui_action_creator_t
{

public:
	enum baloon_tale_t {
		left  = 0, // for others post
		right = 1, // for own post
		none  = 2  // for system message
	};

private:
	PIXVAL bgcol;
	sint64 date;
	baloon_tale_t tale_dir;
	const char* text;

	gui_aligned_container_t cont_body;
	gui_chat_flowtext_t message;

	int old_min;
	gui_label_buf_t lb_time_diff;
	void update_time_diff(time_t now);

public:
	gui_chat_baloon_t(const char* text_, sint64 date_, sint8 player_nr = 1, baloon_tale_t tale_dir_ = left);

	void draw(scr_coord offset) OVERRIDE;

	bool infowin_event(event_t const*) OVERRIDE;
};

 /**
  * One comment component roughly consists of three blocks. (name/time/balloon)
  * The size of the message balloon is variable,
  * but the size, especially the height, is determined after initialization,
  * so it is necessary to monitor it.
  */
class chat_stats_t : public gui_aligned_container_t, public action_listener_t
{
private:
	const chat_message_t::chat_node* msg;

	button_t bt_whisper_to, bt_pos;
	gui_aligned_container_t cont_time;
	gui_chat_baloon_t baloon;
	scr_coord_val preferred_height;

public:
	chat_stats_t(const chat_message_t::chat_node* m, bool continuous);

	const char* get_target() const { return msg->sender; }

	void draw(scr_coord offset) OVERRIDE;

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	scr_size get_max_size() const OVERRIDE { return scr_size(scr_size::inf.w, preferred_height); }
};

/**
 * Chat window
 */
class chat_frame_t : public gui_frame_t, private action_listener_t
{
private:
	char ibuf[256];
	char ibuf_name[256];
	gui_aligned_container_t cont_chat_log[MAX_CHAT_TABS];
	gui_aligned_container_t cont_tab_whisper;

	gui_scrollpane_t
		scrolly_public,
		scrolly_company,
		scrolly_whisper;
	gui_tab_panel_t tabs;
	gui_textinput_t
		input,
		inp_destination;
	gui_label_buf_t
		lb_now_online,
		lb_whisper_target,
		lb_channel;
	button_t opaque_bt,
		bt_send_pos;

	uint32 last_count=0; // of messages in list
	sint8 old_player_nr=0;

	vector_tpl<plainstring> chat_history;
	gui_combobox_t cb_direct_chat_targets;

	void fill_list();

public:
	chat_frame_t();

	/**
	 * Set the window associated helptext
	 * @return the filename for the helptext, or NULL
	 */
	const char * get_help_filename() const OVERRIDE {return "chat.txt";}

	void activate_whisper_to(const char* recipient);

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	void set_dirty() { resize(scr_size(0, 0)); }

	void rdwr(loadsave_t *) OVERRIDE;

	uint32 get_rdwr_id() OVERRIDE { return magic_chatframe; }

	void draw(scr_coord pos, scr_size size) OVERRIDE;
};

#endif
