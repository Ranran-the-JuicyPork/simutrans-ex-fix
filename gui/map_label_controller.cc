/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "../simcolor.h"
#include "../simevent.h"
#include "../display/simimg.h"
#include "../simworld.h"
#include "../simskin.h"
#include "../sys/simsys.h"
#include "../display/simgraph.h"
#include "../descriptor/skin_desc.h"
#include "../dataobj/environment.h"

#include "simwin.h"
#include "map_label_controller.h"
#include "components/gui_image.h"

#include "../boden/wege/kanal.h"
#include "../boden/wege/maglev.h"
#include "../boden/wege/monorail.h"
#include "../boden/wege/narrowgauge.h"
#include "../boden/wege/runway.h"
#include "../boden/wege/schiene.h"
#include "../boden/wege/strasse.h"


//static const char * line_alert_helptexts[3] = {};

map_label_controller_t::map_label_controller_t() : gui_frame_t("")
{
	set_table_layout(1,0);
	bt_station_name_enable.init(button_t::square_state, "show station names");
	bt_station_name_enable.add_listener(this);
	add_component(&bt_station_name_enable);

	add_table(2,1);
	{
		new_component<gui_margin_t>(D_CHECKBOX_WIDTH);
		add_table(2,3);
		{
			new_component<gui_label_t>("Spieler"); // player
			cb_station_name_filter_player.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("All"), SYSCOL_TEXT);
			cb_station_name_filter_player.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Owned"), SYSCOL_TEXT);
			cb_station_name_filter_player.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Use"), SYSCOL_TEXT);
			cb_station_name_filter_player.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Accessable"), SYSCOL_TEXT);
			cb_station_name_filter_player.set_selection(env_t::station_name_filter_player);
			cb_station_name_filter_player.add_listener(this);
			add_component(&cb_station_name_filter_player);

			new_component<gui_label_t>("waytype");
			add_table(2,1);
			{
				bt_station_all_waytype.init(button_t::square_state, "All");
				//bt_station_all_waytype.pressed=; // TODO
				add_component(&bt_station_all_waytype);

				// now add all valid waytype buttons
				add_table(4,0);
				{
					uint8 max_idx = 0;
					if (maglev_t::default_maglev) {
						bt_station_filter_waytype[max_idx].set_typ(button_t::box_state);
						bt_station_filter_waytype[max_idx].set_image(skinverwaltung_t::get_waytype_skin(maglev_wt)->get_image_id(0));
						bt_station_filter_waytype[max_idx].set_tooltip(translator::translate("Maglev"));
						bt_station_filter_waytype[max_idx].background_color = world()->get_settings().get_waytype_color(maglev_wt);
						max_idx++;
					}
				}
				end_table();

				/*
				if (maglev_t::default_maglev) {
					add_tab(c, translator::translate("Maglev"), skinverwaltung_t::maglevhaltsymbol, translator::translate("Maglev"), world()->get_settings().get_waytype_color(maglev_wt));
					tabs_to_waytype[max_idx++] = maglev_wt;
				}
				if (monorail_t::default_monorail) {
					add_tab(c, translator::translate("Monorail"), skinverwaltung_t::monorailhaltsymbol, translator::translate("Monorail"), world()->get_settings().get_waytype_color(monorail_wt));
					tabs_to_waytype[max_idx++] = monorail_wt;
				}
				if (schiene_t::default_schiene) {
					add_tab(c, translator::translate("Train"), skinverwaltung_t::zughaltsymbol, translator::translate("Train"), world()->get_settings().get_waytype_color(track_wt));
					tabs_to_waytype[max_idx++] = track_wt;
				}
				if (narrowgauge_t::default_narrowgauge) {
					add_tab(c, translator::translate("Narrowgauge"), skinverwaltung_t::narrowgaugehaltsymbol, translator::translate("Narrowgauge"), world()->get_settings().get_waytype_color(narrowgauge_wt));
					tabs_to_waytype[max_idx++] = narrowgauge_wt;
				}
				if (!vehicle_builder_t::get_info(tram_wt).empty()) {
					add_tab(c, translator::translate("Tram"), skinverwaltung_t::tramhaltsymbol, translator::translate("Tram"), world()->get_settings().get_waytype_color(tram_wt));
					tabs_to_waytype[max_idx++] = tram_wt;
				}
				if (strasse_t::default_strasse) {
					add_tab(c, translator::translate("Truck"), skinverwaltung_t::autohaltsymbol, translator::translate("Truck"), world()->get_settings().get_waytype_color(road_wt));
					tabs_to_waytype[max_idx++] = road_wt;
				}
				if (!vehicle_builder_t::get_info(water_wt).empty()) {
					add_tab(c, translator::translate("Ship"), skinverwaltung_t::schiffshaltsymbol, translator::translate("Ship"), world()->get_settings().get_waytype_color(water_wt));
					tabs_to_waytype[max_idx++] = water_wt;
				}
				if (runway_t::default_runway) {
					add_tab(c, translator::translate("Air"), skinverwaltung_t::airhaltsymbol, translator::translate("Air"), world()->get_settings().get_waytype_color(air_wt));
					tabs_to_waytype[max_idx++] = air_wt;
				}
				*/
			}
			end_table();

			new_component<gui_label_t>("Fracht"); // freight
			add_table(3,1)->set_spacing(scr_size(0,0));
			{
				bt_station_filter_freight_type[0].init(button_t::roundbox_left_state,NULL);
				bt_station_filter_freight_type[0].set_image(skinverwaltung_t::passengers->get_image_id(0));
				bt_station_filter_freight_type[1].init(button_t::roundbox_middle_state, NULL);
				bt_station_filter_freight_type[1].set_image(skinverwaltung_t::mail->get_image_id(0));
				bt_station_filter_freight_type[2].init(button_t::roundbox_right_state, NULL);
				bt_station_filter_freight_type[2].set_image(skinverwaltung_t::goods->get_image_id(0));

				for (uint8 i=0; i<3; ++i) {
					bt_station_filter_freight_type[i].pressed = !(env_t::station_name_filter_capacity_type & (1 << i));
					bt_station_filter_freight_type[i].add_listener(this);
					add_component(&bt_station_filter_freight_type[i]);
				}
				//bt_ft_filter[0].init(button_t::roundbox_state, factory_type_text[0], scr_coord(0, 0), scr_size(proportional_string_width(translator::translate("All")) + D_BUTTON_PADDINGS_X, D_BUTTON_HEIGHT));
				////filter_buttons[0].pressed = (factory_type_filter_bits == 255);
				//filter_buttons[0].add_listener(this);
				//add_component(&filter_buttons[0]);
			}
			end_table();
		}
		end_table();
	}
	end_table();




	reset_min_windowsize();
}

void map_label_controller_t::draw(scr_coord pos, scr_size size)
{
		bt_station_name_enable.pressed = env_t::show_names & 1;
		gui_frame_t::draw(pos, size);
}

bool map_label_controller_t::action_triggered( gui_action_creator_t *comp, value_t)
{
	if(  comp==&bt_station_name_enable  ) {
		bt_station_name_enable.pressed = !bt_station_name_enable.pressed;
		if (bt_station_name_enable.pressed) {
			env_t::show_names++;
		}
		else {
			env_t::show_names--;
		}
		cb_station_name_filter_player.enable(bt_station_name_enable.pressed);
		bt_station_all_waytype.enable(bt_station_name_enable.pressed);
	}
	else if(  comp==&cb_station_name_filter_player  ) {
		env_t::station_name_filter_player = cb_station_name_filter_player.get_selection();
	}
	else {
		for (uint8 i = 0; i < 3; ++i) {
			if (comp == &bt_station_filter_freight_type[i]) {
				bt_station_filter_freight_type[i].pressed = !bt_station_filter_freight_type[i].pressed;
				env_t::station_name_filter_capacity_type ^= (1 << i);
			}
		}
	}
	world()->set_dirty();
	return true;
}

