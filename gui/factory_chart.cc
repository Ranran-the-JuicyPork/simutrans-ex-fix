/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "../obj/leitung2.h"
#include "../dataobj/translator.h"
#include "../dataobj/environment.h"

#include "factory_chart.h"
#include "components/gui_colorbox.h"
#include "components/gui_divider.h"
#include "components/gui_image.h"

static const char *const header_text[8] =
{
	"Storage", "In Transit", "Arrived", "Consumed", "Unit", "Delivered", "", "Produced"
};

static const gui_chart_t::convert_proc goods_convert[6] =
{
	NULL, NULL, convert_goods,
	NULL, NULL, convert_goods
};


static const uint8 chart_type[MAX_FAB_STAT] =
{
	gui_chart_t::PERCENT, gui_chart_t::STANDARD, gui_chart_t::PERCENT, gui_chart_t::PERCENT,
	gui_chart_t::PERCENT, gui_chart_t::STANDARD, gui_chart_t::STANDARD, gui_chart_t::STANDARD,
	gui_chart_t::STANDARD, gui_chart_t::STANDARD, gui_chart_t::STANDARD, gui_chart_t::STANDARD
};

static const chart_marker_t goods_marker_type[6] =
{
	chart_marker_t::round_box, chart_marker_t::square, chart_marker_t::cross,
	chart_marker_t::round_box, chart_marker_t::none, chart_marker_t::square
};

static const uint8 goods_button_to_chart[6] =
{
	FAB_GOODS_TRANSIT, FAB_GOODS_RECEIVED, FAB_GOODS_CONSUMED,
	FAB_GOODS_DELIVERED, 0, FAB_GOODS_PRODUCED
};

// Corresponds to prod stats table columns
static const chart_marker_t marker_type[6] =
{
	chart_marker_t::diamond, chart_marker_t::none, chart_marker_t::square, chart_marker_t::round_box, chart_marker_t::cross, chart_marker_t::none
};

static const gui_chart_t::convert_proc ref_convert[MAX_FAB_REF_LINE] =
{
	convert_boost, convert_boost, convert_boost, convert_power, NULL, NULL
};

static const uint8 ref_chart_type[MAX_FAB_REF_LINE] =
{
	gui_chart_t::PERCENT, gui_chart_t::PERCENT, gui_chart_t::PERCENT, gui_chart_t::KW, gui_chart_t::STANDARD, gui_chart_t::STANDARD
};

static const char *const prod_ref_text[4] =
{
	"Power demand", "Worker", "Visitor demand", "Mail demand/output"
};

static const char *const prod_help_text[MAX_FAB_STAT] =
{
	"Power usage", "Power output", "boost_by_power_supply",
	"Commuter arrivals", "", "boost_by_staffing",
	"Visitor arrivals", "",	"",
	"Number of mails received", "sended", "boost_by_mailing"
};

static const uint8 prod_color[MAX_FAB_STAT] =
{
	COL_ELECTRICITY-1, COL_ELECTRICITY+1, COL_LIGHT_RED,
	COL_COMMUTER, 0, COL_LIGHT_TURQUOISE,
	COL_LIGHT_PURPLE, 0, 0,
	COL_YELLOW, 159, COL_ORANGE+1
};


static const PIXVAL prod_ref_color[8] =
{
	57799, 991, 63535, 64773, // 0-3: demands
	61916, 1370, 0, 64448     // 4-7: max boost
};

static const PIXVAL prod_ref_offset[8] =
{
	FAB_REF_DEMAND_ELECTRIC,    FAB_REF_DEMAND_PAX,    0, FAB_REF_DEMAND_MAIL,   // 0-3: demands
	FAB_REF_MAX_BOOST_ELECTRIC, FAB_REF_MAX_BOOST_PAX, 0, FAB_REF_MAX_BOOST_MAIL // 4-7: max boost
};


// unused in Exteded: FAB_PAX_GENERATED, FAB_MAIL_GENERATED, FAB_PAX_DEPARTED
static const uint8 prod_button_to_chart[] =
{
	FAB_POWER,            FAB_POWER,         FAB_BOOST_ELECTRIC,
	FAB_PAX_ARRIVED,      MAX_FAB_STAT,      FAB_BOOST_PAX,
	FAB_CONSUMER_ARRIVED, MAX_FAB_STAT,      MAX_FAB_STAT,
	FAB_MAIL_ARRIVED,     FAB_MAIL_DEPARTED, FAB_BOOST_MAIL
};


gui_factory_storage_label_t::gui_factory_storage_label_t(const ware_production_t *ware, uint32 capacity)
{
	this->ware = ware;
	this->capacity = capacity;
}

