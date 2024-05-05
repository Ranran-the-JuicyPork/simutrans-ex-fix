/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <time.h>

#include "simwin.h"
#include "chat_frame.h"

#include "../simmenu.h"
#include "../sys/simsys.h"

#include "../dataobj/translator.h"
#include "../dataobj/environment.h"
#include "../dataobj/gameinfo.h"
#include "../network/network_cmd_ingame.h"
#include "../player/simplay.h"
#include "../display/viewport.h"
#include "../gui/messagebox.h"

#include "components/gui_colorbox.h"

#define CH_PUBLIC  (0)
#define CH_COMPANY (1)
#define CH_WHISPER (2)

static char const* const tab_strings[MAX_CHAT_TABS]=
{
	"public_chat",
	"company_chat",
	"personal_chat"
};


class gui_baloon_tale_t : public gui_colorbox_t
{
	bool right_aligned;

public:
	gui_baloon_tale_t(PIXVAL c, bool right_aligned_=false, scr_coord_val width_ = LINEASCENT*2/3) : gui_colorbox_t(c)
	{
		size_fixed = true;
		show_frame = false;
		width = width_;
		height = width_+1;
		right_aligned = right_aligned_;
		set_size(scr_size(width, height));
	}

	void draw(scr_coord offset) OVERRIDE;

	scr_size get_max_size() const OVERRIDE { return get_min_size(); }
};

void gui_baloon_tale_t::draw(scr_coord offset)
{
	offset += pos;
	for (scr_coord_val x = 0; x < width; x++) {
		if (right_aligned) {
			display_vline_wh_clip_rgb(offset.x + x, offset.y + x, height-x, color, true);
		}
		else {
			display_vline_wh_clip_rgb(offset.x + x, offset.y, x + 1, color, true);
		}
	}
	if (right_aligned) {
		display_fillbox_wh_clip_rgb(offset.x, offset.y+height-1, width, 1, display_blend_colors(color, SYSCOL_SHADOW, 75), true);
	}
}


gui_chat_flowtext_t::gui_chat_flowtext_t(const char* text)
{
	set_text(text);
	set_maximize(true);
	set_show_scroll_x(false);
	set_show_scroll_y(false);
	min_width = get_preferred_size().w + D_H_SPACE * 2;
	min_height = get_preferred_size().h+4;
	max_height = get_preferred_size().h+4;
	set_size(get_size());
}


void gui_chat_flowtext_t::draw(scr_coord offset)
{
	min_height = get_preferred_size().h+4;
	max_height = get_preferred_size().h+4;
	offset.y += 2;
	gui_flowtext_t::draw(offset);
}



gui_chat_baloon_t::gui_chat_baloon_t(const char* text_, sint64 date_, sint8 player_nr, baloon_tale_t tale_dir_) : message(text_)
{
	text = text_[0]==0 ? translator::translate("(deleted)") : text_;
	date = date_;
	tale_dir = tale_dir_;
	old_min = 0;
	const bool is_dark_theme = (env_t::gui_player_color_dark>=env_t::gui_player_color_bright);
	const int base_brend_percent = tale_dir==right ? 60 : 80;
	player_t* player = world()->get_player(player_nr);
	const PIXVAL base_color = color_idx_to_rgb(player ? player->get_player_color1() + env_t::gui_player_color_bright : COL_GREY4);
	bgcol = display_blend_colors(
				base_color,
				color_idx_to_rgb(COL_WHITE),
				is_dark_theme ? (95-base_brend_percent) : base_brend_percent);
	set_focusable(false);
	set_table_layout(2, 2);
	set_alignment(ALIGN_TOP);
	set_spacing(NO_SPACING);
	cont_body.set_table_layout(2,0);
	cont_body.set_spacing(NO_SPACING);
	if( tale_dir==none ) {
		cont_body.new_component_span<gui_label_t>(text, SYSCOL_TEXT_HIGHLIGHT, gui_label_t::centered, 2)->set_padding(scr_size(D_MARGIN_LEFT,0));
	}
	else {
		cont_body.add_component(&message,2);
	}

	if( date ) {
		cont_body.new_component<gui_fill_t>();
		cont_body.add_component(&lb_time_diff);
	}

	if( tale_dir==left ) {
		// left
		add_table(1,3)->set_spacing(NO_SPACING);
		{
			new_component<gui_margin_t>(LINEASCENT/2, LINEASCENT/2);
			new_component<gui_baloon_tale_t>(bgcol);
			new_component<gui_fill_t>(false, true);
		}
		end_table();
	}
	else if( tale_dir==none ) {
		new_component<gui_empty_t>();
	}

	add_component(&cont_body);

	if (tale_dir==right) {
		// right
		add_table(1,3)->set_spacing(NO_SPACING);
		{
			new_component<gui_fill_t>(false, true);
			new_component<gui_baloon_tale_t>(bgcol, true);
			new_component<gui_margin_t>(LINEASCENT/2, LINEASCENT/2);
		}
		end_table();
	}

	new_component_span<gui_fill_t>(false,true,2);
	if( date ){ // old save messages does not have date
		update_time_diff(time(NULL));
	}
	else {
		cont_body.set_size(cont_body.get_min_size());
		set_size(get_min_size());
	}
}

