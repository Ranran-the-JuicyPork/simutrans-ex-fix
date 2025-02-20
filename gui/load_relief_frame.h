/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_LOAD_RELIEF_FRAME_H
#define GUI_LOAD_RELIEF_FRAME_H


#include "components/gui_combobox.h"
#include "savegame_frame.h"


class settings_t;


class load_relief_frame_t : public savegame_frame_t
{
private:
	settings_t* sets;

	gui_label_t load_mode_label;
	gui_combobox_t load_mode;

protected:
	bool item_action(const char *fullpath) OVERRIDE;
	const char *get_info(const char *fullpath) OVERRIDE;
	bool check_file(const char *fullpath, const char *suffix) OVERRIDE;

public:
	/**
	 * Set the window associated helptext
	 * @return the filename for the helptext, or NULL
	 */
	const char *get_help_filename() const OVERRIDE { return "load_relief.txt"; }

	load_relief_frame_t(settings_t*);

	bool action_triggered(gui_action_creator_t *comp, value_t extra) OVERRIDE;
};

#endif
