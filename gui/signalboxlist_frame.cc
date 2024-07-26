// ****************** List of all signalboxes ************************


#include "signalboxlist_frame.h"
#include "gui_theme.h"
#include "../simskin.h"
#include "../simcity.h"
#include "../dataobj/translator.h"
#include "../descriptor/skin_desc.h"
#include "../descriptor/building_desc.h"
#include "../utils/simstring.h"
#include "../display/viewport.h"


static const char* const sb_header_text[signalboxlist_row_t::MAX_COLS] =
{
	"sb_type",
	"sb_connected",
	"Max. signals",
	"Radius",
	"koord", "by_region", "City", "Fixed Costs", "Built in"
};

int signalboxlist_row_t::sort_mode = SB_BUILT_DATE;
bool signalboxlist_row_t::sortreverse = false;

bool signalboxlist_frame_t::filter_has_vacant_slot = false;

static karte_ptr_t welt;


signalbox_radius_cell_t::signalbox_radius_cell_t(uint32 radius)
	: value_cell_t((sint64)radius, gui_chart_t::DISTANCE, centered)
{
	set_value(radius);
	set_size(min_size);
}

void signalbox_radius_cell_t::set_value(sint64 value_)
{
	value = value_;
	buf.clear();

	if (value == 0) {
		buf.append(translator::translate("infinite_range"));
	}
	else if (value < 1000) {
		buf.append(value);
		buf.append("m");
	}
	else {
		const uint8 digit = value < 20000 ? 1 : 0;
		buf.append((double)value / 1000.0, digit);
		buf.append("km");
	}
	min_size = scr_size(proportional_string_width(buf), LINESPACE);
	set_width(size.w - L_CELL_PADDING * 2); // recalc draw_offset.x
}

signalboxlist_row_t::signalboxlist_row_t(signalbox_t *sb)
{
	assert(sb != NULL);
	this->sb = sb;

	// 1. name
	new_component<text_cell_t>(sb->get_name());
	// 2. connected
	old_connected = sb->get_number_of_signals_controlled_from_this_box();
	const uint16 sb_capacity = sb->get_capacity();
	new_component<value_cell_t>((sint64)old_connected, gui_chart_t::STANDARD, table_cell_item_t::right, old_connected==sb_capacity ? COL_DANGER : COL_SAFETY);
	// 3. capacity (static)
	new_component<value_cell_t>((sint64)sb_capacity, gui_chart_t::STANDARD, table_cell_item_t::right);
	// 4. radius
	new_component<signalbox_radius_cell_t>((sint64)sb->get_tile()->get_desc()->get_radius());
	// 5. coord
	const koord sb_pos = sb->get_pos().get_2d();
	new_component<coord_cell_t>(sb_pos, table_cell_item_t::centered);
	// 6. region
	new_component<text_cell_t>(welt->get_settings().regions.empty() ? "" : translator::translate(welt->get_region_name(sb_pos).c_str()));
	// 7. city
	stadt_t* c = welt->get_city(sb_pos);
	new_component<coord_cell_t>(c ? c->get_name() : "-", c ? c->get_center() : koord::invalid);
	// 8. Fixed Costs
	sint64 maintenance = sb->get_tile()->get_desc()->get_maintenance();
	if (maintenance == PRICE_MAGIC)
	{
		maintenance = sb->get_tile()->get_desc()->get_level() * welt->get_settings().maint_building;
	}
	new_component<value_cell_t>((double)welt->calc_adjusted_monthly_figure(maintenance), gui_chart_t::MONEY, table_cell_item_t::right);
	// 9. Built date
	new_component<value_cell_t>((sint64)sb->get_purchase_time(), gui_chart_t::DATE, table_cell_item_t::right);

	// init cells height
	for (auto& cell : owned_cells) {
		cell->set_height(row_height);
	}
}

void signalboxlist_row_t::draw(scr_coord offset)
{
	if (old_connected != sb->get_number_of_signals_controlled_from_this_box()) {
		old_connected = sb->get_number_of_signals_controlled_from_this_box();
		value_cell_t* cell = dynamic_cast<value_cell_t*>(owned_cells[SB_CONNECTED]);
		cell->set_value((sint64)old_connected);
		cell->set_color( old_connected==sb->get_capacity() ? COL_DANGER : COL_SAFETY);
	}
	gui_sort_table_row_t::draw(offset);
}

bool signalboxlist_row_t::infowin_event(const event_t* ev)
{
	bool swallowed = gui_scrolled_list_t::scrollitem_t::infowin_event(ev);
	if (!swallowed && sb) {
		if (IS_RIGHTRELEASE(ev)) {
			for (auto& cell : owned_cells) {
				if (cell->get_type() == table_cell_item_t::cell_coord && cell->getroffen(ev->mouse_pos)) {
					const coord_cell_t* coord_cell = dynamic_cast<const coord_cell_t*>(cell);
					const koord k = coord_cell->get_coord();
					if (k != koord::invalid) {
						world()->get_viewport()->change_world_position(k);
						return true;
					}
					return false;
				}
			}
		}
	}
	return swallowed;
}

bool signalboxlist_row_t::compare(const gui_component_t* aa, const gui_component_t* bb)
{
	const signalboxlist_row_t* row_a = dynamic_cast<const signalboxlist_row_t*>(aa);
	const signalboxlist_row_t* row_b = dynamic_cast<const signalboxlist_row_t*>(bb);
	if (row_a == NULL || row_b == NULL) {
		dbg->warning("signalboxlist_row_t::compare()", "row data error");
		return false;
	}

	const table_cell_item_t* a = row_a->get_element(sort_mode);
	const table_cell_item_t* b = row_b->get_element(sort_mode);
	if (a == NULL || b == NULL) {
		dbg->warning("depotlist_row_t::compare()", "Could not get table_cell_item_t successfully");
		return false;
	}
	int cmp = gui_sort_table_row_t::compare(a, b);
	return sortreverse ? cmp < 0 : cmp > 0; // Do not include 0
}