void gui_chat_baloon_t::update_time_diff(time_t now)
{
	lb_time_diff.buf().append(" (");
	lb_time_diff.set_color(SYSCOL_TEXT_WEAK);
	int diff = difftime(now, date);
	if (diff > 31536000) {
		uint8 years = (uint8)(diff / 31536000);
		if (years == 1) {
			lb_time_diff.buf().append(translator::translate("1 year ago"));
		}
		else {
			lb_time_diff.buf().printf(translator::translate("%u years ago"), years);
		}
	}
	else if (diff > 172800) {
		// 2 days to 365 days
		uint16 days = (uint16)(diff / 86400);
		lb_time_diff.buf().printf(translator::translate("%u days ago"), days);
	}
	else if (diff > 7200) {
		// 2 hours to 48 hours
		uint8 hours = (uint8)(diff / 3600);
		lb_time_diff.buf().printf(translator::translate("%u hours ago"), hours);
	}
	else if (diff > 60) {
		uint8 minutes = (uint8)(diff / 60);
		if (minutes == 1) {
			lb_time_diff.buf().append(translator::translate(" 1 minute ago"));
		}
		else {
			lb_time_diff.buf().printf(translator::translate("%u minutes ago"), minutes);
		}
		lb_time_diff.set_color(SYSCOL_TEXT);
	}
	else {
		lb_time_diff.buf().append(translator::translate("just now"));
		lb_time_diff.set_color(SYSCOL_TEXT);
	}
	lb_time_diff.buf().append(") ");
	lb_time_diff.update();
	cont_body.set_size(cont_body.get_min_size());
	set_size(scr_size(get_size().w, get_min_size().h));
}

void gui_chat_baloon_t::draw(scr_coord offset)
{
	if (date) { // old save messages does not have date
		time_t now = time(NULL);
		tm* tm_event = localtime(&now);

		if (old_min != tm_event->tm_min) {
			old_min = tm_event->tm_min;
			update_time_diff(now);
		}
	}
	scr_coord bg_offset = offset + pos+cont_body.get_pos();

	display_filled_roundbox_clip(bg_offset.x+1, bg_offset.y+1, cont_body.get_size().w, cont_body.get_size().h, display_blend_colors(bgcol,SYSCOL_SHADOW,75), false);
	display_filled_roundbox_clip(bg_offset.x, bg_offset.y, cont_body.get_size().w, cont_body.get_size().h, bgcol, false);
	gui_aligned_container_t::draw(offset);
}


bool gui_chat_baloon_t::infowin_event(const event_t* ev)
{
	bool swallowed = gui_aligned_container_t::infowin_event(ev);
	if (!swallowed && IS_LEFTRELEASE(ev)) {
		cbuffer_t clipboard;
		// add them to clipboard
		char msg_no_break[258];
		for (int j = 0; j < 256; j++) {
			msg_no_break[j] = text[j] == '\n' ? ' ' : text[j];
			if (msg_no_break[j] == 0) {
				msg_no_break[j++] = '\n';
				msg_no_break[j] = 0;
				break;
			}
		}
		clipboard.append(msg_no_break);
		// copy, if there was anything ...
		if (clipboard.len() > 0) {
			dr_copy(clipboard, clipboard.len());
		}

		create_win(new news_img("Copied."), w_time_delete, magic_none);
		swallowed = true;
	}
	return false;
}


