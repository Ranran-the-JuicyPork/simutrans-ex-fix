/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "sortable_table_halt.h"
#include "gui_colorbox.h"

#include "../gui_theme.h"

#include "../../simworld.h"
#include "../../simcolor.h"

#include "../../dataobj/environment.h"
#include "../../display/simgraph.h"
#include "../../display/viewport.h"
#include "../../player/simplay.h"

#include "../../utils/simstring.h"
#include "../../utils/cbuffer_t.h"


halt_name_cell_t::halt_name_cell_t(halthandle_t halt_)
	: coord_cell_t(halt_.is_bound() ? halt_->get_basis_pos() : koord::invalid)
{
	halt=halt_;
	buf.clear();
	buf.printf("%s", halt.is_bound() ? halt->get_name() : "-");
	min_size = scr_size(min(proportional_string_width(buf)+ LINESPACE+D_H_SPACE, LINESPACE*16), LINESPACE);
	set_size(min_size);
}

void halt_name_cell_t::draw(scr_coord offset)
{
	table_cell_item_t::draw(offset);

	if (halt.is_bound()) {
		offset += pos;
		// haltbox
		const bool is_one_center = world()->get_viewport()->is_on_center(halt->get_basis_pos3d());
		const bool is_interchange = (halt->registered_lines.get_count() + halt->registered_convoys.get_count()) > 1;
		if (is_one_center) {
			display_filled_roundbox_clip(offset.x + draw_offset.x-1, offset.y + draw_offset.y + LINESPACE*0.1, LINESPACE+2, LINESPACE * 0.8+2, color_idx_to_rgb(COL_YELLOW), false);
		}
		if (is_interchange) {
			display_filled_roundbox_clip(offset.x + draw_offset.x, offset.y + draw_offset.y + LINESPACE*0.1+1, LINESPACE, LINESPACE*0.8, color_idx_to_rgb(halt->get_owner()->get_player_color1() + env_t::gui_player_color_dark), false);
		}
		display_filled_roundbox_clip(offset.x + draw_offset.x+2, offset.y + draw_offset.y + LINESPACE*0.1+3, LINESPACE -4, LINESPACE*0.8-4, is_interchange ? (is_one_center ? color_idx_to_rgb(COL_YELLOW) : color_idx_to_rgb(COL_WHITE)) : color_idx_to_rgb(halt->get_owner()->get_player_color1() + env_t::gui_player_color_dark), false);
		const scr_rect area(scr_coord(offset.x + draw_offset.x+ LINESPACE + D_H_SPACE, offset.y + draw_offset.y ), size - scr_size(LINESPACE + D_H_SPACE, 0));
		display_proportional_ellipsis_rgb(area, buf, ALIGN_LEFT | DT_CLIP, SYSCOL_TEXT, false, false);
	}
}
