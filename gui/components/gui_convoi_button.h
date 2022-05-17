/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_COMPONENTS_GUI_CONVOI_BUTTON_H
#define GUI_COMPONENTS_GUI_CONVOI_BUTTON_H


#include "gui_button.h"
#include "action_listener.h"

#include "../../simconvoi.h"

/**
 * Button to open convoi window
 */
class gui_convoi_button_t : public button_t, public action_listener_t
{
	convoihandle_t convoi;

public:
gui_convoi_button_t(convoihandle_t convoi) : button_t() {
		this->convoi = convoi;
		init(button_t::posbutton, NULL);
		add_listener(this);
	}
		
	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE {
		convoi->show_info();
		return true;
	}
};

#endif