chat_stats_t::chat_stats_t(const chat_message_t::chat_node* m, bool continuous) :
	msg(m),
	baloon(m->msg, m->local_time, m->player_nr,
		msg->sender == "" ? gui_chat_baloon_t::none
			: (strcmp(msg->sender, env_t::nickname.c_str()) == 0) ? gui_chat_baloon_t::right : gui_chat_baloon_t::left)
{
	preferred_height = D_LABEL_HEIGHT + 4;
	gui_chat_baloon_t::baloon_tale_t tale_dir = msg->sender == "" ? gui_chat_baloon_t::none
		: (strcmp(msg->sender, env_t::nickname.c_str())==0) ? gui_chat_baloon_t::right : gui_chat_baloon_t::left;

	set_focusable(false);
	set_table_layout(1,0);
	bt_whisper_to.set_visible(false);
	// name <company>
	if (!continuous && tale_dir==gui_chat_baloon_t::left) {
		add_table(3, 1)->set_focusable(false);
		{
			bt_whisper_to.set_visible(true);
			bt_whisper_to.init(button_t::arrowright, NULL);
			bt_whisper_to.set_focusable(false);
			bt_whisper_to.add_listener(this);
			add_component(&bt_whisper_to);

			player_t* player = world()->get_player(msg->player_nr);
			PIXVAL text_color = SYSCOL_TEXT;
			if( player ) {
				text_color= color_idx_to_rgb(player->get_player_color1() + env_t::gui_player_color_dark);
			}
			gui_label_buf_t* lb = new_component<gui_label_buf_t>(text_color);
			lb->buf().append(msg->sender.c_str());
			if ( player ) {
				lb->buf().printf(" <%s>", player->get_name());
			}

			// If the background is transparent, it will be difficult to read without shadows.
			lb->set_shadow(SYSCOL_TEXT_SHADOW, true);
			lb->update();

			new_component<gui_fill_t>();
		}
		end_table();
	}

	// date/time
	cont_time.set_table_layout(2,2);
	cont_time.set_spacing(NO_SPACING);
	{
		if (msg->pos == koord::invalid) {
			cont_time.new_component<gui_empty_t>();
		}
		else {
			bt_pos.set_typ(button_t::posbutton_automatic);
			bt_pos.set_targetpos(msg->pos);
			cont_time.add_component(&bt_pos);
		}

		gui_label_buf_t* lb = cont_time.new_component<gui_label_buf_t>(SYSCOL_TEXT);
		lb->buf().printf("%s", translator::get_short_date( msg->time/12, msg->time%12 ));
		lb->update();


		// local time
		if (msg->local_time!=0) {
			char date[18];
			// add the time too
			struct tm* tm_event = localtime(&msg->local_time);
			if (tm_event) {
				strftime(date, 18, "%m-%d %H:%M", tm_event);
			}
			cont_time.new_component_span<gui_label_buf_t>(2)->buf().append(date);
		}
	}
	cont_time.set_size(cont_time.get_min_size());

	// message baloon
	add_table(3,1)->set_alignment(ALIGN_TOP);
	{
		// text
		if (tale_dir != gui_chat_baloon_t::right) {
			add_component(&baloon);
		}
		else {
			new_component<gui_margin_t>(D_MARGIN_LEFT);
		}

		add_component(&cont_time);

		if (tale_dir == gui_chat_baloon_t::right) {
			add_component(&baloon);
		}
		else {
			new_component<gui_margin_t>(D_MARGIN_RIGHT);
		}
	}
	end_table();

	set_size(gui_aligned_container_t::get_min_size());
}

void chat_stats_t::draw(scr_coord offset)
{
	scr_coord_val temp_heght = (D_LABEL_HEIGHT+D_V_SPACE)*bt_whisper_to.is_visible() + max(cont_time.get_min_size().h, baloon.get_min_size().h);
	if (temp_heght!=preferred_height) {
		preferred_height = temp_heght;
		set_size(scr_size(get_size().w, preferred_height));
		chat_frame_t* win = dynamic_cast<chat_frame_t*>(win_get_magic(magic_chatframe));
		if (win) {
			win->set_dirty();
		}
	}
	gui_aligned_container_t::draw(offset);
}


