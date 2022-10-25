/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_FACTORY_CHART_H
#define GUI_FACTORY_CHART_H


#define MAX_PROD_LABEL      (7-1)
#define FACTORY_CHART_HEIGHT (90)

#include "../simfab.h"
#include "gui_frame.h"
#include "components/action_listener.h"
#include "components/gui_label.h"
#include "components/gui_button.h"
#include "components/gui_button_to_chart.h"
#include "components/gui_chart.h"
#include "components/gui_tab_panel.h"
#include "components/gui_aligned_container.h"
#include "components/gui_button_to_chart.h"
#include "components/gui_scrollpane.h"


class gui_factory_storage_label_t : public gui_label_buf_t
{
	const ware_production_t* ware;
	uint32 capacity;
	uint32 old_storage=-1;

public:
	gui_factory_storage_label_t(const ware_production_t* ware, uint32 capacity);

	void draw(scr_coord offset) OVERRIDE;
};


class gui_factory_monthly_prod_label_t : public gui_label_buf_t
{
	fabrik_t *fab;
	const goods_desc_t* goods;
	uint32 pfactor;
	bool is_input_item;

public:
	gui_factory_monthly_prod_label_t(fabrik_t *factory, const goods_desc_t* goods, uint32 factor, bool is_input_item = false);

	void draw(scr_coord offset) OVERRIDE;
};


class factory_chart_t : public gui_aligned_container_t
{
private:
	fabrik_t *factory;

	// Tab panel for grouping 2 sets of statistics
	gui_tab_panel_t tab_panel;

	// GUI components for input/output goods' statistics
	gui_aligned_container_t goods_cont;
	gui_chart_t goods_chart;
	gui_scrollpane_t scroll_goods;

	// GUI components for other production-related statistics
	gui_label_buf_t lb_productivity;
	gui_aligned_container_t prod_cont;
	gui_chart_t prod_chart;
	gui_scrollpane_t scroll_prod;

	// Variables for reference lines
	sint64 prod_ref_line_data[MAX_FAB_REF_LINE];

	gui_button_to_chart_array_t button_to_chart;

public:
	factory_chart_t(fabrik_t *_factory);
	virtual ~factory_chart_t();

	void set_factory(fabrik_t *_factory);

	void update(uint8 chart_tab_idx);

	void rdwr( loadsave_t *file );

	/**
	 * factory window will take our tabs,
	 * we only initialize them and update charts
	 */
	gui_tab_panel_t* get_tab_panel() { return &tab_panel; }
};

#endif
