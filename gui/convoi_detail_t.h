/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_CONVOI_DETAIL_T_H
#define GUI_CONVOI_DETAIL_T_H


#include "gui_frame.h"
#include "components/gui_aligned_container.h"
#include "components/gui_container.h"
#include "components/gui_scrollpane.h"
#include "components/gui_textarea.h"
#include "components/gui_textinput.h"
#include "components/gui_speedbar.h"
#include "components/gui_button.h"
#include "components/gui_label.h"
#include "components/gui_combobox.h"
#include "components/gui_tab_panel.h"
#include "components/action_listener.h"
#include "components/gui_convoy_formation.h"
#include "components/gui_chart.h"
#include "components/gui_button_to_chart.h"
#include "../convoihandle_t.h"
#include "simwin.h"

#include "vehicle_class_manager.h"

class scr_coord;

#define MAX_ACCEL_CURVES 4
#define MAX_FORCE_CURVES 2
#define MAX_PHYSICS_CURVES (MAX_ACCEL_CURVES+MAX_FORCE_CURVES)
#define SPEED_RECORDS 25


// helper class to show the colored acceleration text
class gui_acceleration_label_t : public gui_container_t
{
private:
	convoihandle_t cnv;
public:
	gui_acceleration_label_t(convoihandle_t cnv);

	scr_size get_min_size() const OVERRIDE { return get_size(); };
	scr_size get_max_size() const OVERRIDE { return get_min_size(); }

	void draw(scr_coord offset) OVERRIDE;
};

class gui_acceleration_time_label_t : public gui_container_t
{
private:
	convoihandle_t cnv;
public:
	gui_acceleration_time_label_t(convoihandle_t cnv);

	scr_size get_min_size() const OVERRIDE { return get_size(); };
	scr_size get_max_size() const OVERRIDE { return get_min_size(); }

	void draw(scr_coord offset) OVERRIDE;
};

class gui_acceleration_dist_label_t : public gui_container_t
{
private:
	convoihandle_t cnv;
public:
	gui_acceleration_dist_label_t(convoihandle_t cnv);

	scr_size get_min_size() const OVERRIDE { return get_size(); };
	scr_size get_max_size() const OVERRIDE { return get_min_size(); }

	void draw(scr_coord offset) OVERRIDE;
};


// content of payload info tab
class gui_convoy_payload_info_t : public gui_container_t
{
private:
	convoihandle_t cnv;
	bool show_detail = true; // Currently broken, always true

public:
	gui_convoy_payload_info_t(convoihandle_t cnv);

	void set_cnv(convoihandle_t c) { cnv = c; }
	void set_show_detail(bool yesno) { show_detail = yesno; } // Currently not in use

	void draw(scr_coord offset);
	void display_loading_bar(scr_coord_val xp, scr_coord_val yp, scr_coord_val w, scr_coord_val h, PIXVAL color, uint16 loading, uint16 capacity, uint16 overcrowd_capacity);
};

// content of maintenance info tab
class gui_convoy_maintenance_info_t : public gui_container_t
{
private:
	convoihandle_t cnv;
	bool any_obsoletes;

public:
	gui_convoy_maintenance_info_t(convoihandle_t cnv);

	void set_cnv(convoihandle_t c) { cnv = c; }

	void draw(scr_coord offset);
};

class gui_convoy_spec_table_t : public gui_aligned_container_t
{
	enum {
		SPECS_CAR_NUMBER = 0,
		SPECS_ROLE,
		SPECS_FREIGHT_TYPE,
		SPECS_ENGINE_TYPE,
		SPECS_POWER,
		SPECS_TRACTIVE_FORCE,
		SPECS_SPEED,
		SPECS_TARE_WEIGHT,
		SPECS_MAX_GROSS_WIGHT,
		SPECS_AXLE_LOAD,
		SPECS_LENGTH,          // for debug
		SPECS_BRAKE_FORCE,
		// good

		SPECS_RANGE,

		//--- maintenance values ---
		//SPECS_INCOME
		SPECS_RUNNING_COST,
		SPECS_FIXED_COST,
		SPECS_VALUE,
		SPECS_AGE,
		SPECS_TILTING,