bool chat_stats_t::action_triggered(gui_action_creator_t* comp, value_t /* */)
{
	if ( comp == &bt_whisper_to ) {
		chat_frame_t* win = dynamic_cast<chat_frame_t*>(win_get_magic(magic_chatframe));
		if (!win) {
			// error
		}
		else {
			win->activate_whisper_to(msg->sender.c_str());
		}
	}
	return true;
}

chat_frame_t::chat_frame_t() :
	gui_frame_t( translator::translate("Chat") ),
	scrolly_public(&cont_chat_log[0]),
	scrolly_company(&cont_chat_log[1]),
	scrolly_whisper(&cont_chat_log[2])
{
	ibuf[0] = 0;
	ibuf_name[0] = 0;


	set_table_layout(1,0);
	set_focusable(false);
	cont_tab_whisper.set_focusable(false);
	add_table(3,1);
	{
		opaque_bt.set_focusable(false);
		opaque_bt.init(button_t::square_state, translator::translate("transparent background"));
		opaque_bt.add_listener(this);
		add_component(&opaque_bt);
		new_component<gui_fill_t>();
		lb_now_online.set_focusable(false);
		lb_now_online.set_align(gui_label_t::right);
		add_component(&lb_now_online);
	}
	end_table();

	for (uint8 i = 0;i < MAX_CHAT_TABS;i++) {
		cont_chat_log[i].set_table_layout(1, 0);
		cont_chat_log[i].set_focusable(false);
	}

	cont_tab_whisper.set_table_layout(1,0);
	cont_tab_whisper.add_table(2,1);
	{
		cont_tab_whisper.add_component(&lb_whisper_target);
		cb_direct_chat_targets.set_visible(false);
		cb_direct_chat_targets.add_listener(this);
		cont_tab_whisper.add_component(&cb_direct_chat_targets);
	}
	cont_tab_whisper.end_table();
	cont_tab_whisper.add_component(&scrolly_whisper);

	// add tabs for classifying messages
	tabs.add_tab(&scrolly_public, translator::translate(tab_strings[CH_PUBLIC]));
	tabs.add_tab(&scrolly_company, translator::translate(tab_strings[CH_COMPANY]));
	tabs.add_tab(&cont_tab_whisper, translator::translate(tab_strings[CH_WHISPER]));

	tabs.add_listener(this);
	add_component(&tabs);

	add_table(4,1);
	{
		bt_send_pos.init(button_t::posbutton | button_t::state, NULL);
		bt_send_pos.set_tooltip("Attach current coordinate to the comment");
		bt_send_pos.add_listener(this);
		add_component(&bt_send_pos);

		lb_channel.set_rigid(false);
		add_component(&lb_channel);

		inp_destination.set_text(ibuf_name, lengthof(ibuf_name));
		inp_destination.add_listener(this);
		add_component(&inp_destination);

		input.set_text(ibuf, lengthof(ibuf));
		input.add_listener(this);
		add_component(&input);
	}
	end_table();

	set_resizemode(diagonal_resize);

	fill_list();

	scrolly_public.set_maximize(true);
	scrolly_company.set_maximize(true);
	scrolly_whisper.set_maximize(true);
	reset_min_windowsize();
	set_windowsize(scr_size(D_DEFAULT_WIDTH, D_DEFAULT_HEIGHT));
	resize(scr_size(0, 0));
	//set_focus(&input);
}