void gui_factory_storage_label_t::draw(scr_coord offset)
{
	if (old_storage != ware->get_storage()) {
		old_storage = ware->get_storage();
		if (ware->get_typ()->is_available()) {
			buf().printf("%3u/%u", old_storage, capacity);
			set_color(SYSCOL_TEXT);
		}
		else {
			buf().printf(" %s", translator::translate("n/a"));
			set_color(SYSCOL_TEXT_WEAK);
		}
		update();
	}
	gui_label_buf_t::draw(offset);
}


gui_factory_monthly_prod_label_t::gui_factory_monthly_prod_label_t(fabrik_t *factory, const goods_desc_t *goods, uint32 factor, bool is_input)
{
	fab = factory;
	this->goods = goods;
	is_input_item = is_input;
	pfactor = factor;
}

void gui_factory_monthly_prod_label_t::draw(scr_coord offset)
{
	const uint32 monthly_prod = (uint32)(fab->get_current_production()*pfactor * 10 >> DEFAULT_PRODUCTION_FACTOR_BITS);
	if (is_input_item) {
		if (monthly_prod < 100) {
			buf().printf("%.1f", (float)monthly_prod/10.0);
		}
		else {
			buf().printf("%u", monthly_prod/10);
		}
	}
	else {
		if (monthly_prod < 100) {
			buf().printf("%.1f", (float)monthly_prod/10.0);
		}
		else {
			buf().printf("%u", monthly_prod/10);
		}
	}
	update();
	gui_label_buf_t::draw(offset);
}


factory_chart_t::factory_chart_t(fabrik_t *_factory) :
	scroll_goods(&goods_cont, true, true),
	scroll_prod(&prod_cont, true, true)
{
	set_factory(_factory);
}