		MAX_SPECS
	};
	enum {
		SPECS_DETAIL_START = SPECS_FREIGHT_TYPE
		//SPECS_PAYLOADS,
		//SPECS_COMFORT,
		//SPECS_CATERING
		//SPECS_MIN_LOADING_TIME
		//SPECS_MAX_LOADING_TIME
		//MAX_PAYLOAD_ROW
	};

	convoihandle_t cnv;
	cbuffer_t buf;

	// Insert rows that make up the spec table
	void insert_spec_rows();
	void insert_payload_rows();
public:
	gui_convoy_spec_table_t(convoihandle_t cnv);

	void set_cnv(convoihandle_t c) { cnv = c; }

	void draw(scr_coord offset) OVERRIDE;

	bool display_payload_table = false;

	using gui_aligned_container_t::get_min_size;
	using gui_aligned_container_t::get_max_size;
};

/**
 * Displays an information window for a convoi
 */
class convoi_detail_t : public gui_frame_t , private action_listener_t
{
	enum {
		CD_TAB_MAINTENANCE    = 0,
		CD_TAB_LOADED_DETAIL  = 1,
		CD_TAB_PHYSICS_CHARTS = 2,
		CD_TAB_SPEC_TABLE     = 3,
		CD_TAB_FARE_CHANGER   = 4
	};

public:
	enum sort_mode_t {
		by_destination = 0,
		by_via         = 1,
		by_amount_via  = 2,
		by_amount      = 3,
		SORT_MODES     = 4
	};

private:
	gui_aligned_container_t cont_maintenance, cont_payload, cont_chart_tab, cont_spec_tab;

	convoihandle_t cnv;

	gui_convoy_formation_t formation;
	gui_convoy_payload_info_t payload_info;
	gui_convoy_maintenance_info_t maintenance;
	gui_aligned_container_t cont_accel, cont_force;
	gui_convoy_spec_table_t spec_table;
	gui_convoy_fare_class_changer_t cont_fare_changer;
	gui_chart_t accel_chart, force_chart;

	gui_scrollpane_t scrolly_formation;
	gui_scrollpane_t scrolly_maintenance;
	gui_scrollpane_t scrolly_payload_info;
	gui_scrollpane_t scrolly_chart;
	gui_scrollpane_t scroll_spec;
	gui_scrollpane_t scroll_fare_changer;

	static sint16 tabstate;
	gui_tab_panel_t switch_chart;
	gui_tab_panel_t tabs;

	button_t sale_button;
	button_t withdraw_button;
	button_t retire_button;
	button_t display_detail_button;
	button_t bt_spec_table, bt_payload_table;

	button_t overview_selctor[gui_convoy_formation_t::CONVOY_OVERVIEW_MODES];
	gui_label_buf_t
		lb_vehicle_count,                   // for main frame
		lb_loading_time, lb_catering_level, // for payload tab
		lb_odometer, lb_value;              // for maintenance tab
	gui_acceleration_label_t      *lb_acceleration;
	gui_acceleration_time_label_t *lb_acc_time;
	gui_acceleration_dist_label_t *lb_acc_distance;

	gui_button_to_chart_array_t btn_to_accel_chart, btn_to_force_chart; //button_to_chart

	sint64 accel_curves[SPEED_RECORDS][MAX_ACCEL_CURVES];
	sint64 force_curves[SPEED_RECORDS][MAX_FORCE_CURVES];
	uint8 te_curve_abort_x = SPEED_RECORDS;

	bool has_min_sizer() const OVERRIDE { return true; }

	void update_labels();

	void init(convoihandle_t cnv);

	void set_tab_opened();

public:
	convoi_detail_t(convoihandle_t cnv = convoihandle_t());

	void draw(scr_coord pos, scr_size size) OVERRIDE;

	const char * get_help_filename() const OVERRIDE {return "convoidetail.txt"; }

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	/**
	 * called when convoi was renamed
	 */
	void update_data() { set_dirty(); }

	void rdwr( loadsave_t *file ) OVERRIDE;

	uint32 get_rdwr_id() OVERRIDE { return magic_convoi_detail; }
};

#endif