void chat_frame_t::fill_list()
{
	uint8 chat_mode = tabs.get_active_tab_index();
	// keep focus // broken
	//const bool input_focued = input.get_focus();

	player_t* current_player = world()->get_active_player();

	//scr_coord_val old_size_h = cont_chat_log.get_size().h;
	gameinfo_t gi(world());

	lb_now_online.buf().printf(translator::translate("%u Client(s)\n"), (unsigned)gi.get_clients());
	lb_now_online.update();

	old_player_nr = current_player->get_player_nr();
	//scr_coord_val current_posy_from_buttom = cont_chat_log.get_size().h-scrolly.get_scroll_y();

	cont_chat_log[chat_mode].remove_all();
	cont_chat_log[chat_mode].new_component<gui_fill_t>(false, true);

	last_count = welt->get_chat_message()->get_list().get_count();

	plainstring prev_poster = "";
	sint8 prev_company=-1;
	bool player_locked= current_player->is_locked();
	for(chat_message_t::chat_node* const i : world()->get_chat_message()->get_list() ) {

		// fillter
		switch (chat_mode) {
			case CH_COMPANY:
				if (i->channel_nr!= current_player->get_player_nr()) {
					// other company's message
					continue;
				}
				if (player_locked && i->player_nr == current_player->get_player_nr()) {
					// no permission
					continue;
				}
				if (i->player_nr != current_player->get_player_nr() && !strcmp(env_t::nickname.c_str(),i->sender.c_str())) {
					continue;
				}

				break;
			case CH_WHISPER:
				if (i->destination==NULL || i->sender==NULL) {
					continue;
				}
				if (i->destination == "" || i->sender == "") {
					continue;
				}
				if (!strcmp(env_t::nickname.c_str(), i->destination.c_str()) && !strcmp(env_t::nickname.c_str(), i->sender.c_str())) {
					continue;
				}

				// direct chat mode
				if (ibuf_name[0]!=0 && (strcmp(ibuf_name, i->destination.c_str()) && strcmp(ibuf_name, i->sender.c_str()))) {
					continue;
				}

				if (strcmp(env_t::nickname.c_str(), i->destination.c_str())) {
					chat_history.append_unique(i->destination);
				}
				if (strcmp(env_t::nickname.c_str(), i->sender.c_str())) {
					chat_history.append_unique(i->sender);
				}

				break;
			default:
			case CH_PUBLIC:
				// system message and public chats
				if (i->channel_nr != -1) {
					continue; // other company's message
				}
				if (i->destination != NULL && i->destination != "") {
					continue; // direct message
				}
				break;
		}


		const bool continuous=strcmp(i->sender.c_str(), prev_poster.c_str())==0 && (i->player_nr==prev_company);
		cont_chat_log[chat_mode].new_component<chat_stats_t>(i, continuous);
		prev_poster = i->sender;
		prev_company = i->player_nr;
	}

	inp_destination.set_visible(tabs.get_active_tab_index()==CH_WHISPER);

	//if (input_focued) {
		//set_focus(&input); // cause crash
	//}

	switch (chat_mode) {
		default:
		case CH_PUBLIC:
			// system message and public chats
			lb_channel.set_visible(false);
			//cont_chat_log[0].set_size(cont_chat_log[0].get_min_size());
			//scrolly_public.set_size(scrolly_public.get_size());
			break;
		case CH_COMPANY:
			lb_channel.buf().append( current_player->get_name() );
			lb_channel.set_color( color_idx_to_rgb( current_player->get_player_color1()+env_t::gui_player_color_dark ));
			lb_channel.set_visible(true);
			lb_channel.update();
			//cont_chat_log[1].set_size(cont_chat_log[1].get_min_size());
			//scrolly_company.set_size(cont_chat_log[1].get_min_size());
			// keep old scroll y-position
			scrolly_company.set_scroll_position(0, cont_chat_log[1].get_min_size().h);
			break;
		case CH_WHISPER:
			if (cb_direct_chat_targets.count_elements() != chat_history.get_count()) {
				cb_direct_chat_targets.clear_elements();
				for (uint32 i=0; i<chat_history.get_count(); i++) {
					cb_direct_chat_targets.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(chat_history.get_element(i), SYSCOL_TEXT);
				}
			}

			cb_direct_chat_targets.set_visible(chat_history.get_count() && cb_direct_chat_targets.count_elements());

			//cont_chat_log[2].set_size(cont_chat_log[2].get_min_size());
			//scrolly_whisper.set_size(scrolly_whisper.get_min_size());
			break;
	}
	// keep old scroll y-position
	//scrolly.set_scroll_amount_y(scrolly.get_scroll_y() + cont_chat_log.get_size().h - old_size_h);
	//scrolly.set_scroll_amount_y(cont_chat_log.get_size().h);
	resize(scr_size(0,0));
}