signalboxlist_frame_t::signalboxlist_frame_t(player_t *player) :
	gui_frame_t( translator::translate("sb_title"), player ),
	scrolly(gui_scrolled_list_t::windowskin, signalboxlist_row_t::compare),
	scroll_sortable(&cont_sortable, true, true)
{
	this->player = player;
	last_signalbox_count = 0;

	scrolly.add_listener(this);
	scrolly.set_show_scroll_x(false);
	scrolly.set_checkered(true);
	scroll_sortable.set_maximize(true);
	scrolly.set_scroll_amount_y(LINESPACE + D_H_SPACE + 2); // default cell height

	// init table sort buttons
	table_header.add_listener(this);
	for (uint8 col = 0; col < signalboxlist_row_t::MAX_COLS; col++) {
		table_header.new_component<sortable_header_cell_t>(sb_header_text[col]);
	}
	cont_sortable.set_margin(NO_SPACING, NO_SPACING);
	cont_sortable.set_spacing(NO_SPACING);
	cont_sortable.set_table_layout(1, 2);
	cont_sortable.set_alignment(ALIGN_TOP); // Without this, the layout will be broken if the height is small.
	cont_sortable.add_component(&table_header);
	cont_sortable.add_component(&scrolly);

	set_table_layout(1,0);

	add_table(3,1);
	{
		new_component<gui_label_t>("Filter:");

		filter_vacant_slot.init(button_t::square_state, "Vacant slot");
		filter_vacant_slot.set_tooltip("helptxt_filter_sb_has_vacant_slot");
		filter_vacant_slot.add_listener(this);
		filter_vacant_slot.pressed = filter_has_vacant_slot;
		add_component(&filter_vacant_slot);

		new_component<gui_margin_t>(LINESPACE*10);
	}
	end_table();

	add_component(&scroll_sortable);
	set_min_windowsize(scr_size(LINESPACE * 24, LINESPACE * 12));
	fill_list();

	set_resizemode(diagonal_resize);
}


/**
 * This method is called if an action is triggered
 */
bool signalboxlist_frame_t::action_triggered( gui_action_creator_t *comp,value_t v)
{
	if (comp == &filter_vacant_slot) {
		filter_has_vacant_slot = !filter_has_vacant_slot;
		filter_vacant_slot.pressed = filter_has_vacant_slot;
		fill_list();
	}
	else if (comp == &table_header) {
		signalboxlist_row_t::sort_mode = v.i;
		table_header.set_selection(signalboxlist_row_t::sort_mode);
		signalboxlist_row_t::sortreverse = table_header.is_reversed();
		scrolly.sort(0);
	}
	else if (comp == &scrolly) {
		scrolly.get_selection();
		signalboxlist_row_t* row = (signalboxlist_row_t*)scrolly.get_element(v.i);
		row->show_info();
	}
	return true;
}


void signalboxlist_frame_t::fill_list()
{
	scrolly.clear_elements();
	for(signalbox_t* const sigb : signalbox_t::all_signalboxes) {
		if (filter_has_vacant_slot && !sigb->can_add_more_signals()) {
			continue;
		}

		if(sigb->get_owner() == player && sigb->get_first_tile() == sigb ) {
			scrolly.new_component<signalboxlist_row_t>( sigb );
		}
	}
	scrolly.sort(0);
	scrolly.set_size( scrolly.get_size());

	// recalc stats table width
	scr_coord_val max_widths[signalboxlist_row_t::MAX_COLS] = {};
	// check column widths
	table_header.set_selection(signalboxlist_row_t::sort_mode);
	table_header.set_width(D_H_SPACE); // init width
	for (uint c = 0; c < signalboxlist_row_t::MAX_COLS; c++) {
		// get header widths
		max_widths[c] = table_header.get_min_width(c);
	}
	for (int r = 0; r < scrolly.get_count(); r++) {
		signalboxlist_row_t* row = (signalboxlist_row_t*)scrolly.get_element(r);
		for (uint c = 0; c < signalboxlist_row_t::MAX_COLS; c++) {
			max_widths[c] = max(max_widths[c], row->get_min_width(c));
		}
	}
	// set column widths
	for (uint c = 0; c < signalboxlist_row_t::MAX_COLS; c++) {
		table_header.set_col(c, max_widths[c]);
	}
	table_header.set_width(table_header.get_size().w + D_SCROLLBAR_WIDTH);
	for (int r = 0; r < scrolly.get_count(); r++) {
		signalboxlist_row_t* row = (signalboxlist_row_t*)scrolly.get_element(r);
		row->set_size(scr_size(0, row->get_size().h));
		for (uint c = 0; c < signalboxlist_row_t::MAX_COLS; c++) {
			row->set_col(c, max_widths[c]);
		}
	}
	//cont_sortable.set_size(cont_sortable.get_min_size());

	last_signalbox_count = signalbox_t::all_signalboxes.get_count();
	resize(scr_size(0,0));
}


void signalboxlist_frame_t::draw(scr_coord pos, scr_size size)
{
	if(  signalbox_t::all_signalboxes.get_count() != last_signalbox_count  ) {
		fill_list();
	}

	gui_frame_t::draw(pos,size);
}