void factory_chart_t::set_factory(fabrik_t *_factory)
{
	factory = _factory;
	if (factory == NULL) {
		return;
	}

	remove_all();
	goods_cont.remove_all();
	prod_cont.remove_all();
	button_to_chart.clear();

	set_table_layout(1,0);
	add_component( &tab_panel );

	const uint32 input_count = factory->get_input().get_count();
	const uint32 output_count = factory->get_output().get_count();
	if(  input_count>0  ||  output_count>0  ) {
		// only add tab if there is something to display
		tab_panel.add_tab(&scroll_goods, translator::translate("Goods"));
		goods_cont.set_table_layout(1, 0);

		// first tab: charts for goods production/consumption

		// input_output = is_output_storage
		goods_cont.add_table(8,0);
		{
			for( uint8 input_output=0; input_output<2; input_output++ ) {
				//const bool is_output = (input_output==1);
				if( (input_output==0 && !input_count)  ||  (input_output==1 && !output_count) ) continue;
				const uint8 item_count = input_output ? output_count:input_count;
				// header
				goods_cont.new_component<gui_image_t>()->set_image(skinverwaltung_t::input_output ? skinverwaltung_t::input_output->get_image_id(input_output) : IMG_EMPTY, true);
				goods_cont.new_component_span<gui_label_t>(header_text[0], SYSCOL_TEXT_HIGHLIGHT, gui_label_t::centered, 2);
				goods_cont.new_component<gui_label_t>(header_text[1+input_output*4], SYSCOL_TEXT_HIGHLIGHT);
				goods_cont.new_component<gui_label_t>(header_text[2+input_output*4], SYSCOL_TEXT_HIGHLIGHT);
				goods_cont.new_component_span<gui_label_t>(header_text[3+input_output*4], SYSCOL_TEXT_HIGHLIGHT, 2);
				goods_cont.new_component<gui_label_t>(header_text[4], SYSCOL_TEXT_HIGHLIGHT);

				goods_cont.new_component_span<gui_border_t>(3);
				goods_cont.new_component<gui_border_t>();
				if (input_output) {
					goods_cont.new_component<gui_empty_t>();
				}
				else {
					goods_cont.new_component<gui_border_t>();
				}
				goods_cont.new_component_span<gui_border_t>(2);
				goods_cont.new_component<gui_border_t>();

				// create table of buttons, insert curves to chart
				const array_tpl<ware_production_t>& material = input_output ? factory->get_output() : factory->get_input();
				for (uint32 g=0; g < item_count; ++g) {
					const bool is_available = material[g].get_typ()->is_available();
					const PIXVAL goods_color = color_idx_to_rgb(material[g].get_typ()->get_color_index());

					// goods name
					goods_cont.new_component<gui_colorbox_t>(goods_color)->set_size(GOODS_COLOR_BOX_SIZE);
					button_t* b = goods_cont.new_component<button_t>();
					b->init(button_t::chart_marker_state_automatic | button_t::flexible, material[g].get_typ()->get_name());
					b->background_color = goods_color;
					b->set_marker_style(marker_type[0] | draw_horizontal_line);
					//b->set_tooltip("");
					b->pressed = false;
					b->enable(is_available);

					uint16 curve = goods_chart.add_curve(goods_color, material[g].get_stats(), MAX_FAB_GOODS_STAT, FAB_GOODS_STORAGE, MAX_MONTH, false, false, true, 0, convert_goods, marker_type[0]);
					button_to_chart.append(b, &goods_chart, curve);

					const sint64 pfactor = input_output ? (sint64)factory->get_desc()->get_product(g)->get_factor()
						: (factory->get_desc()->get_supplier(g) ? (sint64)factory->get_desc()->get_supplier(g)->get_consumption() : 1ll);
					goods_cont.new_component<gui_factory_storage_label_t>(&material[g], (uint32)material[g].get_capacity(pfactor));

					// 4-6
					for (uint8 col = 4; col < 7; col++) {
						if (input_output && col==5) {
							goods_cont.new_component<gui_empty_t>();
							continue;
						}
						const uint8 goods_chart_index = col-4 + input_output*3;
						PIXVAL chart_col= goods_color;
						if (is_dark_color(goods_color)) {
							chart_col = display_blend_colors(goods_color, color_idx_to_rgb(COL_WHITE), (col-3) * 18);
						}
						else {
							chart_col = display_blend_colors(goods_color, color_idx_to_rgb(COL_BLACK), 56 - (col-3)*14);
						}
						uint16 curve = goods_chart.add_curve(chart_col, material[g].get_stats(), MAX_FAB_GOODS_STAT, goods_button_to_chart[goods_chart_index], MAX_MONTH, false, false, true, 0, goods_convert[goods_chart_index], goods_marker_type[goods_chart_index]);

						button_t* b = goods_cont.new_component<button_t>();
						b->init(button_t::chart_marker_state_automatic, NULL);
						b->set_marker_style(goods_marker_type[goods_chart_index] | draw_horizontal_line);
						b->background_color = chart_col;
						b->pressed = false;
						b->enable(is_available);
						button_to_chart.append(b, &goods_chart, curve);
					}
					// col-7: basic usaege
					goods_cont.new_component<gui_factory_monthly_prod_label_t>(factory, material[g].get_typ(), pfactor, input_output)->set_tooltip( translator::translate("basic_prod_per_month") );
					// col-8: unit
					goods_cont.new_component<gui_label_t>(material[g].get_typ()->get_mass());
				}
				goods_cont.new_component_span<gui_margin_t>(0, D_V_SPACE,8);
			}
		}
		goods_cont.end_table();

		goods_cont.add_component(&goods_chart);

		// GUI components for goods input/output statistics
		goods_chart.set_min_size(scr_size(D_DEFAULT_WIDTH - D_MARGIN_LEFT - D_MARGIN_RIGHT, FACTORY_CHART_HEIGHT));
		goods_chart.set_dimension(12, 10000);
		goods_chart.set_background(SYSCOL_CHART_BACKGROUND);
		goods_chart.set_ltr(env_t::left_to_right_graphs);

		goods_cont.new_component<gui_empty_t>();
	}

	tab_panel.add_tab( &scroll_prod, translator::translate("Production/Boost") );
	prod_cont.set_table_layout(1,0);

	// GUI components for other production-related statistics
	prod_chart.set_min_size(scr_size(D_DEFAULT_WIDTH - D_MARGIN_LEFT - D_MARGIN_RIGHT, FACTORY_CHART_HEIGHT));
	prod_chart.set_dimension(12, 10000);
	prod_chart.set_background(SYSCOL_CHART_BACKGROUND);
	prod_chart.set_ltr(env_t::left_to_right_graphs);

	// demand display
	prod_cont.add_table(6,0);
	{
		// header
		prod_cont.new_component_span<gui_label_t>("Demand", SYSCOL_TEXT_HIGHLIGHT, gui_label_t::centered, 2);
		prod_cont.new_component<gui_label_t>("Arrived", SYSCOL_TEXT_HIGHLIGHT);
		prod_cont.new_component<gui_label_t>("sended", SYSCOL_TEXT_HIGHLIGHT);
		prod_cont.new_component<gui_label_t>("Boost (%)", SYSCOL_TEXT_HIGHLIGHT);
		prod_cont.new_component<gui_label_t>("Max Boost (%)", SYSCOL_TEXT_HIGHLIGHT);

		prod_cont.new_component_span<gui_border_t>(2);
		prod_cont.new_component<gui_border_t>();
		prod_cont.new_component<gui_border_t>();
		prod_cont.new_component<gui_border_t>();
		prod_cont.new_component<gui_border_t>();

		for (uint8 r=0; r<4; r++) {
			gebaeude_t *gb = factory->get_building();
			// no demand => skip!
			if( (r==0  &&  factory->get_scaled_electric_demand()==0)
				||  (r > 0 && !gb) ) {
				continue;
			}
			if( r==1  &&  !gb->get_adjusted_jobs() )           continue;
			if( r==2  &&  !gb->get_adjusted_visitor_demand() ) continue;
			if( r==3  &&  !gb->get_adjusted_mail_demand() )    continue;

			// col-0: demand ref button(label)
			if( r==0 && factory->get_desc()->is_electricity_producer() ) {
				prod_cont.new_component<gui_label_t>("Electrical output:", SYSCOL_TEXT, gui_label_t::centered);
			}
			else if( r==2 ) {
				prod_cont.new_component<gui_label_t>(prod_ref_text[r], SYSCOL_TEXT, gui_label_t::centered);
			}
			else {
				button_t* b = prod_cont.new_component<button_t>();
				b->init( button_t::chart_marker_state_automatic | button_t::flexible, prod_ref_text[r] );
				b->background_color = prod_ref_color[r];
				b->set_marker_style(marker_type[0]|draw_horizontal_line);
				b->pressed = false;
				// add ref line
				uint16 curve = prod_chart.add_curve(prod_ref_color[r], prod_ref_line_data + prod_ref_offset[r], 0, 0, MAX_MONTH, ref_chart_type[prod_ref_offset[r]], false, true, 0, ref_convert[prod_ref_offset[r]], marker_type[0]);
				button_to_chart.append(b, &prod_chart, curve);
			}

			// col-1: demand label
			switch (r)
			{
				case 0: // power
				{
					gui_label_buf_t *lb = prod_cont.new_component<gui_label_buf_t>();
					if (factory->get_desc()->is_electricity_producer()) {
						lb->buf().append(factory->get_scaled_electric_demand() >> POWER_TO_MW);
						lb->buf().append(" MW");
					}
					else {
						lb->buf().append((factory->get_scaled_electric_demand() * 1000) >> POWER_TO_MW);
						lb->buf().append(" KW");
					}
					lb->update();
					break;
				}
				case 1: // job demand
				{
					gui_label_buf_t *lb = prod_cont.new_component<gui_label_buf_t>();
#ifdef DEBUG
					lb->buf().printf(": %d/%d", gb->get_adjusted_jobs() - gb->check_remaining_available_jobs(), gb->get_adjusted_jobs());
#else
					lb->buf().printf(": %d/%d", gb->get_adjusted_jobs() - max(0, gb->check_remaining_available_jobs()), gb->get_adjusted_jobs());
#endif
					lb->update();
					break;
				}
				case 2: // visitor demand
				{
					gui_label_buf_t *lb = prod_cont.new_component<gui_label_buf_t>();
					lb->buf().printf(": %d", gb->get_adjusted_visitor_demand());
					lb->update();
					break;
				}
				case 3: // mail demand
				{
					gui_label_buf_t *lb = prod_cont.new_component<gui_label_buf_t>();
					lb->buf().printf(": %d", gb->get_adjusted_mail_demand());
					lb->update();
					break;
				}
				default:
					prod_cont.new_component<gui_empty_t>();
					break;
			}

			// col-2to4
			const bool display_boost = (r!=2) &&
				(      (r==0 && factory->get_desc()->get_electric_boost() && !factory->get_desc()->is_electricity_producer())
					|| (r==1 && factory->get_desc()->get_pax_boost()      )
					|| (r==3 && factory->get_desc()->get_mail_boost()     )
				);
			for (uint8 col=2; col<5; col++) {
				const uint8 prod_chart_index = r*3 + col-2;
				const uint8 s = prod_button_to_chart[prod_chart_index];

				if( (col==4 && !display_boost)
					|| s==MAX_FAB_STAT
					|| (r == 0 && (col == 2 && factory->get_desc()->is_electricity_producer() || col == 3 && !factory->get_desc()->is_electricity_producer())))
				{
					prod_cont.new_component<gui_empty_t>();
				}
				else {
					button_t* b = prod_cont.new_component<button_t>();
					b->init(button_t::chart_marker_state_automatic, NULL);
					b->set_tooltip(prod_help_text[prod_chart_index]);
					b->background_color = color_idx_to_rgb(prod_color[prod_chart_index]);
					b->set_marker_style(marker_type[col] | draw_horizontal_line);
					b->pressed = false;
					// add curve
					uint16 curve = prod_chart.add_curve(color_idx_to_rgb(prod_color[prod_chart_index]), factory->get_stats(), MAX_FAB_STAT, s, MAX_MONTH, chart_type[s], false, true, 0, (r==0&&col<4)?convert_power:NULL, marker_type[col]);
					button_to_chart.append(b, &prod_chart, curve);
				}

			}

			// col-5: max. boost chart button
			if (!display_boost) {
				prod_cont.new_component<gui_empty_t>();
			}
			else {
				button_t* b = prod_cont.new_component<button_t>();
				b->init(button_t::chart_marker_state_automatic, NULL);
				b->background_color = prod_ref_color[r+4];
				b->set_marker_style(marker_type[5] | draw_horizontal_line);
				b->pressed = false;
				// add ref line
				uint16 curve = prod_chart.add_curve(prod_ref_color[r+4], prod_ref_line_data + prod_ref_offset[r+4], 0, 0, MAX_MONTH, ref_chart_type[prod_ref_offset[r+4]], false, true, 0, ref_convert[prod_ref_offset[r+4]], marker_type[5]);
				button_to_chart.append(b, &prod_chart, curve);
			}
		}

		prod_cont.new_component_span<gui_border_t>(6);

		// operation rate
		prod_cont.new_component<gui_label_t>("Productivity");
		prod_cont.new_component<gui_empty_t>();
		prod_cont.new_component_span<gui_empty_t>(2);
		prod_cont.add_component(&lb_productivity);
		lb_productivity.set_align(gui_label_t::centered);
		gui_label_buf_t *lb = prod_cont.new_component<gui_label_buf_t>(SYSCOL_TEXT, gui_label_t::centered);
		const uint32 max_productivity = (100 * (factory->get_desc()->get_electric_boost() + factory->get_desc()->get_pax_boost() + factory->get_desc()->get_mail_boost())) >> DEFAULT_PRODUCTION_FACTOR_BITS;
		lb->buf().printf("%d%%", max_productivity + 100);
		lb->update();

		button_t* b = prod_cont.new_component<button_t>();
		b->init(button_t::chart_marker_state_automatic | button_t::flexible, "Operation rate");
		b->set_marker_style(marker_type[0] | draw_horizontal_line);
		b->background_color = color_idx_to_rgb(COL_BROWN);
		b->pressed = false;
		uint16 curve = prod_chart.add_curve(color_idx_to_rgb(COL_BROWN), factory->get_stats(), MAX_FAB_STAT, FAB_PRODUCTION, MAX_MONTH, gui_chart_t::PERCENT, false, true, 2, NULL, marker_type[0]);
		button_to_chart.append(b, &prod_chart, curve);

	}
	prod_cont.end_table();

	prod_cont.new_component<gui_margin_t>(0,D_V_SPACE);

	prod_cont.add_component( &prod_chart );

	set_size(get_min_size());
	// initialize reference lines' data (these do not change over time)
	prod_ref_line_data[FAB_REF_MAX_BOOST_ELECTRIC] = factory->get_desc()->get_electric_boost();
	prod_ref_line_data[FAB_REF_MAX_BOOST_PAX] = factory->get_desc()->get_pax_boost();
	prod_ref_line_data[FAB_REF_MAX_BOOST_MAIL] = factory->get_desc()->get_mail_boost();
}


factory_chart_t::~factory_chart_t()
{
	button_to_chart.clear();
}


void factory_chart_t::update(uint8 tab_idx)
{
	// update reference lines' data (these might change over time)
	prod_ref_line_data[FAB_REF_DEMAND_ELECTRIC] = ( factory->get_desc()->is_electricity_producer() ? 0 : factory->get_scaled_electric_demand()*1000 );
	prod_ref_line_data[FAB_REF_DEMAND_PAX] = factory->get_scaled_pax_demand();
	prod_ref_line_data[FAB_REF_DEMAND_MAIL] = factory->get_scaled_mail_demand();

	switch (tab_idx) {
		case 1:
		{
			scroll_prod.set_show_scroll_x(scroll_prod.get_size().h > D_SCROLLBAR_HEIGHT );
			lb_productivity.buf().printf("%u%% ", factory->get_actual_productivity());
			lb_productivity.update();
			break;
		}
		default:
		case 0:
			scroll_goods.set_show_scroll_x(scroll_goods.get_size().h > D_SCROLLBAR_HEIGHT);
			break;
	}
}


void factory_chart_t::rdwr( loadsave_t *file )
{
	// button-to-chart array
	button_to_chart.rdwr(file);
}