bool chat_frame_t::action_triggered( gui_action_creator_t *comp, value_t v )
{
	if(  comp==&input  &&  ibuf[0]!=0  ) {
		const sint8 channel = tabs.get_active_tab_index()==CH_COMPANY ? (sint8)world()->get_active_player_nr() : -1;
		plainstring dest = tabs.get_active_tab_index() == CH_WHISPER ? ibuf_name : 0;

		if (dest!=0 && strcmp(dest.c_str(), env_t::nickname.c_str())==0) {
			return true; // message to myself
		}

		// Send chat message to server for distribution
		nwc_chat_t* nwchat = new nwc_chat_t( ibuf, welt->get_active_player()->get_player_nr(), (sint8)channel, env_t::nickname.c_str(), dest.c_str(), bt_send_pos.pressed ? world()->get_viewport()->get_world_position() : koord::invalid);
		network_send_server( nwchat );


		ibuf[0] = 0;
	}
	else if(  comp==&inp_destination  &&  tabs.get_active_tab_index()==CH_WHISPER  ) {
		lb_whisper_target.buf().printf("To: %s", ibuf_name);
		lb_whisper_target.update();
		fill_list();
	}
	else if(  comp==&opaque_bt  ) {
		if (!opaque_bt.pressed && env_t::chat_window_transparency != 100) {
			set_transparent(100 - env_t::chat_window_transparency, gui_theme_t::gui_color_chat_window_network_transparency);
		}
		else {
			set_transparent(0, gui_theme_t::gui_color_chat_window_network_transparency);
		}
		opaque_bt.pressed ^= 1;
	}
	else if(  comp==&bt_send_pos  ) {
		bt_send_pos.pressed ^= 1;
	}
	else if(  comp==&cb_direct_chat_targets  ) {
		if (!cb_direct_chat_targets.count_elements()) return true;
		activate_whisper_to(cb_direct_chat_targets.get_selected_item()->get_text());
		fill_list();
	}
	else if(  comp==&tabs  ) {
		fill_list();
		inp_destination.set_visible(tabs.get_active_tab_index() == CH_WHISPER);
		if( tabs.get_active_tab_index() == CH_WHISPER ) {
			lb_channel.buf().append("direct_chat_to:");
			lb_channel.set_color(SYSCOL_TEXT);
			lb_channel.set_visible(true);
			lb_channel.update();
		}
		//set_focus(&input);
		resize(scr_size(0, 0));
	}
	return true;
}



void chat_frame_t::draw(scr_coord pos, scr_size size)
{
	if(  welt->get_chat_message()->get_list().get_count() != last_count || old_player_nr != world()->get_active_player_nr()  ) {
		fill_list();
	}
	{
		set_dirty();
	}
	resize(scr_size(0, 0));
	gui_frame_t::draw(pos, size);
}


void chat_frame_t::rdwr(loadsave_t *file)
{
	// window size
	scr_size size = get_windowsize();
	size.rdwr( file );

	scrolly_public.rdwr(file);
	scrolly_company.rdwr(file);
	scrolly_whisper.rdwr(file);
	tabs.rdwr(file);
	file->rdwr_str( ibuf, lengthof(ibuf) );

	if(  file->is_loading()  ) {
		fill_list();

		reset_min_windowsize();
		set_windowsize(size);
	}
}

void chat_frame_t::activate_whisper_to(const char* recipient)
{
	if (strcmp(recipient, env_t::nickname.c_str())==0) {
		return; // message to myself
	}
	sprintf(ibuf_name, "%s", recipient);
	lb_whisper_target.buf().printf("To: %s", recipient);
	lb_whisper_target.update();

	inp_destination.set_text(ibuf_name, lengthof(ibuf_name));
	inp_destination.set_size(scr_size(proportional_string_width(ibuf_name), D_EDIT_HEIGHT));

	tabs.set_active_tab_index(CH_WHISPER);
	inp_destination.set_visible(true);
	lb_channel.buf().append("direct_chat_to:");
	lb_channel.set_color(SYSCOL_TEXT);
	lb_channel.set_visible(true);
	lb_channel.update();
	//set_focus(&input); // this cause the crash
	//fill_list();
	resize(scr_size(0,0));
}
