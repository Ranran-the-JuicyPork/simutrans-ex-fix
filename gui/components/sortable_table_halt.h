/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_COMPONENTS_SORTABLE_TABLE_HALT_H
#define GUI_COMPONENTS_SORTABLE_TABLE_HALT_H


#include "sortable_table.h"

#include "../../simhalt.h"
#include "../../halthandle_t.h"

#include "gui_aligned_container.h"
#include "../../display/scr_coord.h"

#include "gui_container.h"


class halt_name_cell_t : public coord_cell_t
{
	halthandle_t halt;

public:
	halt_name_cell_t(halthandle_t halt);

	koord get_coord() const { return halt->get_basis_pos(); }

	void draw(scr_coord offset) OVERRIDE;
};

#endif
