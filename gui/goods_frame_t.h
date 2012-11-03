/*
 * Copyright (c) 1997 - 2003 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef goods_frame_t_h
#define goods_frame_t_h

#include "gui_frame.h"
#include "components/gui_button.h"
#include "components/gui_scrollpane.h"
#include "components/gui_label.h"
#include "components/gui_numberinput.h"
#include "components/action_listener.h"
#include "components/gui_combobox.h"
#include "goods_stats_t.h"
#include "../utils/cbuffer_t.h"

class karte_t;

/**
 * Shows statistics. Only goods so far.
 * @author Hj. Malthaner
 */
class goods_frame_t : public gui_frame_t, private action_listener_t
{
private:
	enum sort_mode_t { unsortiert=0, nach_name=1, nach_gewinn=2, nach_bonus=3, nach_catg=4, SORT_MODES=5 };
	static const char *sort_text[SORT_MODES];

	// static, so we remember the last settings
	static int relative_speed_change;
	// Distance in km
	static uint16 distance;
	// Distance in tiles
	static uint16 tile_distance;
	static uint8 comfort;
	static uint8 catering_level;
	static bool sortreverse;
	static sort_mode_t sortby;
	static bool filter_goods;
	//static waytype_t wtype;
	waytype_t wtype;

	karte_t* welt;
	char	speed_bonus[6];
	char	distance_txt[6];
	char	comfort_txt[6];
	char	catering_txt[6];
	cbuffer_t	speed_message;
	uint16 good_list[256];

	gui_label_t		sort_label;
	button_t		sortedby;
	button_t		sorteddir;
	gui_label_t		change_speed_label;
	gui_label_t		change_distance_label;
	gui_label_t		change_comfort_label;
	gui_label_t		change_catering_label;

	/*
	button_t		speed_up;
	button_t		speed_down;
	button_t		distance_up;
	button_t		distance_down;
	button_t		comfort_up;
	button_t		comfort_down;
	button_t		catering_up;
	button_t		catering_down;
	*/
	
	// replace button list with numberinput components for faster navigation
	// @author: HeinBloed, April 2012
	gui_numberinput_t distance_input, comfort_input, catering_input, speed_input;


	gui_combobox_t	way_type;
	button_t		filter_goods_toggle;

	goods_stats_t goods_stats;
	gui_scrollpane_t scrolly;

	// creates the list and pass it to the child finction good_stats, which does the display stuff ...
	static bool compare_goods(uint16, uint16);
	void sort_list();

public:
	goods_frame_t(karte_t *wl);

	/**
	* resize window in response to a resize event
	* @author Hj. Malthaner
	* @date   16-Oct-2003
	*/
	void resize(const koord delta);

	bool has_min_sizer() const {return true;}

    /**
     * Manche Fenster haben einen Hilfetext assoziiert.
     * @return den Dateinamen f�r die Hilfe, oder NULL
     * @author V. Meyer
     */
    const char * get_hilfe_datei() const {return "goods_filter.txt"; }

    /**
     * komponente neu zeichnen. Die �bergebenen Werte beziehen sich auf
     * das Fenster, d.h. es sind die Bildschirkoordinaten des Fensters
     * in dem die Komponente dargestellt wird.
     * @author Hj. Malthaner
     */
    void zeichnen(koord pos, koord gr);

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;
};

#endif
