/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_SAVEGAME_FRAME_H
#define GUI_SAVEGAME_FRAME_H


#include <string.h>
#include <cstring>
#include <sys/stat.h>

#include "gui_frame.h"
#include "../tpl/slist_tpl.h"
#include "../tpl/vector_tpl.h"
#include "components/action_listener.h"
#include "components/gui_container.h"
#include "components/gui_scrollpane.h"
#include "components/gui_textinput.h"
#include "components/gui_divider.h"
#include "components/gui_label.h"
#include "components/gui_button.h"

#include <string>

using std::string;

class loadfont_frame_t;


/**
 * Base class from which all GUI dialogs to load/save generics can inherit from
 * @note When I refer to a "qualified" path I mean it can contain sub-directories or even fully qualified path. i.e. : "save/a.sve" or "c:\simutrans\scenario\file.nut"
 */
class savegame_frame_t : public gui_frame_t, public action_listener_t
{
friend class loadfont_frame_t;

private:
	vector_tpl<std::string> paths;     //@< Paths in which this dialog will search for

	const char *suffix;                //@< Extension of the files this dialog will use, can be NULL Can include or not the "." at start, will work on both cases
	char        ibuf[PATH_MAX];        //@< Input buffer for the text input component
	char        searchpath[PATH_MAX];  //@< Default search path
	bool        in_action;             //@< To avoid double mouse action
	bool        only_directories;      //@< Search for directories (used in pak_selector)
	bool        searchpath_defined;    //@< Is default path defined?
	bool        is_back_to_menu;       //@< Open the banner menu on cancel

	void add_section(std::string &name);

protected:

	/**
	 * Entries in list can be actual file entries or
	 * headers, that have a different look.
	 */
	enum dirlist_item_t {
		LI_HEADER, //@< This is a header list item.
		LI_ENTRY   //@< This is a data list item.
	};


	/**
	 * A list item.
	 * Max Kielland: Shouldn't this be an ADT and then have
	 * each derivate to define their own item class? This would also
	 * take care of differences, sorting and freeing resources.
	 */
	struct dir_entry_t
	{
		dir_entry_t(button_t* button_, button_t* del_, gui_label_t* label_, dirlist_item_t type_ = LI_ENTRY, const char *info_=NULL) :
			del(del_),
			button(button_),
			label(label_),
			type(type_),
			info(info_)
		{}

		button_t       *del;    //@< Delete button placed in the first column.
		button_t       *button; //@< Action button placed in the second column.
		gui_label_t    *label;  //@< Label placed in the third column.
		dirlist_item_t  type;   //@< Item type, data or header.
		const char     *info;   //@< A qualified path (might be relative) to the file, not just the name
		static int compare (const dir_entry_t & l, const dir_entry_t &r) {
			if (( NULL == l.info ) && ( NULL != r.info )) {
				return -1;
			}
			if (( NULL == r.info ) && ( NULL != l.info )) {
				return 1;
			}
			if (( NULL != l.info ) && ( NULL != r.info )) {
				return strcmp ( l.info, r.info );
			}
			return 0;
		}
	};

	// Standard GUI controls in dialogue
	gui_textinput_t  input;         //@< Filename input field
	button_t         savebutton;    //@< Save button
	button_t         cancelbutton;  //@< Cancel button
	gui_label_t      fnlabel;       //@< Static file name label
	gui_aligned_container_t
	                 top_frame,     //@< Contains input field
					 bottom_left_frame, //@< container for elements on the left of the last row
	                 button_frame;  //@< Gui container for all items
	gui_scrollpane_t scrolly;       //@< Scroll panel for the GUI container

	slist_tpl<dir_entry_t> entries;  //@< Internal list representing the file listing

	uint32           num_sections;   //@< Internal counter representing the number of sections added to the list
	bool             delete_enabled; //@< Show the first column of delete buttons.
	bool             label_enabled;  //@< Show the third column of labels.

	bool file_table_button_pressed;

	/**
	 * Called on each entry that passed the check
	 */
	void add_file(const char *path, const char *filename, const char *pak, const bool no_cutting_suffix);
	virtual void add_file(const char *fullpath, const char *filename, const bool not_cutting_suffix);
	void        add_path     ( const char *path );
	void        set_filename ( const char *fn );
	void        set_extension( const char *ext ) { suffix = ext; }
	void        cleanup_path ( char *path );
	void        shorten_path ( char *dest, const char *orig, const size_t max_size );
	std::string get_filename(const char *fullpath, const bool with_extension = true) const;
	void        list_filled(void);

	// compare item to another with info and filename
	virtual bool compare_items ( const dir_entry_t & entry, const char *info, const char *filename );

	 // Virtual callback function that will be executed when the user clicks ok,
	virtual bool cancel_action ( const char * /*fullpath*/ ) { return true; } // Callback for cancel button click
	virtual bool del_action    ( const char *   fullpath   );                 // Callback for delete button click
	virtual bool ok_action     ( const char * /*fullpath*/ ) { return true; } // Callback for ok button click

	virtual bool check_file         ( const char *filename, const char *suffix );

	// Pure virtual functions
	virtual const char *get_info    ( const char *fname    ) = 0;
	virtual bool        item_action ( const char *fullpath ) = 0;


	/**
	 * called by fill_list():
	 */
	virtual void set_file_table_default_sort_order();

public:

	/**
	 * extracts base name from a full path
	 */
	std::string get_basename(const char *fullpath);

	savegame_frame_t(const char *suffix, bool only_directorie, const char *path, const bool delete_enabled, const bool back_to_menu = false  );
//	savegame_frame_t(const char *suffix = NULL, bool only_directories = false, const char *path = NULL, const bool delete_enabled = true);
//	savegame_frame_t(const char *suffix = NULL, bool only_directories = false, const char *path = NULL, const bool delete_enabled = true, bool use_table = false );
	virtual ~savegame_frame_t();

	bool action_triggered  ( gui_action_creator_t*, value_t ) OVERRIDE;
	bool infowin_event     ( event_t const* ) OVERRIDE;

	virtual void fill_list ( void );
};

#endif
