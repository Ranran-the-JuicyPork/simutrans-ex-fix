/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project and may not be used
 * in other projects without written permission of the author.
 */

/*
 * Hauptklasse fuer Simutrans, Datenstruktur die alles Zusammenhaelt
 * Hansjoerg Malthaner, 1997
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef simdebug_h
#include "simdebug.h"
#endif

#ifndef tpl_vector_h
#include "tpl/vector_tpl.h"
#endif


#include "boden/boden.h"
#include "boden/wasser.h"
#include "simplay.h"
#include "simfab.h"
#include "simconvoi.h"
#include "simcity.h"
#include "simskin.h"
#include "simwin.h"
#include "simhalt.h"
#include "simdepot.h"
#include "simversion.h"
#include "simmesg.h"
#include "simmenu.h"
#include "simcolor.h"
#include "simlinemgmt.h"

#include "simintr.h"
#include "simio.h"

#include "simimg.h"
#include "old_blockmanager.h"
#include "vehicle/simvehikel.h"
#include "vehicle/simverkehr.h"
#include "simworld.h"
#include "simview.h"

#include "simwerkz.h"
#include "simtools.h"
#include "simsound.h"

#include "simgraph.h"
#include "simdisplay.h"
#include "simsys.h"

#include "boden/wege/schiene.h"

#include "dings/zeiger.h"
#include "dings/baum.h"
#include "dings/signal.h"
#include "dings/roadsign.h"
#include "dings/wayobj.h"
#include "dings/groundobj.h"
#include "dings/gebaeude.h"

#include "gui/welt.h"
#include "gui/karte.h"
#include "gui/optionen.h"
#include "gui/map_frame.h"
#include "gui/optionen.h"
#include "gui/player_frame_t.h"
#include "gui/messagebox.h"
#include "gui/loadsave_frame.h"
#include "gui/money_frame.h"
#include "gui/schedule_list.h"
#include "gui/convoi_frame.h"
#include "gui/halt_list_frame.h"        //30-Dec-01     Markus Weber    Added
#include "gui/citylist_frame_t.h"
#include "gui/message_frame_t.h"
#include "gui/help_frame.h"
#include "gui/goods_frame_t.h"
#include "gui/jump_frame.h"
#include "gui/werkzeug_parameter_waehler.h"

#include "dataobj/translator.h"
#include "dataobj/loadsave.h"
#include "dataobj/einstellungen.h"
#include "dataobj/umgebung.h"
#include "dataobj/tabfile.h"
#include "dataobj/powernet.h"

#include "utils/simstring.h"

#include "bauer/brueckenbauer.h"
#include "bauer/tunnelbauer.h"
#include "bauer/fabrikbauer.h"
#include "bauer/wegbauer.h"
#include "bauer/hausbauer.h"
#include "bauer/vehikelbauer.h"

#include "besch/grund_besch.h"
#include "besch/sound_besch.h"

#include "ifc/sync_steppable.h"


//#define DEMO
//#undef DEMO

// advance 201 ms per sync_step in fast forward mode
#define MAGIC_STEP (201)

// frame per second for fast forward
#define FF_PPS (10)

// offsets for mouse pointer
const int karte_t::Z_PLAN      = 4;
const int karte_t::Z_GRID      = 0;


static bool is_dragging = false;



// changes the snowline height (for the seasons)
bool
karte_t::recalc_snowline()
{
	static int mfactor[12] = { 99, 95, 80, 50, 25, 10, 0, 5, 20, 35, 65, 85 };
	static uint8 month_to_season[12] = { 2, 2, 2, 3, 3, 0, 0, 0, 0, 1, 1, 2 };

	// calculate snowline with day precicion
	// use linear interpolation
	const long ticks_this_month = gib_zeit_ms() & (karte_t::ticks_per_tag-1);
	const long faktor = mfactor[letzter_monat] + (  ( (mfactor[(letzter_monat+1)%12]-mfactor[letzter_monat])*(ticks_this_month>>12) ) >> (karte_t::ticks_bits_per_tag-12) );

	// just remember them
	const sint16 old_snowline = snowline;
	const sint16 old_season = season;

	// and calculate new values
	season=month_to_season[letzter_monat];   //  (2+letzter_monat/3)&3; // summer always zero
	const int winterline = einstellungen->gib_winter_snowline();
	const int summerline = einstellungen->gib_climate_borders()[arctic_climate]+1;
	snowline = summerline - (sint16)(((summerline-winterline)*faktor)/100);
	snowline = (snowline*Z_TILE_STEP) + grundwasser;

	// changed => we update all tiles ...
	return (old_snowline!=snowline  ||  old_season!=season);
}



void
karte_t::setze_scroll_multi(int n)
{
    einstellungen->setze_scroll_multi(n);
}



/**
 * Read a heightfield from file
 * @param filename name of heightfield file
 * @author Hj. Malthaner
 */
void
karte_t::calc_hoehe_mit_heightfield(const cstring_t & filename)
{
	display_set_progress_text(translator::translate("Init map ..."));
	FILE *file = fopen(filename, "rb");
	if(file) {
		const int display_total = 16 + gib_einstellungen()->gib_anzahl_staedte()*4 + gib_einstellungen()->gib_land_industry_chains() + gib_einstellungen()->gib_city_industry_chains();
		char buf [256];
		int param[3], index=0;
		char *c=buf+2;

		// parsing the header of this mixed file format is nottrivial ...
		fread(buf, 1, 3, file);
		buf[2] = 0;
		if(strncmp(buf, "P6", 2)) {
			dbg->fatal("karte_t::load_heightfield()","Heightfield has wrong image type %s instead P6", buf);
		}

		while(index<3) {
			// the format is "P6[whitespace]width[whitespace]height[[whitespace bitdepth]]newline]
			// however, Photoshop is the first program, that uses space for the first whitespace ...
			// so we cater for Photoshop too
			while(*c  &&  *c<=32) {
				c++;
			}
			// usually, after P6 there comes a comment with the maker
			// but comments can be anywhere
			if(*c==0) {
				read_line(buf, 255, file);
				c = buf;
				continue;
			}
			param[index++] = atoi(c);
			while(*c>='0'  &&  *c<='9') {
				c++;
			}
		}

		if(param[0] != gib_groesse_x()  || param[1] != gib_groesse_y()) {
			dbg->fatal("karte_t::load_heightfield()","Heightfield has wrong size (%d,%d)", param[0], param[1] );
		}

		if(param[2]!=255) {
			dbg->fatal("karte_t::load_heightfield()","Heightfield has wrong color depth %d", param[2] );
		}

		// after the header only binary data will follow

		int y;
		for(y=0; y<=gib_groesse_y(); y++) {
			for(int x=0; x<=gib_groesse_x(); x++) {
				setze_grid_hgt(koord(x,y), grundwasser);
			}
		}

		for(y=0; y<gib_groesse_y(); y++) {
			for(int x=0; x<gib_groesse_x(); x++) {
				int R = fgetc(file);
				int G = fgetc(file);
				int B = fgetc(file);

				setze_grid_hgt( koord(x,y), ((((R*2+G*3+B)/4 - 224) & 0xFFF0)/16)*Z_TILE_STEP );
			}

			if(is_display_init()) {
				display_progress((y*16)/gib_groesse_y(), display_total);
				display_flush(IMG_LEER, 0, "", "", 0, 0);
			}
		}

		fclose(file);
	}
	else {
		dbg->fatal("karte_t::load_heightfield()","cannot open %s", (const char *)filename );
	}
}



/**
 * Hoehe eines Punktes der Karte mit "perlin noise"
 *
 * @param frequency in 0..1.0 roughness, the higher the rougher
 * @param amplitude in 0..160.0 top height of mountains, may not exceed 160.0!!!
 * @author Hj. Malthaner
 */
int
karte_t::perlin_hoehe(const int x, const int y,
                      const double frequency, const double amplitude)
{
//    double perlin_noise_2D(double x, double y, double persistence);
//    return ((int)(perlin_noise_2D(x, y, 0.6)*160.0)) & 0xFFFFFFF0;
    return ((int)(perlin_noise_2D(x, y, frequency)*amplitude)) & 0xFFFFFFF0;
}



void
karte_t::calc_hoehe_mit_perlin()
{
	display_set_progress_text(translator::translate("Init map ..."));
	const int display_total = 16 + gib_einstellungen()->gib_anzahl_staedte()*4 + gib_einstellungen()->gib_land_industry_chains() + gib_einstellungen()->gib_city_industry_chains();
	for(int y=0; y<=gib_groesse_y(); y++) {

		for(int x=0; x<=gib_groesse_x(); x++) {
			// Hajo: to Markus: replace the fixed values with your
			// settings. Amplitude is the top highness of the
			// montains, frequency is something like landscape 'roughness'
			// amplitude may not be greater than 160.0 !!!
			// please don't allow frequencies higher than 0.8 it'll
			// break the AI's pathfinding. Frequency values of 0.5 .. 0.7
			// seem to be ok, less is boring flat, more is too crumbled
			// the old defaults are given here: f=0.6, a=160.0
			const int h=perlin_hoehe(x,y,einstellungen->gib_map_roughness(),einstellungen->gib_max_mountain_height() );
			setze_grid_hgt(koord(x,y), (h/16)*Z_TILE_STEP );
			//	  DBG_MESSAGE("karte_t::calc_hoehe_mit_perlin()","%i",h);
		}

		if(is_display_init()) {
			display_progress((y*16)/gib_groesse_y(), display_total);
			display_flush(IMG_LEER, 0, "", "", 0, 0);
		}
		else {
			printf("X");fflush(NULL);
		}
	}
	print(" - ok\n");fflush(NULL);
}


void karte_t::raise_clean(sint16 x, sint16 y, sint16 h)
{
  if(ist_in_gittergrenzen(x, y)) {
    const koord k (x,y);

    if(lookup_hgt(k) < h) {
      setze_grid_hgt(k, h);

#ifndef DOUBLE_GROUNDS
      raise_clean(x-1, y-1, h-Z_TILE_STEP);
      raise_clean(x  , y-1, h-Z_TILE_STEP);
      raise_clean(x+1, y-1, h-Z_TILE_STEP);
      raise_clean(x-1, y  , h-Z_TILE_STEP);
      // Punkt selbst hat schon die neue Hoehe
      raise_clean(x+1, y  , h-Z_TILE_STEP);
      raise_clean(x-1, y+1, h-Z_TILE_STEP);
      raise_clean(x  , y+1, h-Z_TILE_STEP);
      raise_clean(x+1, y+1, h-Z_TILE_STEP);
#else
      raise_clean(x-1, y-1, h-Z_TILE_STEP*2);
      raise_clean(x  , y-1, h-Z_TILE_STEP*2);
      raise_clean(x+1, y-1, h-Z_TILE_STEP*2);
      raise_clean(x-1, y  , h-Z_TILE_STEP*2);
      // Punkt selbst hat schon die neue Hoehe
      raise_clean(x+1, y  , h-Z_TILE_STEP*2);
      raise_clean(x-1, y+1, h-Z_TILE_STEP*2);
      raise_clean(x  , y+1, h-Z_TILE_STEP*2);
      raise_clean(x+1, y+1, h-Z_TILE_STEP*2);
#endif

    }
  }
}


void karte_t::cleanup_karte()
{
	// we need a copy to smoothen the map to a realistic level
	sint8 *grid_hgts_cpy = new sint8[(gib_groesse_x()+1)*(gib_groesse_y()+1)];
	memcpy(grid_hgts_cpy,grid_hgts,(gib_groesse_x()+1)*(gib_groesse_y()+1));
	// now connect the heights
	sint16 i,j;
	for(j=0; j<=gib_groesse_y(); j++) {
		for(i=0; i<=gib_groesse_x(); i++) {
			raise_clean(i,j, grid_hgts_cpy[j*(gib_groesse_x()+1)+i]*Z_TILE_STEP+Z_TILE_STEP );
		}
	}
	delete [] grid_hgts_cpy;

	// now lower the corners to ground level
	for(i=0; i<gib_groesse_x(); i++) {
		lower_to(i, 0, grundwasser,false);
		lower_to(i, gib_groesse_y(), grundwasser,false);
	}
	for(i=0; i<=gib_groesse_y(); i++) {
		lower_to(0, i, grundwasser,false);
		lower_to(gib_groesse_x(), i, grundwasser,false);
	}
	for(i=0; i<=gib_groesse_x(); i++) {
		raise_to(i, 0, grundwasser,false);
		raise_to(i, gib_groesse_y(), grundwasser,false);
	}
	for(i=0; i<=gib_groesse_y(); i++) {
		raise_to(0, i, grundwasser,false);
		raise_to(gib_groesse_x(), i, grundwasser,false);
	}
}



// karte_t methoden

void
karte_t::destroy()
{
DBG_MESSAGE("karte_t::destroy()", "destroying world");

	// rotate the map until it can be saved
	for( int i=0;  i<4  &&  nosave;  i++  ) {
DBG_MESSAGE("karte_t::destroy()", "rotating");
		nosave = false;
		rotate90();
	}
	assert( !nosave );

DBG_MESSAGE("karte_t::destroy()", "label clear");
	labels.clear();

	if(zeiger) {
		zeiger->setze_pos(koord3d::invalid);
		delete zeiger;
		zeiger = NULL;
	}

	// alle convois aufr�umen
	while (!convoi_array.empty()) {
		convoihandle_t cnv = convoi_array.back();
		cnv->destroy();
	}
	convoi_array.clear();
DBG_MESSAGE("karte_t::destroy()", "convois destroyed");

	// alle haltestellen aufr�umen
	haltestelle_t::destroy_all(this);
DBG_MESSAGE("karte_t::destroy()", "stops destroyed");

	// entfernt alle synchronen objekte aus der liste
	sync_list.clear();
DBG_MESSAGE("karte_t::destroy()", "sync list cleared");

	// delete towns first (will also delete all their houses)
	// for the next game we need to remember the desired number ...
	sint32 no_of_cities=einstellungen->gib_anzahl_staedte();
	while (!stadt.empty()) {
		rem_stadt(stadt.front());
	}
	einstellungen->setze_anzahl_staedte(no_of_cities);
DBG_MESSAGE("karte_t::destroy()", "towns destroyed");

	// dinge aufr�umen
	cached_groesse_gitter_x = cached_groesse_gitter_y = 1;
	cached_groesse_karte_x = cached_groesse_karte_y = 0;
	if(plan) {
		delete [] plan;
		plan = NULL;
	}
	DBG_MESSAGE("karte_t::destroy()", "planquadrat destroyed");

	// gitter aufr�umen
	if(grid_hgts) {
		delete [] grid_hgts;
		grid_hgts = NULL;
	}

	// marker aufr�umen
	marker.init(0,0);
DBG_MESSAGE("karte_t::destroy()", "marker destroyed");

	// spieler aufr�umen
	for(int i=0; i<MAX_PLAYER_COUNT; i++) {
		if(spieler[i]) {
			delete spieler[i];
			spieler[i] = NULL;
		}
	}
DBG_MESSAGE("karte_t::destroy()", "player destroyed");

	// clear all ids
	simlinemgmt_t::init_line_ids();
DBG_MESSAGE("karte_t::destroy()", "lines destroyed");

	// alle fabriken aufr�umen
	slist_iterator_tpl<fabrik_t*> fab_iter(fab_list);
	while(fab_iter.next()) {
		delete fab_iter.get_current();
	}
	fab_list.clear();
DBG_MESSAGE("karte_t::destroy()", "factories destroyed");

	// hier nur entfernen, aber nicht l�schen
	ausflugsziele.clear();
DBG_MESSAGE("karte_t::destroy()", "attraction list destroyed");

assert( depot_t::get_depot_list().empty() );

DBG_MESSAGE("karte_t::destroy()", "world destroyed");
	printf("World destroyed.\n");
}



/**
 * Zugriff auf das St�dte Array.
 * @author Hj. Malthaner
 */
const stadt_t *karte_t::get_random_stadt() const
{
	return stadt.at_weight(simrand(stadt.get_sum_weight()));
}

void karte_t::add_stadt(stadt_t *s)
{
	einstellungen->setze_anzahl_staedte(einstellungen->gib_anzahl_staedte()+1);
	stadt.append(s, s->gib_einwohner(), 64);
}

/**
 * Removes town from map, houses will be left overs
 * @author prissi
 */
bool karte_t::rem_stadt(stadt_t *s)
{
	if(s == NULL  ||  stadt.empty()) {
		// no town there to delete ...
		return false;
	}

	// reduce number of towns
	if(s->gib_name()) { DBG_MESSAGE("karte_t::rem_stadt()", s->gib_name() ); }
	stadt.remove(s);
	DBG_MESSAGE("karte_t::rem_stadt()", "reduce city to %i", einstellungen->gib_anzahl_staedte()-1 );
	einstellungen->setze_anzahl_staedte(einstellungen->gib_anzahl_staedte()-1);

	// remove all links from factories
	DBG_MESSAGE("karte_t::rem_stadt()", "fab_list %i", fab_list.count() );
	slist_iterator_tpl<fabrik_t *> iter(fab_list);
	while(iter.next()) {
		(iter.get_current())->remove_arbeiterziel(s);
	}

	// ok, we can delete this
	DBG_MESSAGE("karte_t::rem_stadt()", "delete" );
	delete s;

	return true;
}



// just allocates space;
void
karte_t::init_felder()
{
	assert(plan==0);

	plan   = new planquadrat_t[gib_groesse_x()*gib_groesse_y()];
	grid_hgts = new sint8[(gib_groesse_x()+1)*(gib_groesse_y()+1)];

	memset(grid_hgts, 0, sizeof(sint8)*(gib_groesse_x()+1)*(gib_groesse_y()+1));

	marker.init(gib_groesse_x(),gib_groesse_y());

	simlinemgmt_t::init_line_ids();

	win_setze_welt( this );
	reliefkarte_t::gib_karte()->setze_welt(this);

	for(int i=0; i<MAX_PLAYER_COUNT ; i++) {
		spieler[i] = new spieler_t(this, i);
	}
	active_player = spieler[0];
	active_player_nr = 0;

	// defaults without timeline
	average_speed[0] = 60;
	average_speed[1] = 80;
	average_speed[2] = 40;
	average_speed[3] = 350;

	// clear world records
	max_road_speed.speed = 0;
	max_rail_speed.speed = 0;
	max_maglev_speed.speed = 0;
	max_ship_speed.speed = 0;
	max_air_speed.speed = 0;

	// make timer loop invalid
	for( int i=0;  i<32;  i++ ) {
		last_frame_ms[i] = 0x7FFFFFFFu;
	}
	last_frame_idx = 0;
	pending_season_change = 0;

	nosave = false;
}


void karte_t::init(einstellungen_t* sets)
{
	mute_sound(true);

	intr_disable();
	if(plan) {
		destroy();
	}

	current_mouse_funk.funk = NULL;
	if(is_display_init()) {
		display_show_pointer(false);
	}
	change_world_position( koord(0,0), 0, 0 );

	einstellungen = sets;

	x_off = y_off = 0;

	ticks = 0;
	last_step_ticks = 0;
	schedule_counter = 0;
	// ticks = 0x7FFFF800;  // Testing the 31->32 bit step

	letzter_monat = 0;
	letztes_jahr = einstellungen->gib_starting_year();//umgebung_t::starting_year;
	current_month = letzter_monat + (letztes_jahr*12);
	setze_ticks_bits_per_tag(einstellungen->gib_bits_per_month());
	next_month_ticks =  karte_t::ticks_per_tag;
	season=(2+letzter_monat/3)&3; // summer always zero
	snowline = sets->gib_winter_snowline()*Z_TILE_STEP + grundwasser;
	current_mouse_funk.funk = NULL;
	is_dragging = false;
	steps = 0;
	recalc_average_speed();	// resets timeline

	grundwasser = sets->gib_grundwasser();      //29-Nov-01     Markus Weber    Changed
	grund_besch_t::calc_water_level( this, height_to_climate );
	snowline = sets->gib_winter_snowline()*Z_TILE_STEP;

	if(sets->gib_beginner_mode()) {
		warenbauer_t::set_multiplier( umgebung_t::beginner_price_factor );
		sets->setze_just_in_time( 0 );
	}
	else {
		warenbauer_t::set_multiplier( 1000 );
	}

	// wird gecached, um den Pointerzugriff zu sparen, da
	// die groesse _sehr_ oft referenziert wird
	cached_groesse_gitter_x = einstellungen->gib_groesse_x();
	cached_groesse_gitter_y = einstellungen->gib_groesse_y();
	cached_groesse_max = max(cached_groesse_gitter_x,cached_groesse_gitter_y);
	cached_groesse_karte_x = cached_groesse_gitter_x-1;
	cached_groesse_karte_y = cached_groesse_gitter_y-1;

DBG_DEBUG("karte_t::init()","init_felder");
	init_felder();

DBG_DEBUG("karte_t::init()","setze_grid_hgt");
	for (int j = 0; j <= gib_groesse_y(); j++) {
		for(int i = 0; i <= gib_groesse_x(); i++) {
			setze_grid_hgt(koord(i, j), 0);
		}
	}

DBG_DEBUG("karte_t::init()","kartenboden_setzen");
	for(sint16 y=0; y<gib_groesse_y(); y++) {
		for(sint16 x=0; x<gib_groesse_x(); x++) {
			access(x,y)->kartenboden_setzen( new boden_t(this, koord3d(x,y, 0), 0 ) );
		}
	}

	max_rail_speed.speed = max_maglev_speed.speed = max_road_speed.speed = max_ship_speed.speed = max_air_speed.speed = 0;

	print("Creating landscape shape...\n");
	// calc_hoehe(0, 0, gib_groesse()-1, gib_groesse()-1);

DBG_DEBUG("karte_t::init()","calc_hoehe_mit_heightfield");
	setsimrand( 0xFFFFFFFF, einstellungen->gib_karte_nummer() );
	if(einstellungen->heightfield.len() > 0) {
		calc_hoehe_mit_heightfield(einstellungen->heightfield);
	}
	else {
		calc_hoehe_mit_perlin();
	}

DBG_DEBUG("karte_t::init()","cleanup karte");
	cleanup_karte();
	nosave = false;

	// right season for recalculations
	recalc_snowline();

DBG_DEBUG("karte_t::init()","set ground");
	// Hajo: init slopes
	koord k;
	for(k.y=0; k.y<gib_groesse_y(); k.y++) {
		for(k.x=0; k.x<gib_groesse_x(); k.x++) {
			access(k)->abgesenkt(this);
			grund_t *gr = lookup_kartenboden(k);
			gr->setze_grund_hang( calc_natural_slope(k) );
			gr->calc_bild();
		}
	}

DBG_DEBUG("karte_t::init()","distributing groundobjs");
	if(  umgebung_t::ground_object_probability > 0  ) {
		// add eyecandy like rocky, moles, flowers, ...
		koord k;
		sint32 queried = simrand(umgebung_t::ground_object_probability*2);
		for(k.y=0; k.y<gib_groesse_y(); k.y++) {
			for(k.x=0; k.x<gib_groesse_x(); k.x++) {
				grund_t *gr = lookup_kartenboden(k);
				if(gr->gib_typ()==grund_t::boden) {
					queried --;
					if(  queried<0  ) {
						const groundobj_besch_t *besch = groundobj_t::random_groundobj_for_climate( get_climate(gr->gib_hoehe()), gr->gib_grund_hang() );
						if(besch) {
							queried = simrand(umgebung_t::ground_object_probability*2);
							gr->obj_add( new groundobj_t( this, gr->gib_pos(), besch ) );
						}
					}
				}
			}
		}
	}

DBG_DEBUG("karte_t::init()","distributing trees");
	baum_t::distribute_trees(this,3);

DBG_DEBUG("karte_t::init()","built timeline");
	stadtauto_t::built_timeline_liste(this);

print("Creating cities ...\n");
DBG_DEBUG("karte_t::init()","hausbauer_t::neue_karte()");
	hausbauer_t::neue_karte();
	fabrikbauer_t::neue_karte(this);

DBG_DEBUG("karte_t::init()","prepare cities");
	stadt.clear();
	vector_tpl<koord> *pos = stadt_t::random_place(this, einstellungen->gib_anzahl_staedte());

	if (pos != NULL && !pos->empty()) {
		// prissi if we could not generate enough positions ...
		einstellungen->setze_anzahl_staedte( pos->get_count() );	// new number of towns (if we did not found enough positions) ...
		const int max_display_progress=16+einstellungen->gib_anzahl_staedte()*4+einstellungen->gib_land_industry_chains()+einstellungen->gib_city_industry_chains();

		// Ansicht auf erste Stadt zentrieren
		change_world_position( koord3d((*pos)[0], min_hgt((*pos)[0])) );

		for (int i = 0; i < einstellungen->gib_anzahl_staedte(); i++) {
//			int citizens=(int)(einstellungen->gib_mittlere_einwohnerzahl()*0.9);
//			citizens = citizens/10+simrand(2*citizens+1);

			int current_citicens = (2500l * einstellungen->gib_mittlere_einwohnerzahl()) /(simrand(20000)+100);

			stadt_t* s = new stadt_t(spieler[1], (*pos)[i], current_citicens);
DBG_DEBUG("karte_t::init()","Erzeuge stadt %i with %ld inhabitants",i,(s->get_city_history_month())[HIST_CITICENS] );
			stadt.append(s, current_citicens, 64);
		}

		int x = 16;
		for (weighted_vector_tpl<stadt_t*>::const_iterator i = stadt.begin(), end = stadt.end(); i != end; ++i) {
			// Hajo: do final init after world was loaded/created
			(*i)->laden_abschliessen();
			// the growth is slow, so update here the progress bar
			if(is_display_init()) {
				display_progress(x, max_display_progress);
				x += 2;
				display_flush(IMG_LEER, 0, "", "", 0, 0);
			}
			else {
				printf("*");fflush(NULL);
			}
		}

		// Hajo: connect some cities with roads
		const weg_besch_t* besch = wegbauer_t::gib_besch(*umgebung_t::intercity_road_type);
		if(besch == 0) {
			dbg->warning("karte_t::init()", "road type '%s' not found", (const char*)*umgebung_t::intercity_road_type);
			// Hajo: try some default (might happen with timeline ... )
			besch = wegbauer_t::weg_search(road_wt,80,get_timeline_year_month(),weg_t::type_flat);
		}

		// Hajo: No owner so that roads can be removed!
		wegbauer_t bauigel (this, 0);
		bauigel.route_fuer(wegbauer_t::strasse, besch);
		bauigel.set_keep_existing_ways(true);
		bauigel.set_maximum(umgebung_t::intercity_road_length);

		// Hajo: search for road offset
		koord roff (0,1);
		if (!lookup((*pos)[0] + roff)->gib_kartenboden()->hat_weg(road_wt)) {
			roff = koord(0,2);
		}

		int old_progress_count = 16+2*einstellungen->gib_anzahl_staedte();
		int count = 0;
		const int max_count=(einstellungen->gib_anzahl_staedte()*(einstellungen->gib_anzahl_staedte()-1))/2;

		for(int i = 0; i < einstellungen->gib_anzahl_staedte(); i++) {
			for (int j = i + 1; j < einstellungen->gib_anzahl_staedte(); j++) {
				const koord k1 = (*pos)[i] + roff;
				const koord k2 = (*pos)[j] + roff;
				const koord diff = k1-k2;
				const int d = diff.x*diff.x + diff.y*diff.y;

				if(d < umgebung_t::intercity_road_length) {
//DBG_DEBUG("karte_t::init()","built route fom city %d to %d", i, j);
					bauigel.calc_route(lookup(k1)->gib_kartenboden()->gib_pos(), lookup(k2)->gib_kartenboden()->gib_pos());
					if(bauigel.max_n >= 1) {
						bauigel.baue();
					}
					else {
//DBG_DEBUG("karte_t::init()","no route found fom city %d to %d", i, j);
					}
				}
				else {
//DBG_DEBUG("karte_t::init()","cites %d and %d are too far away", i, j);
				}
				count ++;
				// how much we continued?
				if(is_display_init()) {
					int progress_count = 16+einstellungen->gib_anzahl_staedte()*2+ (count*einstellungen->gib_anzahl_staedte()*2)/max_count;
					if(old_progress_count!=progress_count) {
						display_progress(progress_count, max_display_progress );
						display_flush(IMG_LEER, 0, "", "", 0, 0);
						old_progress_count = progress_count;
					}
				}
			}
		}

		delete pos;
	}
	else {
		// could not generate any town
		if(pos) {
			delete pos;
		}
		einstellungen->setze_anzahl_staedte( 0 );	// new number of towns (if we did not found enough positions) ...
	}

	fabrikbauer_t::verteile_industrie(this, einstellungen->gib_land_industry_chains(), false);
	fabrikbauer_t::verteile_industrie(this, einstellungen->gib_city_industry_chains(), true);
	// crossconnect all?
	if(umgebung_t::crossconnect_factories) {
		slist_iterator_tpl <fabrik_t *> iter (this->fab_list);
		while( iter.next() ) {
			iter.get_current()->add_all_suppliers();
		}
	}

	// tourist attractions
	fabrikbauer_t::verteile_tourist(this, einstellungen->gib_tourist_attractions());

	print("Preparing startup ...\n");
	if(zeiger == 0) {
		zeiger = new zeiger_t(this, koord3d::invalid, spieler[0]);
	}

	// finishes the line preparation and sets id 0 to invalid ...
	spieler[0]->simlinemgmt.laden_abschliessen();

	setze_maus_funktion(wkz_abfrage, skinverwaltung_t::fragezeiger->gib_bild_nr(0), Z_PLAN,  NO_SOUND, NO_SOUND );

	recalc_average_speed();

#ifndef DEMO
	for (int i = 0; i < 6; i++) {
		spieler[i + 2]->set_active( umgebung_t::automaten[i] );
	}
#endif

	active_player_nr = 0;
	active_player = spieler[0];

	setze_dirty();

	fast_forward = false;
	simloops = 60;
	reset_timer();

	if(is_display_init()) {
		display_show_pointer(true);
	}
	mute_sound(false);
}


karte_t::karte_t() : convoi_array(0), ausflugsziele(16), stadt(0), quick_shortcuts(15), marker(0,0)
{
	// length of day and other time stuff
	ticks_bits_per_tag = 20;
	ticks_per_tag = (1 << ticks_bits_per_tag);
	fast_forward = false;
	time_multiplier = 16;
	next_wait_time = this_wait_time = 30;
	current_mouse_funk.funk = NULL;

	for (unsigned int i=0; i<15; i++) {
//DBG_MESSAGE("karte_t::karte_t()","Append NULL to quick_shortcuts at %d\n",i);
		quick_shortcuts.append(NULL);
	}

	follow_convoi = convoihandle_t();
	setze_dirty();
	set_scroll_lock(false);

	einstellungen_t * sets = new einstellungen_t();

	if(umgebung_t::testlauf) {
		sets->setze_groesse(256,384);
		sets->setze_anzahl_staedte(16);
		sets->setze_land_industry_chains(8);
		sets->setze_city_industry_chains(4);
		sets->setze_tourist_attractions(8);
		sets->setze_verkehr_level(7);
		sets->setze_karte_nummer( 33 );
		sets->setze_station_coverage( umgebung_t::station_coverage_size );
	}
	else {
		sets->setze_groesse(64,64);
		sets->setze_anzahl_staedte(1);
		sets->setze_land_industry_chains(1);
		sets->setze_city_industry_chains(0);
		sets->setze_tourist_attractions(1);
		sets->setze_verkehr_level(7);
		sets->setze_karte_nummer( 33 );
		sets->setze_station_coverage( umgebung_t::station_coverage_size );
	}

	sets->setze_starting_year( umgebung_t::starting_year );
	sets->setze_use_timeline( umgebung_t::use_timeline==1 );
	sets->setze_bits_per_month( umgebung_t::bits_per_month );
	sets->setze_just_in_time( umgebung_t::just_in_time );

	// standard prices
	warenbauer_t::set_multiplier( 1000 );

	zeiger = 0;
	plan = 0;
	ij_off = koord::invalid;
	x_off = 0;
	y_off = 0;
	grid_hgts = 0;
	einstellungen = sets;
	schedule_counter = 0;
	nosave = false;

	for(int i=0; i<MAX_PLAYER_COUNT ; i++) {
		spieler[i] = 0;
	}

	// set builder to invalid tile ...
	extern koord3d wkz_wegebau_start;
	wkz_wegebau_start = koord3d::invalid;
}


karte_t::~karte_t()
{
    destroy();

    if(einstellungen) {
        delete einstellungen;
        einstellungen = NULL;
    }
}


bool karte_t::can_lower_plan_to(sint16 x, sint16 y, sint16 h) const
{
	const planquadrat_t *plan = lookup(koord(x,y));

	if(plan == 0 || !is_plan_height_changeable(x, y)) {
		return false;
	}

	int hmax = plan->gib_kartenboden()->gib_hoehe();
	// irgendwo ein Tunnel vergraben?
	while(h < hmax) {
		if(plan->gib_boden_in_hoehe(h)) {
			return false;
		}
		h += Z_TILE_STEP;
	}
	return true;
}


bool karte_t::can_raise_plan_to(sint16 x, sint16 y, sint16 h) const
{
	const planquadrat_t *plan = lookup(koord(x,y));
	if(plan == 0 || !is_plan_height_changeable(x, y)) {
		return false;
	}

	// irgendwo eine Br�cke im Weg?
	int hmin = plan->gib_kartenboden()->gib_hoehe();
	while(h > hmin) {
		if(plan->gib_boden_in_hoehe(h)) {
			return false;
		}
		h -= Z_TILE_STEP;
	}
	return true;
}


bool karte_t::is_plan_height_changeable(sint16 x, sint16 y) const
{
	const planquadrat_t *plan = lookup(koord(x,y));
	bool ok = true;

	if(plan != NULL) {
		grund_t *gr = plan->gib_kartenboden();

		ok = (gr->ist_natur() || gr->ist_wasser())  &&  !gr->hat_wege();

		for(  int i=0; ok  &&  i<gr->gib_top(); i++  ) {
			const ding_t *dt = gr->obj_bei(i);
			if(dt != NULL) {
			ok =
				dt->gib_typ() == ding_t::baum  ||
				dt->gib_typ() == ding_t::zeiger  ||
				dt->gib_typ() == ding_t::wolke  ||
				dt->gib_typ() == ding_t::sync_wolke  ||
				dt->gib_typ() == ding_t::async_wolke  ||
				dt->gib_typ() == ding_t::groundobj;
			}
		}
	}

	return ok;
}


bool karte_t::can_raise_to(sint16 x, sint16 y, sint16 h) const
{
    const planquadrat_t *plan = lookup(koord(x,y));
    bool ok = true;		// annahme, es geht, pruefung ob nicht

    if(plan) {
	if(lookup_hgt(koord(x, y)) < h) {
	    ok =
		// Nachbar-Planquadrate testen
		can_raise_plan_to(x-1,y-1, h) &&
		can_raise_plan_to(x,y-1, h)   &&
		can_raise_plan_to(x-1,y, h)   &&
		can_raise_plan_to(x,y, h)     &&
#ifndef DOUBLE_GROUNDS
		// Nachbar-Gridpunkte testen
		can_raise_to(x-1, y-1, h-Z_TILE_STEP) &&
		can_raise_to(x  , y-1, h-Z_TILE_STEP) &&
		can_raise_to(x+1, y-1, h-Z_TILE_STEP) &&
		can_raise_to(x-1, y  , h-Z_TILE_STEP) &&
		can_raise_to(x+1, y  , h-Z_TILE_STEP) &&
		can_raise_to(x-1, y+1, h-Z_TILE_STEP) &&
		can_raise_to(x  , y+1, h-Z_TILE_STEP) &&
		can_raise_to(x+1, y+1, h-Z_TILE_STEP);
#else
		// Nachbar-Gridpunkte testen
		can_raise_to(x-1, y-1, h-Z_TILE_STEP*2) &&
		can_raise_to(x  , y-1, h-Z_TILE_STEP*2) &&
		can_raise_to(x+1, y-1, h-Z_TILE_STEP*2) &&
		can_raise_to(x-1, y  , h-Z_TILE_STEP*2) &&
		can_raise_to(x+1, y  , h-Z_TILE_STEP*2) &&
		can_raise_to(x-1, y+1, h-Z_TILE_STEP*2) &&
		can_raise_to(x  , y+1, h-Z_TILE_STEP*2) &&
		can_raise_to(x+1, y+1, h-Z_TILE_STEP*2);
#endif
	}
    } else {
	ok = false;
    }
    return ok;
}

bool karte_t::can_raise(sint16 x, sint16 y) const
{
	if(ist_in_gittergrenzen(x, y)) {
		return can_raise_to(x, y, lookup_hgt(koord(x, y))+Z_TILE_STEP);
	} else {
		return true;
	}
}

int karte_t::raise_to(sint16 x, sint16 y, sint16 h,bool set_slopes)
{
	const koord pos (x,y);
	int n = 0;

	if(ist_in_gittergrenzen(x,y)) {
		if(lookup_hgt(pos) < h) {
			setze_grid_hgt(pos, h);

			n = 1;

#ifndef DOUBLE_GROUNDS
			n += raise_to(x-1, y-1, h-Z_TILE_STEP,set_slopes);
			n += raise_to(x  , y-1, h-Z_TILE_STEP,set_slopes);
			n += raise_to(x+1, y-1, h-Z_TILE_STEP,set_slopes);
			n += raise_to(x-1, y  , h-Z_TILE_STEP,set_slopes);

			n += raise_to(x, y, h,set_slopes);

			n += raise_to(x+1, y  , h-Z_TILE_STEP,set_slopes);
			n += raise_to(x-1, y+1, h-Z_TILE_STEP,set_slopes);
			n += raise_to(x  , y+1, h-Z_TILE_STEP,set_slopes);
			n += raise_to(x+1, y+1, h-Z_TILE_STEP,set_slopes);
#else
			n += raise_to(x-1, y-1, h-Z_TILE_STEP*2,set_slopes);
			n += raise_to(x  , y-1, h-Z_TILE_STEP*2,set_slopes);
			n += raise_to(x+1, y-1, h-Z_TILE_STEP*2,set_slopes);
			n += raise_to(x-1, y  , h-Z_TILE_STEP*2,set_slopes);

			n += raise_to(x, y, h,set_slopes);

			n += raise_to(x+1, y  , h-Z_TILE_STEP*2,set_slopes);
			n += raise_to(x-1, y+1, h-Z_TILE_STEP*2,set_slopes);
			n += raise_to(x  , y+1, h-Z_TILE_STEP*2,set_slopes);
			n += raise_to(x+1, y+1, h-Z_TILE_STEP*2,set_slopes);
#endif
			if(set_slopes) {
				planquadrat_t *plan;
				if((plan=access(x,y))) {
					plan->angehoben( this );
				}

				if((plan = access(x-1,y))) {
					plan->angehoben( this );
				}

				if((plan = access(x,y-1))) {
					plan->angehoben( this );
				}

				if((plan = access(x-1,y-1))) {
					plan->angehoben( this );
				}
			}

		}
	}

	return n;
}


int karte_t::raise(koord pos)
{
	bool ok = can_raise(pos.x, pos.y);
	int n = 0;
	if(ok && ist_in_gittergrenzen(pos)) {
		n = raise_to(pos.x, pos.y, lookup_hgt(pos)+Z_TILE_STEP,true);
	}
	return n;
}

bool karte_t::can_lower_to(sint16 x, sint16 y, sint16 h) const
{
    const planquadrat_t *plan = lookup(koord(x,y));
    bool ok = true;		// annahme, es geht, pruefung ob nicht

    if(plan) {
	if(lookup_hgt(koord(x, y)) > h) {
	    ok =
		// Nachbar-Planquadrate testen
		can_lower_plan_to(x-1,y-1, h) &&
		can_lower_plan_to(x,y-1, h)   &&
		can_lower_plan_to(x-1,y, h)   &&
		can_lower_plan_to(x,y, h)     &&
#ifndef DOUBLE_GROUNDS
		// Nachbar-Gridpunkte testen
		can_lower_to(x-1, y-1, h+Z_TILE_STEP) &&
		can_lower_to(x  , y-1, h+Z_TILE_STEP) &&
		can_lower_to(x+1, y-1, h+Z_TILE_STEP) &&
		can_lower_to(x-1, y  , h+Z_TILE_STEP) &&
		can_lower_to(x+1, y  , h+Z_TILE_STEP) &&
		can_lower_to(x-1, y+1, h+Z_TILE_STEP) &&
		can_lower_to(x  , y+1, h+Z_TILE_STEP) &&
		can_lower_to(x+1, y+1, h+Z_TILE_STEP);
#else
		// Nachbar-Gridpunkte testen
		can_lower_to(x-1, y-1, h+Z_TILE_STEP*2) &&
		can_lower_to(x  , y-1, h+Z_TILE_STEP*2) &&
		can_lower_to(x+1, y-1, h+Z_TILE_STEP*2) &&
		can_lower_to(x-1, y  , h+Z_TILE_STEP*2) &&
		can_lower_to(x+1, y  , h+Z_TILE_STEP*2) &&
		can_lower_to(x-1, y+1, h+Z_TILE_STEP*2) &&
		can_lower_to(x  , y+1, h+Z_TILE_STEP*2) &&
		can_lower_to(x+1, y+1, h+Z_TILE_STEP*2);
#endif
	}
    } else {
	ok = false;
    }
    return ok;
}



bool karte_t::can_lower(sint16 x, sint16 y) const
{
	if(ist_in_gittergrenzen(x, y)) {
		return can_lower_to(x, y, lookup_hgt(koord(x, y))-Z_TILE_STEP);
	} else {
		return true;
	}
}



int karte_t::lower_to(sint16 x, sint16 y, sint16 h, bool set_slopes)
{
	int n = 0;
	if(ist_in_gittergrenzen(x,y)) {
		const koord pos (x, y);

		if(lookup_hgt(pos) > h) {
			setze_grid_hgt(pos, h);

			n = 1;

#ifndef DOUBLE_GROUNDS
			n += lower_to(x-1, y-1, h+Z_TILE_STEP,set_slopes);
			n += lower_to(x  , y-1, h+Z_TILE_STEP,set_slopes);
			n += lower_to(x+1, y-1, h+Z_TILE_STEP,set_slopes);
			n += lower_to(x-1, y  , h+Z_TILE_STEP,set_slopes);

			n += lower_to(x, y, h,set_slopes);

			n += lower_to(x+1, y  , h+Z_TILE_STEP,set_slopes);
			n += lower_to(x-1, y+1, h+Z_TILE_STEP,set_slopes);
			n += lower_to(x  , y+1, h+Z_TILE_STEP,set_slopes);
			n += lower_to(x+1, y+1, h+Z_TILE_STEP,set_slopes);
#else
			n += lower_to(x-1, y-1, h+Z_TILE_STEP*2,set_slopes);
			n += lower_to(x  , y-1, h+Z_TILE_STEP*2,set_slopes);
			n += lower_to(x+1, y-1, h+Z_TILE_STEP*2,set_slopes);
			n += lower_to(x-1, y  , h+Z_TILE_STEP*2,set_slopes);

			n += lower_to(x, y, h,set_slopes);

			n += lower_to(x+1, y  , h+Z_TILE_STEP*2,set_slopes);
			n += lower_to(x-1, y+1, h+Z_TILE_STEP*2,set_slopes);
			n += lower_to(x  , y+1, h+Z_TILE_STEP*2,set_slopes);
			n += lower_to(x+1, y+1, h+Z_TILE_STEP*2,set_slopes);
#endif
			if(set_slopes) {
				planquadrat_t *plan;
				if((plan=access(x,y))) {
					plan->abgesenkt( this );
				}

				if((plan = access(x-1,y))) {
					plan->abgesenkt( this );
				}

				if((plan = access(x,y-1))) {
					plan->abgesenkt( this );
				}

				if((plan = access(x-1,y-1))) {
					plan->abgesenkt( this );
				}
			}
		}
	}
  return n;
}



int karte_t::lower(koord pos)
{
	bool ok = can_lower(pos.x, pos.y);
	int n = 0;
	if(ok && ist_in_gittergrenzen(pos.x, pos.y)) {
		n = lower_to(pos.x, pos.y, lookup_hgt(pos)-Z_TILE_STEP,true);
	}
	return n;
}


static koord ebene_offsets[] = {koord(0,0), koord(1,0), koord(0,1), koord(1,1)};

bool karte_t::can_ebne_planquadrat(koord pos, sint16 hgt)
{
	for(int i=0; i<4; i++) {
		koord p = pos + ebene_offsets[i];

		if(lookup_hgt(p) > hgt) {

			if(!can_lower_to(p.x, p.y, hgt)) {
				return false;
			}

		} else if(lookup_hgt(p) < hgt) {

			if(!can_raise_to(p.x, p.y, hgt)) {
				return false;
			}
		}
	}
	return true;
}



// make a flat leve at this position (only used for AI at the moment)
bool karte_t::ebne_planquadrat(spieler_t *sp, koord pos, sint16 hgt)
{
	int n = 0;
	bool ok = true;

	for(int i=0; i<4; i++) {
		koord p = pos + ebene_offsets[i];

		if(lookup_hgt(p) > hgt) {

			if(can_lower_to(p.x, p.y, hgt)) {
				n += lower_to(p.x, p.y, hgt, true);
			} else {
				ok = false;
				break;
			}

		} else if(lookup_hgt(p) < hgt) {

			if(can_raise_to(p.x, p.y, hgt)) {
				n += raise_to(p.x, p.y, hgt, true);
			} else {
				ok = false;
				break;
			}
		}
	}
	// was changed => recalc + pay for it
	if(n>0) {
		// update image
		for(int j=pos.y-n; j<=pos.y+n; j++) {
			for(int i=pos.x-n; i<=pos.x+n; i++) {
				grund_t *gr = lookup_kartenboden( koord(i,j) );
				if(gr) gr->calc_bild();
			}
		}
		sp->buche( n*umgebung_t::cst_alter_land, pos, COST_CONSTRUCTION );
	}
	return ok;
}



void karte_t::setze_maus_funktion( int (* funktion)(spieler_t *, karte_t *, koord), int zeiger_bild, int zeiger_versatz, int ok_sound, int ko_sound)
{
	setze_maus_funktion( (int (*)(spieler_t *, karte_t *, koord, value_t))funktion, zeiger_bild, zeiger_versatz, 0l, ok_sound, ko_sound);
}



/**
 * Spezialvarainte mit einem Parameter, der immer �bergeben wird
 * Hajo: changed parameter type from long to value_t because some
 *       parts of the code pass pointers
 * @author V. Meyer, Hj. Malthaner
 */
void karte_t::setze_maus_funktion(int (* funktion)(spieler_t *, karte_t *, koord, value_t param), int zeiger_bild, int zeiger_versatz, value_t param, int ok_sound, int ko_sound)
{
	// gibt es eien neue funktion ?
	if(funktion != NULL) {
		// gab es eine alte funktion ?
		if(current_mouse_funk.funk!=NULL) {
			current_mouse_funk.funk(get_active_player(), this, EXIT, current_mouse_funk.param);
		}

		is_dragging = false;

		struct sound_info info;
		info.index = SFX_SELECT;
		info.volume = 255;
		info.pri = 0;
		sound_play(info);

		current_mouse_funk.funk = funktion;
		current_mouse_funk.param = param;
		current_mouse_funk.ok_sound = ok_sound;
		current_mouse_funk.ko_sound = ko_sound;
		current_mouse_funk.last_pos = koord::invalid;
		current_mouse_funk.zeiger_versatz = zeiger_versatz;
		current_mouse_funk.zeiger_bild = zeiger_bild;

		// reset pointer
		koord3d zpos = zeiger->gib_pos();
		zeiger->setze_bild(zeiger_bild);
		zeiger->setze_yoff(zeiger_versatz);
		zeiger->change_pos( zpos );

		current_mouse_funk.funk(get_active_player(), this, INIT, current_mouse_funk.param);
	}
}



/**
 * Sets grid height.
 * Never set grid_hgts manually, always
 * use this method.
 * @author Hj. Malthaner
 */
void karte_t::setze_grid_hgt(koord k, int hgt)
{
	if(ist_in_gittergrenzen(k.x, k.y)) {
		grid_hgts[k.x + k.y*(gib_groesse_x()+1)] = (hgt/Z_TILE_STEP);
	}
}


int
karte_t::min_hgt(const koord pos) const
{
	const int h1 = lookup_hgt(pos);
	const int h2 = lookup_hgt(pos+koord(1, 0));
	const int h3 = lookup_hgt(pos+koord(1, 1));
	const int h4 = lookup_hgt(pos+koord(0, 1));

	return min(min(h1,h2), min(h3,h4));
}


int
karte_t::max_hgt(const koord pos) const
{
	const int h1 = lookup_hgt(pos);
	const int h2 = lookup_hgt(pos+koord(1, 0));
	const int h3 = lookup_hgt(pos+koord(1, 1));
	const int h4 = lookup_hgt(pos+koord(0, 1));

	return max(max(h1,h2), max(h3,h4));
}


void
karte_t::rotate90()
{
	// asumme we can save this rotation
	nosave = false;

	//announce current target rotation
	einstellungen->rotate90();

	// clear marked region
	zeiger->change_pos( koord3d::invalid );

	// first: rotate all things on the map
	planquadrat_t *new_plan = new planquadrat_t[cached_groesse_gitter_y*cached_groesse_gitter_x];
	for( int x=0;  x<cached_groesse_gitter_x;  x++  ) {
		for( int y=0;  y<cached_groesse_gitter_y;  y++  ) {
			int nr = x+(y*cached_groesse_gitter_x);
			int new_nr = (cached_groesse_karte_y-y)+(x*cached_groesse_gitter_y);
			new_plan[new_nr] = plan[nr];
			plan[nr] = planquadrat_t();
			// now rotate everything on the ground(s)
			for(  uint i=0;  i<new_plan[new_nr].gib_boden_count();  i++  ) {
				new_plan[new_nr].gib_boden_bei(i)->rotate90();
			}
		}
	}
	delete [] plan;
	plan = new_plan;

	// rotate heightmap
	sint8 *new_hgts = new sint8[(cached_groesse_gitter_x+1)*(cached_groesse_gitter_y+1)];
	for( int x=0;  x<=cached_groesse_gitter_x;  x++  ) {
		for( int y=0;  y<=cached_groesse_gitter_y;  y++  ) {
			int nr = x+(y*(cached_groesse_gitter_x+1));
			int new_nr = (cached_groesse_gitter_y-y)+(x*(cached_groesse_gitter_y+1));
			new_hgts[new_nr] = grid_hgts[nr];
		}
	}
	delete [] grid_hgts;
	grid_hgts = new_hgts;

	// rotate borders
	sint16 xw = cached_groesse_karte_x;
	cached_groesse_karte_x = cached_groesse_karte_y;
	cached_groesse_karte_y = xw;

	int wx = cached_groesse_gitter_x;
	cached_groesse_gitter_x = cached_groesse_gitter_y;
	cached_groesse_gitter_y = wx;

	// rotate all other objects like factories and convois
	for(unsigned i=0; i<convoi_array.get_count();  i++) {
		convoi_array[i]->rotate90( cached_groesse_karte_x );
	}

	// now step all towns (to generate passengers)
	for (weighted_vector_tpl<stadt_t*>::const_iterator i = stadt.begin(), end = stadt.end(); i != end; ++i) {
		(*i)->rotate90( cached_groesse_karte_x );
	}

	slist_iterator_tpl<fabrik_t *> iter(fab_list);
	while(iter.next()) {
		iter.get_current()->rotate90( cached_groesse_karte_x );
	}

	slist_iterator_tpl <halthandle_t> halt_iter (haltestelle_t::gib_alle_haltestellen());
	while( halt_iter.next() ) {
		halt_iter.get_current()->rotate90( cached_groesse_karte_x );
	}

	for(  int i=0;  i<MAX_PLAYER_COUNT;  i++  ) {
		spieler[i]->rotate90( cached_groesse_karte_x );
	}

	// rotate label texts
	slist_iterator_tpl <koord> label_iter (labels);
	while( label_iter.next() ) {
		label_iter.access_current().rotate90( cached_groesse_karte_x );
	}

	// rotate view
	ij_off.rotate90( cached_groesse_karte_x );

	if(cached_groesse_gitter_x != cached_groesse_gitter_y) {
		// the marking array and the map must be reinit
		marker.init( cached_groesse_gitter_x, cached_groesse_gitter_y );
		reliefkarte_t::gib_karte()->setze_welt( this );
	}

	//  rotate map search array
	fabrikbauer_t::neue_karte( this );

	// update minimap
	if(reliefkarte_t::is_visible) {
		reliefkarte_t::gib_karte()->set_mode( reliefkarte_t::gib_karte()->get_mode() );
	}

	// finally recalculate schedules for goods in transit ...
	set_schedule_counter();

	setze_dirty();
}
// -------- Verwaltung von Fabriken -----------------------------


bool
karte_t::add_fab(fabrik_t *fab)
{
//DBG_MESSAGE("karte_t::add_fab()","fab = %p",fab);
	assert(fab != NULL);
	fab_list.insert( fab );
	return true;
}



// beware: must remove also links from stops and towns
bool karte_t::rem_fab(fabrik_t *fab)
{
	if(!fab_list.remove( fab )) {
		return false;
	}

	// now all the interwoven connections must be cleared
	koord pos = fab->gib_pos().gib_2d();
	planquadrat_t* plan = access(pos);
	if(plan) {

		// we need a copy, since the verbinde fabriken is modifying the list
		halthandle_t list[16];
		const uint8 count = plan->get_haltlist_count();
		assert(count<16);
		memcpy( list, plan->get_haltlist(), count*sizeof(halthandle_t) );
		for( uint8 i=0;  i<count;  i++  ) {
			// first remove all the tiles that do not connect
			plan->remove_from_haltlist( this, list[i] );
			// then reconnect
			list[i]->verbinde_fabriken();
		}

		// remove all links from factories
		slist_iterator_tpl<fabrik_t *> iter (fab_list);
		while(iter.next()) {
			fabrik_t * fab = iter.get_current();
			fab->rem_lieferziel(pos);
			fab->rem_supplier(pos);
		}

		// remove all links to cities
		slist_iterator_tpl<stadt_t *> iter_city (fab->gib_arbeiterziele());
		while(iter_city.next()) {
			iter_city.get_current()->remove_arbeiterziel(fab);
		}

		// finally delete it
		delete fab;

		// recalculate factory position map
		fabrikbauer_t::neue_karte(this);
	}
	return true;
}

fabrik_t *
karte_t::get_random_fab() const
{
	const int anz = fab_list.count();
	fabrik_t *result = NULL;

	if(anz > 0) {
		const int end = simrand( anz );
		result = fab_list.at(end);
	}
	return result;
}


/*----------------------------------------------------------------------------------------------------------------------*/
/* same procedure for tourist attractions */

void
karte_t::add_ausflugsziel(gebaeude_t *gb)
{
	assert(gb != NULL);
	ausflugsziele.append( gb, gb->gib_tile()->gib_besch()->gib_level(), 16 );
//DBG_MESSAGE("karte_t::add_ausflugsziel()","appended ausflugsziel at %i",ausflugsziele.get_count() );
}

void
karte_t::remove_ausflugsziel(gebaeude_t *gb)
{
	assert(gb != NULL);
	ausflugsziele.remove( gb );
}



/* select a random target for a tourist; targets are weighted by their importance */
const gebaeude_t *
karte_t::gib_random_ausflugsziel() const
{
	const unsigned long sum_pax=ausflugsziele.get_sum_weight();
	if (!ausflugsziele.empty() && sum_pax > 0) {
		return ausflugsziele.at_weight( simrand(sum_pax) );
	}
	// so there are no destinations ... should never occur ...
	dbg->fatal("karte_t::gib_random_ausflugsziel()","nothing found.");
	return NULL;
}


// -------- Verwaltung von Staedten -----------------------------

stadt_t *karte_t::suche_naechste_stadt(const koord pos) const
{
    long min_dist = 99999999;
    stadt_t *best = NULL;

	for (weighted_vector_tpl<stadt_t*>::const_iterator i = stadt.begin(), end = stadt.end(); i != end; ++i) {
		stadt_t* s = *i;
		const koord k = s->gib_pos();
	    const long dist = (pos.x-k.x)*(pos.x-k.x) + (pos.y-k.y)*(pos.y-k.y);
	    if(dist < min_dist) {
				min_dist = dist;
				best = s;
	    }
    }
    return best;
}


// -------- Verwaltung von synchronen Objekten ------------------

bool
karte_t::sync_add(sync_steppable *obj)
{
	assert(obj != NULL);
	sync_add_list.insert( obj );
	return true;
}

bool
karte_t::sync_remove(sync_steppable *obj)	// entfernt alle dinge == obj aus der Liste
{
	assert(obj != NULL);
	if(sync_add_list.remove(obj)) {
		return true;
	}
	return sync_list.remove( obj );
}



/*
 * this routine is called before an image is displayed
 * it moves vehicles and pedestrians
 * only time consuming thing are done in step()
 * everything else is done here
 */
void
karte_t::sync_step(long delta_t)
{
	// just for progress
	ticks += delta_t;

	// ingore calls by interrupt during fast forward ...
	if (!sync_add_list.empty()) {
		slist_iterator_tpl<sync_steppable *> iter (sync_add_list);
		while(iter.next()) {
			sync_steppable *ss = iter.get_current();
			sync_list.put( ss, ss );
		}
		sync_add_list.clear();
	}

	ptrhashtable_iterator_tpl<sync_steppable*,sync_steppable*> iter (sync_list);

	// Hajo: we use a slight hack here to remove the current
	// object from the list without wrecking the iterator
	bool ok = iter.next();
	while(ok) {
		sync_steppable *ss = iter.get_current_value();

		// Hajo: advance iterator, so that we can remove the current object
		// safely
		ok = iter.next();

		if (!ss->sync_step(delta_t)) {
			sync_list.remove(ss);
			delete ss;
		}

	}

	for(int x=0; x<8; x++) {
		gib_spieler(x)->age_messages(delta_t);
	}

	// change view due to following a convoi?
	if(follow_convoi.is_bound()) {
		const koord3d new_pos=follow_convoi->gib_vehikel(0)->gib_pos();
		if(new_pos!=koord3d::invalid) {
			const sint16 rw = get_tile_raster_width();
			const sint16 new_xoff = tile_raster_scale_x(-follow_convoi->gib_vehikel(0)->gib_xoff(),rw);
			const sint16 new_yoff =	tile_raster_scale_y(-follow_convoi->gib_vehikel(0)->gib_yoff(),rw) + tile_raster_scale_y(new_pos.z*TILE_HEIGHT_STEP/Z_TILE_STEP,rw);
			change_world_position( new_pos.gib_2d(), new_xoff, new_yoff );
		}
	}

	if(!fast_forward  ||  delta_t!=MAGIC_STEP) {
		// display new frame with water animation
		intr_refresh_display( false );
		update_frame_sleep_time();
	}
}


// does all the magic about idle time and such stuff
void
karte_t::update_frame_sleep_time()
{
	// get average frame time
	uint32 last_ms = dr_time();
	last_frame_ms[last_frame_idx] = last_ms;
	last_step_nr[last_frame_idx] = steps;
	last_frame_idx = (last_frame_idx+1)%32;
	if(last_frame_ms[last_frame_idx]<last_ms) {
		realFPS = (1000*32) / (last_ms-last_frame_ms[last_frame_idx]);
		simloops = ((steps-last_step_nr[last_frame_idx])*10000*16)/((last_ms-last_frame_ms[last_frame_idx])*time_multiplier);
	}
	else {
		realFPS = umgebung_t::fps;
		simloops = 60;
	}

	// now change the pauses
	if(!fast_forward) {
		// change pause/frame spacing ...
		// the frame spacing will be only touched in emergencies
		if(simloops<=10) {
			if(next_wait_time==0) {
				set_frame_time( get_frame_time()+2 );
			}
			next_wait_time = 0;
		}
		else if(simloops<=20) {
			next_wait_time = 0;
			increase_frame_time();
		}
		else if(simloops>50) {
			if(next_wait_time+1<(uint32)get_frame_time()) {
				next_wait_time ++;
			}
			else {
				next_wait_time = get_frame_time();
			}
		}
		else if(simloops<45  &&  next_wait_time>0) {
			next_wait_time --;
		}

		if(realFPS>(uint16)(umgebung_t::fps*17)/16  ||  simloops<=30) {
			increase_frame_time();
			if(next_wait_time>0) {
				next_wait_time --;
			}
		}
		else if(realFPS<(uint16)umgebung_t::fps) {
			if(realFPS<(uint16)(umgebung_t::fps/2)  &&  next_wait_time>0) {
				set_frame_time( get_frame_time()-1 );
			}
			else {
				reduce_frame_time();
			}
		}
	}
	else if((last_frame_idx&7)==0){
		// fast forward mode

		// try to get a target speed
		int five_back = (last_frame_idx+31-6)%32;
		int one_back = (last_frame_idx+32-1)%32;
		sint32 last_simloops = ((last_step_nr[one_back]-last_step_nr[five_back])*10000)/(last_frame_ms[one_back]-last_frame_ms[five_back]);

		if(last_simloops<=10) {
			increase_frame_time();
		}
		else {
			int frame_intervall = max( 100, 1000/umgebung_t::fps );
			if(get_frame_time()>frame_intervall) {
				reduce_frame_time();
			}
			else {
				if(last_simloops<umgebung_t::max_acceleration*50) {
					if(next_wait_time>0) {
						if(last_simloops<umgebung_t::max_acceleration*5) {
							next_wait_time = max( next_wait_time-3, 0 );
						}
						else {
							next_wait_time --;
						}
					}
				}
				else if(last_simloops>umgebung_t::max_acceleration*50) {
					if(next_wait_time<90) {
						if(last_simloops>umgebung_t::max_acceleration*5) {
							// brake much more ...
							next_wait_time += 5;
						}
						else {
							next_wait_time ++;
						}
					}
				}
			}
		}
	}
	this_wait_time = next_wait_time;
}



void
karte_t::neuer_monat()
{
	current_month ++;
	letzter_monat ++;
	if(letzter_monat>11) {
		letzter_monat = 0;
	}
	DBG_MESSAGE("karte_t::neuer_monat()","Month %d has started", letzter_monat);

	DBG_MESSAGE("karte_t::neuer_monat()","sync_step %u objects", sync_list.count() );

	// this should be done before a map update, since the map may want an update of the way usage
//	DBG_MESSAGE("karte_t::neuer_monat()","ways");
	slist_iterator_tpl <weg_t *> weg_iter (weg_t::gib_alle_wege());
	while( weg_iter.next() ) {
		weg_iter.get_current()->neuer_monat();
	}

	// recalc old settings (and maybe update the staops with the current values)
	reliefkarte_t::gib_karte()->neuer_monat();

	INT_CHECK("simworld 1701");

//	DBG_MESSAGE("karte_t::neuer_monat()","convois");
	// hsiegeln - call new month for convois
	for(unsigned i=0;  i<convoi_array.get_count();  i++ ) {
		convoihandle_t cnv = convoi_array[i];
		cnv->new_month();
	}

	INT_CHECK("simworld 1701");

//	DBG_MESSAGE("karte_t::neuer_monat()","factories");
	slist_iterator_tpl<fabrik_t*> iter (fab_list);
	while(iter.next()) {
		fabrik_t * fab = iter.get_current();
		fab->neuer_monat();
	}
	INT_CHECK("simworld 1278");


//	DBG_MESSAGE("karte_t::neuer_monat()","cities");
	// roll city history and copy the new citicens (i.e. the new weight) into the stadt array
	// no INT_CHECK() here, or dialoges will go crazy!!!
	weighted_vector_tpl<stadt_t*> new_weighted_stadt(stadt.get_count() + 1);
	for (weighted_vector_tpl<stadt_t*>::const_iterator i = stadt.begin(), end = stadt.end(); i != end; ++i) {
		stadt_t* s = *i;
		s->neuer_monat();
		new_weighted_stadt.append(s, s->gib_einwohner(), 64);
		INT_CHECK("simworld 1278");
	}
	swap(stadt, new_weighted_stadt);

	INT_CHECK("simworld 1282");

//	DBG_MESSAGE("karte_t::neuer_monat()","players");
	// spieler
	for(int i=0; i<MAX_PLAYER_COUNT; i++) {
		if(spieler[i] != NULL) {
			spieler[i]->neuer_monat();
		}
	}

	INT_CHECK("simworld 1289");

//	DBG_MESSAGE("karte_t::neuer_monat()","halts");
	slist_iterator_tpl <halthandle_t> halt_iter (haltestelle_t::gib_alle_haltestellen());
	while( halt_iter.next() ) {
		halt_iter.get_current()->neuer_monat();
		INT_CHECK("simworld 1877");
	}

	// now switch year to get the right year for all timeline stuff ...
	if(letzter_monat==0) {
		neues_jahr();
		INT_CHECK("simworld 1299");
	}

	wegbauer_t::neuer_monat(this);
	INT_CHECK("simworld 1299");

	recalc_average_speed();
	INT_CHECK("simworld 1921");

	if(umgebung_t::autosave>0  &&  letzter_monat%umgebung_t::autosave==0) {
		char buf[128];
		sprintf( buf, "save/autosave%02i.sve", letzter_monat+1 );
		speichern( buf, true );
	}
}



void karte_t::neues_jahr()
{
	letztes_jahr = current_month/12;

DBG_MESSAGE("karte_t::neues_jahr()","Year %d has started", letztes_jahr);

	char buf[256];
	sprintf(buf,translator::translate("Year %i has started."),letztes_jahr);
	message_t::get_instance()->add_message(buf,koord::invalid,message_t::general,COL_BLACK,skinverwaltung_t::neujahrsymbol->gib_bild_nr(0));

	for(unsigned i=0;  i<convoi_array.get_count();  i++ ) {
		convoihandle_t cnv = convoi_array[i];
		cnv->neues_jahr();
	}

	for(int i=0; i<MAX_PLAYER_COUNT; i++) {
		gib_spieler(i)->neues_jahr();
	}
}



// recalculated speed boni for different vehicles
// and takes care of all timeline stuff
void karte_t::recalc_average_speed()
{
	// retire/allocate vehicles
	stadtauto_t::built_timeline_liste(this);

//	DBG_MESSAGE("karte_t::recalc_average_speed()","");
	if(use_timeline()) {

		char	buf[256];
		for(int i=road_wt; i<=air_wt; i++) {
			slist_tpl<const vehikel_besch_t*>* cl = vehikelbauer_t::gib_info((waytype_t)i);
			if(cl) {
				const char *vehicle_type=NULL;
				switch(i) {
					case road_wt:
						vehicle_type = "road vehicle";
						break;
					case track_wt:
						vehicle_type = "rail car";
						break;
					case water_wt:
						vehicle_type = "water vehicle";
						break;
					case monorail_wt:
						vehicle_type = "monorail vehicle";
						break;
					case tram_wt:
						vehicle_type = "street car";
						break;
					case air_wt:
						vehicle_type = "airplane";
						break;
				}
				vehicle_type = translator::translate( vehicle_type );

				slist_iterator_tpl<const vehikel_besch_t*> vehinfo(cl);
				while (vehinfo.next()) {
					const vehikel_besch_t* info = vehinfo.get_current();
					const uint16 intro_month = info->get_intro_year_month();
					if(intro_month == current_month) {
						sprintf(buf,
							translator::translate("New %s now available:\n%s\n"),
							vehicle_type,
							translator::translate(info->gib_name()));
							message_t::get_instance()->add_message(buf,koord::invalid,message_t::new_vehicle,NEW_VEHICLE,info->gib_basis_bild());
					}

					const uint16 retire_month = info->get_retire_year_month();
					if(retire_month == current_month) {
						sprintf(buf,
							translator::translate("Production of %s has been stopped:\n%s\n"),
							vehicle_type,
							translator::translate(info->gib_name()));
							message_t::get_instance()->add_message(buf,koord::invalid,message_t::new_vehicle,NEW_VEHICLE,info->gib_basis_bild());
					}
				}
			}
		}

		// city road check
		const weg_besch_t* city_road_test = wegbauer_t::gib_besch(*umgebung_t::city_road_type, get_timeline_year_month());
		if(city_road_test) {
			city_road = city_road_test;
		}
		else {
			DBG_MESSAGE("karte_t::neuer_monat()","Month %d has started", letzter_monat);
			city_road = wegbauer_t::weg_search(road_wt,30,get_timeline_year_month(),weg_t::type_flat);
		}

		int num_averages[4]={0,0,0,0};
		int speed_sum[4]={0,0,0,0};
		const uint16 timeline_month = get_timeline_year_month();
		for(int i=road_wt; i<=air_wt; i++) {
			// check for speed
			const int speed_bonus_categorie = (i>=4  &&  i<=7) ? 2 : (i==air_wt ? 4 : i );
			slist_tpl<const vehikel_besch_t*>* cl = vehikelbauer_t::gib_info((waytype_t)i);
			if(cl) {
				slist_iterator_tpl<const vehikel_besch_t*> vehinfo(cl);
				while (vehinfo.next()) {
					const vehikel_besch_t* info = vehinfo.get_current();
					if(info->gib_leistung()>0  &&  !info->is_future(timeline_month)  &&  !info->is_retired(timeline_month)) {
						speed_sum[speed_bonus_categorie-1] += info->gib_geschw();
						num_averages[speed_bonus_categorie-1] ++;
					}
				}
			}
		}
		// recalculate them
		for(int i=0;  i<4;  i++) {
			if(num_averages[i]>0) {
				average_speed[i] = speed_sum[i]/num_averages[i];
			}
		}
	}
	else {
		// defaults
		average_speed[0] = 60;
		average_speed[1] = 80;
		average_speed[2] = 40;
		average_speed[3] = 350;

		city_road = wegbauer_t::gib_besch(*umgebung_t::city_road_type, 0);
		if(city_road==NULL) {
			city_road = wegbauer_t::weg_search(road_wt,30,0,weg_t::type_flat);
		}
	}
}



int karte_t::get_average_speed(waytype_t typ) const
{
	switch(typ) {
		case road_wt: return average_speed[0];
		case track_wt:
		case monorail_wt:
		case tram_wt: return average_speed[1];
		case water_wt: return average_speed[2];
		case air_wt: return average_speed[3];
		default: return 1;
	}
}



// returns the current speed record
sint32 karte_t::get_record_speed( waytype_t w ) const
{
	switch(w) {
		case road_wt: return max_road_speed.speed;
		case track_wt:
		case tram_wt: return max_rail_speed.speed;
		case monorail_wt: return max_maglev_speed.speed;
		case water_wt: return max_ship_speed.speed;
		case air_wt: return max_air_speed.speed;
		default: return 0;
	}
}



// sets the new speed record
void karte_t::notify_record( convoihandle_t cnv, sint32 max_speed, koord pos )
{
	speed_record_t *sr = NULL;
	switch(cnv->gib_vehikel(0)->gib_waytype()) {
		case road_wt: sr = &max_road_speed; break;
		case track_wt:
		case tram_wt: sr = &max_rail_speed; break;
		case monorail_wt: sr = &max_maglev_speed; break;
		case water_wt: sr = &max_ship_speed; break;
		case air_wt: sr = &max_air_speed; break;
		default: assert(0);
	}

	// avoid the case of two convois with identical max speed ...
	if(cnv!=sr->cnv  &&  sr->speed+1==max_speed) {
		return;
	}

	// really new/faster?
	if(pos!=sr->pos  ||  sr->speed+1<max_speed) {
		// update it
		sr->cnv = cnv;
		sr->speed = max_speed-1;
		sr->year_month = current_month;
		sr->pos = pos;
		sr->besitzer = NULL; // will be set, when accepted
	}
	else {
		sr->cnv = cnv;
		sr->speed = max_speed-1;
		sr->pos = pos;

		// same convoi and same position
		if(sr->besitzer==NULL  &&  current_month!=sr->year_month) {
			// notfiy the world of this new record
			sr->speed = max_speed-1;
			sr->besitzer = cnv->gib_besitzer();
			const char* msg;
			switch(cnv->gib_vehikel(0)->gib_waytype()) {
				default: NOT_REACHED
				case road_wt:     msg = "New world record for motorcars: %.1f km/h by %s."; break;
				case track_wt:
				case tram_wt:     msg = "New world record for railways: %.1f km/h by %s.";  break;
				case monorail_wt: msg = "New world record for monorails: %.1f km/h by %s."; break;
				case water_wt:    msg = "New world record for ship: %.1f km/h by %s.";      break;
				case air_wt:      msg = "New world record for planes: %.1f km/h by %s.";    break;
			}
			char text[1024];
			sprintf( text, translator::translate(msg), (float)speed_to_kmh(10*sr->speed)/10.0, sr->cnv->gib_name() );
			message_t::get_instance()->add_message(text, sr->pos, message_t::new_vehicle, sr->besitzer->get_player_color1() );
		}
	}
}




void
karte_t::step()
{
	static uint32 tile_counter = 0;

	// first: check for new month
	if(ticks > next_month_ticks) {

		next_month_ticks += karte_t::ticks_per_tag;

		// avoid overflow here ...
		if(ticks>next_month_ticks) {
			ticks %= karte_t::ticks_per_tag;
			ticks += karte_t::ticks_per_tag;
			next_month_ticks = ticks+karte_t::ticks_per_tag;
			last_step_ticks %= karte_t::ticks_per_tag;
		}

		neuer_monat();
	}

	// to make sure the tick counter will be updated
	INT_CHECK("karte_t::step");

	// check for pending seasons change
	if(pending_season_change>0) {
		// process
		const uint32 end_count = min( cached_groesse_gitter_x*cached_groesse_gitter_y,  tile_counter + max( 16384, cached_groesse_gitter_x*cached_groesse_gitter_y/16 ) );
		while(  tile_counter < end_count  ) {
			plan[tile_counter].check_season(current_month);
			tile_counter ++;
			if((tile_counter&0x3FF)==0) {
				INT_CHECK("karte_t::step");
			}
		}
		if(  tile_counter >= (uint32)cached_groesse_gitter_x*(uint32)cached_groesse_gitter_y  ) {
			pending_season_change --;
			tile_counter = 0;
		}
	}

	// to make sure the tick counter will be updated
	INT_CHECK("karte_t::step");

	const long delta_t = (long)ticks-(long)last_step_ticks;
	// needs plausibility check?!?
	if(delta_t>10000  || delta_t<0) {
		last_step_ticks = ticks;
		return;
	}
	// avoid too often steps ...
	if(delta_t<170) {
		return;
	}
	last_step_ticks = ticks;

	// Hajo: Convois need extra frequent steps to avoid unneccesary
	// long waiting times
	// the convois goes alternating up and down to avoid a preferred
	// order and loading only of the train highest in the game
	for(unsigned i=0; i<convoi_array.get_count();  i++) {
		convoihandle_t cnv = convoi_array[i];
		cnv->step();
		if((i&7)==0) {
			INT_CHECK("simworld 1947");
		}
	}

	// now step all towns (to generate passengers)
	for (weighted_vector_tpl<stadt_t*>::const_iterator i = stadt.begin(), end = stadt.end(); i != end; ++i) {
		(*i)->step(delta_t);
	}

	slist_iterator_tpl<fabrik_t *> iter(fab_list);
	while(iter.next()) {
		iter.get_current()->step(delta_t);
	}

	// then step all players
	INT_CHECK("simworld 1975");
	spieler[steps & 7]->step();

	// ok, next step
	steps ++;
	INT_CHECK("simworld 1975");

	if((steps%8)==0) {
		check_midi();
	}

	// will also call all objects if needed ...
	if(  recalc_snowline()  ) {
		pending_season_change ++;
	}
}



/**
 * If this is true, the map will not be scrolled
 * on right-drag
 * @author Hj. Malthaner
 */
void karte_t::set_scroll_lock(bool yesno)
{
	scroll_lock = yesno;
	if(yesno  &&  follow_convoi.is_bound()) {
		follow_convoi = convoihandle_t();
	}
}



// change the center viewport position for a certain ground tile
// any possible convoi to follow will be disabled
void karte_t::change_world_position( koord3d new_ij )
{
	const sint16 rw = get_tile_raster_width();
	change_world_position( new_ij.gib_2d(), 0, tile_raster_scale_y(new_ij.z*TILE_HEIGHT_STEP/Z_TILE_STEP,rw) );
	follow_convoi = convoihandle_t();
}



// change the center viewport position
void karte_t::change_world_position( koord new_ij, sint16 new_xoff, sint16 new_yoff )
{
	//position changed? => update and mark dirty
	if(new_ij!=ij_off  ||  new_xoff!=x_off  ||  new_yoff!=y_off) {
		ij_off = new_ij;
		x_off = new_xoff;
		y_off = new_yoff;
		setze_dirty();
	}
}



void karte_t::blick_aendern(event_t *ev)
{
	if(!scroll_lock) {
		const int raster = get_tile_raster_width();
		koord new_ij = ij_off;

		sint16 new_xoff = x_off - (ev->mx - ev->cx) * einstellungen->gib_scroll_multi();
		new_ij.x -= new_xoff/raster;
		new_ij.y += new_xoff/raster;
		new_xoff %= raster;

		int lines = 0;
		sint16 new_yoff = y_off - (ev->my-ev->cy) * einstellungen->gib_scroll_multi();
		if(y_off>0) {
			lines = (new_yoff + (raster/4))/(raster/2);
		}
		else {
			lines = (new_yoff - (raster/4))/(raster/2);
		}
		new_ij -= koord( lines, lines );
		new_yoff -= (raster/2)*lines;

		// this sets the new position and mark screen dirty
		// => with next refresh we will be at a new location
		change_world_position( new_ij, new_xoff, new_yoff );

		// move the mouse pointer back to starting location => infinite mouse movement
		if ((ev->mx - ev->cx)!=0  ||  (ev->my-ev->cy)!=0) {
#ifdef __BEOS__
			change_drag_start(ev->mx - ev->cx, ev->my - ev->cy);
#else
			display_move_pointer(ev->cx, ev->cy);
#endif
		}
	}
}


// nordhang = 3
// suedhang = 12

/**
 * returns the natural slope a a position
 * @author prissi
 */
uint8	karte_t::calc_natural_slope( const koord pos ) const
{
	if(ist_in_kartengrenzen(pos.x, pos.y)) {

		const sint8 * p = &grid_hgts[pos.x + pos.y*(gib_groesse_x()+1)];

		const int h1 = *p;
		const int h2 = *(p+1);
		const int h3 = *(p+gib_groesse_x()+2);
		const int h4 = *(p+gib_groesse_x()+1);

		const int mini = min(min(h1,h2), min(h3,h4));

		const int d1=h1-mini;
		const int d2=h2-mini;
		const int d3=h3-mini;
		const int d4=h4-mini;

#ifndef DOUBLE_GROUNDS
		const int slope = (d1<<3) + (d2<<2) + (d3<<1) + d4;
#else
		const int slope = (d1*27) + (d2*9) + (d3*3) + d4;
#endif
		return slope;
	}
	return 0;
}



bool
karte_t::ist_wasser(koord pos, koord dim) const
{
	koord k;

	for(k.x = pos.x; k.x < pos.x + dim.x; k.x++) {
		for(k.y = pos.y; k.y < pos.y + dim.y; k.y++) {
			if(lookup_hgt(k) > gib_grundwasser()) {
				return false;
			}
		}
	}
	return true;
}

bool
karte_t::ist_platz_frei(koord pos, sint16 w, sint16 h, int *last_y, climate_bits cl) const
{
	koord k;

	if(pos.x<0 || pos.y<0 || pos.x+w>=gib_groesse_x() || pos.y+h>=gib_groesse_y()) {
		return false;
	}

	grund_t *gr = lookup_kartenboden(pos);
	const sint16 platz_h = gr->gib_grund_hang() ? max_hgt(pos) : gr->gib_hoehe();	// remember the max height of the first tile

	// ACHTUNG: Schleifen sind mit finde_plaetze koordiniert, damit wir ein
	// paar Abfragen einsparen k�nnen bei h > 1!
	// V. Meyer
	for(k.y=pos.y+h-1; k.y>=pos.y; k.y--) {
		for(k.x=pos.x; k.x<pos.x+w; k.x++) {
			const grund_t *gr = lookup_kartenboden(k);

			// we can built, if: max height all the same, everything removable and no buildings there
			// since this is called very often, we us a trick:
			// if gib_grund_hang()!=0 (non flat) then we add 127 (bigger than any slope) and substract it
			// will break with double slopes!
#ifdef DOUBLE_GROUNDS
#error "Fix this function!"
#endif
			if(platz_h!=(gr->gib_hoehe()+Z_TILE_STEP*((gr->gib_grund_hang()+127)/128))  ||  !gr->ist_natur() ||  gr->kann_alle_obj_entfernen(NULL) != NULL  ||  (cl&(1<<get_climate(gr->gib_hoehe())))==0) {
				if(last_y) {
					*last_y = k.y;
				}
				return false;
			}
		}
	}
	return true;
}



slist_tpl<koord> *
karte_t::finde_plaetze(sint16 w, sint16 h, climate_bits cl) const
{
	slist_tpl<koord> * list = new slist_tpl<koord>();
	koord start;
	int last_y;

DBG_DEBUG("karte_t::finde_plaetze()","for size (%i,%i) in map (%i,%i)",w,h,gib_groesse_x(),gib_groesse_y() );
	for(start.x=0; start.x<gib_groesse_x()-w; start.x++) {
		for(start.y=0; start.y<gib_groesse_y()-h; start.y++) {
			if(ist_platz_frei(start, w, h, &last_y, cl)) {
				list->insert(start);
			}
			else {
				// Optimiert f�r gr��ere Felder, hehe!
				// Die Idee: wenn bei 2x2 die untere Reihe nicht geht, k�nnen
				// wir gleich 2 tiefer weitermachen! V. Meyer
				start.y = last_y;
			}
		}
	}
	return list;
}


/**
 * Play a sound, but only if near enoungh.
 * Sounds are muted by distance and clipped completely if too far away.
 *
 * @author Hj. Malthaner
 */
bool
karte_t::play_sound_area_clipped(koord pos, sound_info info)
{
	if(is_sound) {
		const int center = display_get_width() >> 7;
		const int dist = abs((pos.x-center) - ij_off.x) + abs((pos.y-center) - ij_off.y);

		if(dist < 25) {
			info.volume = 255-dist*9;
			sound_play(info);
		}
		return dist < 25;
	}
	return false;
}



void
karte_t::speichern(const char *filename,bool silent)
{
#ifndef DEMO
DBG_MESSAGE("karte_t::speichern()", "saving game to '%s'", filename);

	loadsave_t  file;

	display_show_load_pointer( true );
	if(!file.wr_open(filename,loadsave_t::save_mode,umgebung_t::objfilename)) {
		create_win(new news_img("Kann Spielstand\nnicht speichern.\n"), w_info, magic_none);
		dbg->error("karte_t::speichern()","cannot open file for writing! check permissions!");
	}
	else {
		speichern(&file,silent);
		const char *success = file.close();
		if(success) {
			static char err_str[512];
			sprintf( err_str, translator::translate("Error during saving:\n%s"), success );
			create_win( new news_img(err_str), w_time_delete, magic_none);
		}
		else {
			if(!silent) {
				create_win( new news_img("Spielstand wurde\ngespeichert!\n"), w_time_delete, magic_none);
				// update the filename, if no autosave
				einstellungen->setze_filename(filename);
			}
		}
	}
	display_show_load_pointer( false );
#endif
}


void
karte_t::speichern(loadsave_t *file,bool silent)
{
	bool needs_redraw = false;

DBG_MESSAGE("karte_t::speichern(loadsave_t *file)", "start");
	if(!silent) {
		display_set_progress_text(translator::translate("Saving map ..."));
		display_progress(0,gib_groesse_y());
		display_flush(IMG_LEER, 0, "", "", 0, 0);
	}

	// rotate the map until it can be saved
	for( int i=0;  i<4  &&  nosave;  i++  ) {
		nosave = false;
		rotate90();
		needs_redraw = true;
	}
	assert( !nosave );

	einstellungen->rdwr(file);

	file->rdwr_long(ticks, " ");
	file->rdwr_long(letzter_monat, " ");
	file->rdwr_long(letztes_jahr, "\n");

	for (weighted_vector_tpl<stadt_t*>::const_iterator i = stadt.begin(), end = stadt.end(); i != end; ++i) {
		(*i)->rdwr(file);
		if(silent) {
			INT_CHECK("saving");
		}
	}
DBG_MESSAGE("karte_t::speichern(loadsave_t *file)", "saved cities ok");

	for(int j=0; j<gib_groesse_y(); j++) {
		for(int i=0; i<gib_groesse_x(); i++) {
			access(i, j)->rdwr(this, file);
		}
		if(silent) {
			INT_CHECK("saving");
		}
		else {
			display_progress(j, gib_groesse_y());
			display_flush(IMG_LEER, 0, "", "", 0, 0);
		}
	}
DBG_MESSAGE("karte_t::speichern(loadsave_t *file)", "saved tiles");

	for(int j=0; j<(gib_groesse_y()+1)*(gib_groesse_x()+1); j++) {
		file->rdwr_byte(grid_hgts[j], "\n");
	}
DBG_MESSAGE("karte_t::speichern(loadsave_t *file)", "saved hgt");

	sint32 fabs = fab_list.count();
	file->rdwr_long(fabs, "\n");
	slist_iterator_tpl<fabrik_t*> fiter( fab_list );
	while(fiter.next()) {
		(fiter.get_current())->rdwr(file);
		if(silent) {
			INT_CHECK("saving");
		}
	}
DBG_MESSAGE("karte_t::speichern(loadsave_t *file)", "saved fabs");

	sint32 haltcount=haltestelle_t::gib_alle_haltestellen().count();
	file->rdwr_long(haltcount,"hc");
	slist_iterator_tpl<halthandle_t> iter (haltestelle_t::gib_alle_haltestellen());
	while(iter.next()) {
		iter.get_current()->rdwr( file );
	}
DBG_MESSAGE("karte_t::speichern(loadsave_t *file)", "saved stops");

	for(unsigned i=0;  i<convoi_array.get_count();  i++ ) {
		// one MUST NOT call INT_CHECK here or else the convoi will be broken during reloading!
		convoihandle_t cnv = convoi_array[i];
		cnv->rdwr(file);
	}
	if(silent) {
		INT_CHECK("saving");
	}
	file->wr_obj_id("Ende Convois");
DBG_MESSAGE("karte_t::speichern(loadsave_t *file)", "saved %i convois",convoi_array.get_count());

	for(int i=0; i<MAX_PLAYER_COUNT; i++) {
		spieler[i]->rdwr(file);
		if(silent) {
			INT_CHECK("saving");
		}
	}
DBG_MESSAGE("karte_t::speichern(loadsave_t *file)", "saved players");

	// centered on what?
	file->rdwr_delim("View ");
	sint32 dummy = ij_off.x;
	file->rdwr_long(dummy, " ");
	dummy = ij_off.y;
	file->rdwr_long(dummy, "\n");

	if(needs_redraw) {
		update_map();
	}
}



// just the preliminaries, opens the file, checks the versions ...
bool
karte_t::laden(const char *filename)
{
	bool ok=false;
	mute_sound(true);
	display_show_load_pointer(true);

#ifndef DEMO
	loadsave_t file;

	DBG_MESSAGE("karte_t::laden", "loading game from '%s'", filename);

	if(!file.rd_open(filename)) {

		if(  (sint32)file.get_version()==-1  ||  file.get_version()>loadsave_t::int_version(SAVEGAME_VER_NR, NULL, NULL )  ) {
			create_win( new news_img("WRONGSAVE"), w_info, magic_none );
		}
		else {
			create_win(new news_img("Kann Spielstand\nnicht laden.\n"), w_info, magic_none);
		}
	} else if(file.get_version() < 84006) {
		// too old
		create_win(new news_img("WRONGSAVE"), w_info, magic_none);
	}
	else {
/*
		event_t ev;
		ev.ev_class=EVENT_NONE;
		check_pos_win(&ev);
*/
DBG_MESSAGE("karte_t::laden()","Savegame version is %d", file.get_version());

		laden(&file);
		ok = true;
		file.close();
		create_win( new news_img("Spielstand wurde\ngeladen!\n"), w_time_delete, magic_none);
		setze_dirty();
	}
#endif
	einstellungen->setze_filename(filename);
	display_show_load_pointer(false);
	return ok;
}


// handles the actual loading
void karte_t::laden(loadsave_t *file)
{
	char buf[80];

	intr_disable();
	current_mouse_funk.funk = NULL;
	destroy_all_win();


	display_set_progress_text(translator::translate("Loading map ..."));
	display_progress(0, 100);	// does not matter, since fixed width
	display_flush(IMG_LEER, 0, "", "", 0, 0);

	destroy();

	// powernets zum laden vorbereiten -> tabelle loeschen
	powernet_t::neue_karte(this);

	// jetzt geht das laden los
	DBG_MESSAGE("karte_t::laden", "Fileversion: %d, %p", file->get_version(), einstellungen);
	einstellungen->rdwr(file);
	if(einstellungen->gib_allow_player_change()  &&  umgebung_t::use_timeline!=2) {
		// not locked => eventually switch off timeline settings, if explicitly stated
		einstellungen->setze_use_timeline( umgebung_t::use_timeline );
		DBG_DEBUG("karte_t::laden", "timeline: reset to %i", umgebung_t::use_timeline );
	}
	if(einstellungen->gib_beginner_mode()) {
		warenbauer_t::set_multiplier( umgebung_t::beginner_price_factor );
	}
	else {
		warenbauer_t::set_multiplier( 1000 );
	}

	// just an initialisation for the loading
	season = (2+letzter_monat/3)&3; // summer always zero
	snowline = einstellungen->gib_winter_snowline()*Z_TILE_STEP + grundwasser;
	current_mouse_funk.funk = NULL;

DBG_DEBUG("karte_t::laden", "einstellungen loaded (groesse %i,%i) timeline=%i beginner=%i",einstellungen->gib_groesse_x(),einstellungen->gib_groesse_y(),umgebung_t::use_timeline,einstellungen->gib_beginner_mode());

	// wird gecached, um den Pointerzugriff zu sparen, da
	// die groesse _sehr_ oft referenziert wird
	cached_groesse_gitter_x = einstellungen->gib_groesse_x();
	cached_groesse_gitter_y = einstellungen->gib_groesse_y();
	cached_groesse_max = max(cached_groesse_gitter_x,cached_groesse_gitter_y);
	cached_groesse_karte_x = cached_groesse_gitter_x-1;
	cached_groesse_karte_y = cached_groesse_gitter_y-1;
	x_off = y_off = 0;

	// Reliefkarte an neue welt anpassen
	reliefkarte_t::gib_karte()->setze_welt(this);

	//12-Jan-02     Markus Weber added
	grundwasser = einstellungen->gib_grundwasser();
	grund_besch_t::calc_water_level( this, height_to_climate );

DBG_DEBUG("karte_t::laden()","grundwasser %i",grundwasser);

	init_felder();

	// reinit pointer with new pointer object and old values
	zeiger = new zeiger_t(this, koord3d::invalid, spieler[0]);
	setze_maus_funktion(wkz_abfrage, skinverwaltung_t::fragezeiger->gib_bild_nr(0), Z_PLAN,  NO_SOUND, NO_SOUND );

	hausbauer_t::neue_karte();
	fabrikbauer_t::neue_karte(this);

DBG_DEBUG("karte_t::laden", "init felder ok");

	file->rdwr_long(ticks, " ");
	file->rdwr_long(letzter_monat, " ");
	file->rdwr_long(letztes_jahr, "\n");
	if(file->get_version()<86006) {
		letztes_jahr += umgebung_t::starting_year;
	}
	// set the current month count
	setze_ticks_bits_per_tag(einstellungen->gib_bits_per_month());
	current_month = letzter_monat + (letztes_jahr*12);
	season = (2+letzter_monat/3)&3; // summer always zero
	steps = 0;

DBG_MESSAGE("karte_t::laden()","savegame loading at tick count %i",ticks);
	recalc_average_speed();	// resets timeline

DBG_DEBUG("karte_t::laden", "init %i cities",einstellungen->gib_anzahl_staedte());
	stadt.clear();
	stadt.resize(einstellungen->gib_anzahl_staedte());
	for(int i=0; i<einstellungen->gib_anzahl_staedte(); i++) {
		stadt_t *s = new stadt_t(this, file);
		stadt.append( s, s->gib_einwohner(), 64 );
	}

	DBG_MESSAGE("karte_t::laden()","loading blocks");
	old_blockmanager_t::rdwr(this, file);

	DBG_MESSAGE("karte_t::laden()","loading tiles");
	for (int y = 0; y < gib_groesse_y(); y++) {
		for (int x = 0; x < gib_groesse_x(); x++) {
			if(file->is_eof()) {
				dbg->fatal("karte_t::laden()","Savegame file mangled (too short)!");
			}
			access(x, y)->rdwr(this, file);
		}
		display_progress(y, gib_groesse_y()+stadt.get_count()+256);
		display_flush(IMG_LEER, 0, "", "", 0, 0);
	}

DBG_MESSAGE("karte_t::laden()","loading grid");
	if(file->get_version()<99005) {
		for (int y = 0; y <= gib_groesse_y(); y++) {
			for (int x = 0; x <= gib_groesse_x(); x++) {
				sint32 hgt;
				file->rdwr_long(hgt, "\n");
				// old height step was 16!
				setze_grid_hgt(koord(x, y), (hgt*Z_TILE_STEP)/16 );
			}
		}
	}
	else {
		// hgt now bytes
		for( sint32 i=0;  i<(gib_groesse_y()+1)*(gib_groesse_x()+1);  i++  ) {
			file->rdwr_byte( grid_hgts[i], "\n" );
		}
	}

	if(file->get_version()<88009) {
		DBG_MESSAGE("karte_t::laden()","loading slopes from older version");
		// Hajo: load slopes for older versions
		// now part of the grund_t structure
		for (int y = 0; y < gib_groesse_y(); y++) {
			for (int x = 0; x < gib_groesse_x(); x++) {
				sint8 slope;
				file->rdwr_byte(slope, ",");
				access(x, y)->gib_kartenboden()->setze_grund_hang(slope);
			}
		}
	}

	if(file->get_version()<=88000) {
		// because from 88.01.4 on the foundations are handled differently
		for (int y = 0; y < gib_groesse_y(); y++) {
			for (int x = 0; x < gib_groesse_x(); x++) {
				koord k(x,y);
				if(access(x,y)->gib_kartenboden()->gib_typ()==grund_t::fundament) {
					access(x,y)->gib_kartenboden()->setze_hoehe( max_hgt(k) );
					access(x,y)->gib_kartenboden()->setze_grund_hang(hang_t::flach);
				}
			}
		}
	}

	// Reliefkarte an neue welt anpassen
	DBG_MESSAGE("karte_t::laden()", "init relief");
	win_setze_welt( this );
	reliefkarte_t::gib_karte()->setze_welt(this);

	sint32 fabs;
	file->rdwr_long(fabs, "\n");
	DBG_MESSAGE("karte_t::laden()", "prepare for %i factories", fabs);

	for(sint32 i = 0; i < fabs; i++) {
		// liste in gleicher reihenfolge wie vor dem speichern wieder aufbauen
		fabrik_t *fab = new fabrik_t(this, file);
		if(fab->gib_besch()) {
			fab_list.append( fab );
		}
		else {
			dbg->error("karte_t::laden()","Unknown fabrik skipped!");
		}
		if(i&7) {
			display_progress(gib_groesse_y()+(24*i)/fabs, gib_groesse_y()+stadt.get_count()+256);
			display_flush(IMG_LEER, 0, "", "", 0, 0);
		}
	}

	DBG_MESSAGE("karte_t::laden()", "clean up factories");
	slist_iterator_tpl<fabrik_t*> fiter ( fab_list );
	while(fiter.next()) {
		fiter.get_current()->laden_abschliessen();
	}

	// crossconnect all?
	if(umgebung_t::crossconnect_factories) {
	    DBG_MESSAGE("karte_t::laden()", "crossconnecting factories");
		slist_iterator_tpl <fabrik_t *> iter (this->fab_list);
		while( iter.next() ) {
			iter.get_current()->add_all_suppliers();
		}
	}
	display_progress(gib_groesse_y()+24, gib_groesse_y()+256+stadt.get_count());
	display_flush(IMG_LEER, 0, "", "", 0, 0);

DBG_MESSAGE("karte_t::laden()", "%d factories loaded", fab_list.count());

	// must be done after reliefkarte is initialized
	int x = gib_groesse_y() + 24;
	for (weighted_vector_tpl<stadt_t*>::const_iterator i = stadt.begin(), end = stadt.end(); i != end; ++i) {
		// old versions did not save factory connections
		if(file->get_version()<99014) {
			(*i)->verbinde_fabriken();
		}
		(*i)->laden_abschliessen();
		display_progress(x++, gib_groesse_y() + 256 + stadt.get_count());
		display_flush(IMG_LEER, 0, "", "", 0, 0);
	}

	// must resort them ...
	weighted_vector_tpl<stadt_t*> new_weighted_stadt(stadt.get_count() + 1);
	for (weighted_vector_tpl<stadt_t*>::const_iterator i = stadt.begin(), end = stadt.end(); i != end; ++i) {
		stadt_t* s = *i;
		new_weighted_stadt.append(s, s->gib_einwohner(), 64);
		INT_CHECK("simworld 1278");
	}
	swap(stadt, new_weighted_stadt);
	DBG_MESSAGE("karte_t::laden()", "cities initialized");

	// load linemanagement status (and lines)
	// @author hsiegeln
	if (file->get_version() > 82003  &&  file->get_version()<88003) {
		DBG_MESSAGE("karte_t::laden()", "load linemanagement");
		gib_spieler(0)->simlinemgmt.rdwr(this, file);
	}
	// end load linemanagement

	DBG_MESSAGE("karte_t::laden()", "load stops");
	// now load the stops
	// (the players will be load later and overwrite some values,
	//  like the total number of stops build (for the numbered station feature)
	if(file->get_version()>=99008) {
		sint32 halt_count;
		file->rdwr_long(halt_count,"hc");
		for(int i=0; i<halt_count; i++) {
			halthandle_t halt = haltestelle_t::create( this, file );
			if(halt->existiert_in_welt()) {
				halt->gib_besitzer()->halt_add(halt);
			}
			else {
				dbg->warning("karte_t::laden()", "could not restore stopnear %i,%i", halt->gib_init_pos().x, halt->gib_init_pos().y );
			}
		}
	}

	DBG_MESSAGE("karte_t::laden()", "load convois");
	while( true ) {
		file->rd_obj_id(buf, 79);

		// printf("'%s'\n", buf);

		if (strcmp(buf, "Ende Convois") == 0) {
			break;
		}
		else {
			convoi_t *cnv = new convoi_t(this, file);
			convoi_array.append( cnv->self, 16 );

			if(cnv->in_depot()) {
				grund_t * gr = lookup(cnv->gib_pos());
				depot_t *dep = gr ? gr->gib_depot() : 0;
				if(dep) {
					cnv->betrete_depot(dep);
				}
				else {
					dbg->error("karte_t::laden()", "no depot for convoi, blocks may now be wrongly reserved!");
					cnv->destroy();
				}
			}
			else {
				sync_add( cnv );
			}
		}
	}

	display_progress(gib_groesse_y()+24+stadt.get_count(), gib_groesse_y()+256+stadt.get_count());
	display_flush(IMG_LEER, 0, "", "", 0, 0);
DBG_MESSAGE("karte_t::laden()", "%d convois/trains loaded", convoi_array.get_count());

	// jetzt k�nnen die spieler geladen werden
	for(int i=0; i<MAX_PLAYER_COUNT; i++) {
		spieler[i]->rdwr(file);
		display_progress(gib_groesse_y()+24+stadt.get_count()+(i*3), gib_groesse_y()+256+stadt.get_count());
		display_flush(IMG_LEER, 0, "", "", 0, 0);
	}
	for(int i=0; i<MAX_PLAYER_COUNT-2; i++) {
		umgebung_t::automaten[i] = spieler[i+2]->is_active();
	}
DBG_MESSAGE("karte_t::laden()", "players loaded");

	// nachdem die welt jetzt geladen ist k�nnen die Blockstrecken neu
	// angelegt werden
	old_blockmanager_t::laden_abschliessen(this);
	DBG_MESSAGE("karte_t::laden()", "blocks loaded");

	file->rdwr_delim("View ");
	file->rdwr_long(mi, " ");
	file->rdwr_long(mj, "\n");
	DBG_MESSAGE("karte_t::laden()", "Setting view to %d,%d", mi,mj);
	if(ist_in_kartengrenzen(mi,mj)) {
		change_world_position( koord3d(mi,mj,min_hgt(koord(mi,mj))) );
	}
	else {
		change_world_position( koord3d(mi,mj,0) );
	}

	// right season for recalculations
	recalc_snowline();

DBG_MESSAGE("karte_t::laden()", "%d ways loaded",weg_t::gib_alle_wege().count());
	for (int y = 0; y < gib_groesse_y(); y++) {
		for (int x = 0; x < gib_groesse_x(); x++) {
			const planquadrat_t *plan = lookup(koord(x,y));
			const int boden_count = plan->gib_boden_count();
			for(int schicht=0; schicht<boden_count; schicht++) {
				grund_t *gr = plan->gib_boden_bei(schicht);
				for(int n=0; n<gr->gib_top(); n++) {
					ding_t *d = gr->obj_bei(n);
					if(d) {
						d->laden_abschliessen();
					}
				}
				gr->calc_bild();
			}
		}
		display_progress(gib_groesse_y()+48+stadt.get_count()+(y*128)/gib_groesse_y(), gib_groesse_y()+256+stadt.get_count());
		display_flush(IMG_LEER, 0, "", "", 0, 0);
	}

	// assing lines and other stuff for convois
	for(unsigned i=0;  i<convoi_array.get_count();  i++ ) {
		convoihandle_t cnv = convoi_array[i];
		cnv->laden_abschliessen();
	}

	// finish the loading of stops (i.e. assign the right good for these stops)
	for(  slist_tpl<halthandle_t>::const_iterator i=haltestelle_t::gib_alle_haltestellen().begin(); i!=haltestelle_t::gib_alle_haltestellen().end();  ) {
		if(  (*i)->gib_besitzer()==NULL  ||  !(*i)->existiert_in_welt()  ) {
			// this stop was only needed for loading goods ...
			halthandle_t h = (*i);
			++i;	// goto next
			haltestelle_t::destroy(h);	// remove from list
		}
		else {
			(*i)->laden_abschliessen();
			++i;
		}
	}

	// register all line stops and change line types, if needed
	for(int i=0; i<MAX_PLAYER_COUNT ; i++) {
		spieler[i]->laden_abschliessen();
	}

	// recalculate halt connections
	slist_iterator_tpl<halthandle_t>iter(haltestelle_t::gib_alle_haltestellen());
	int hnr=0, hmax=haltestelle_t::gib_alle_haltestellen().count();
	while(iter.next()) {
		if((hnr++%32)==0) {
			display_progress(gib_groesse_y()+48+stadt.get_count()+128+(hnr*80)/hmax, gib_groesse_y()+256+stadt.get_count());
			display_flush(IMG_LEER, 0, "", "", 0, 0);
		}
		iter.get_current()->rebuild_destinations();
	}

#if 1
	// reroute goods for benchmarking
	long dt = dr_time();
	iter.begin();
	while(iter.next()) {
		iter.get_current()->reroute_goods();
	}
	DBG_MESSAGE("reroute_goods()","for all haltstellen_t took %ld ms", dr_time()-dt );
#endif


	// make counter for next month
	ticks = ticks % karte_t::ticks_per_tag;
	next_month_ticks = karte_t::ticks_per_tag;
	letzter_monat %= 12;

	DBG_MESSAGE("karte_t::laden()","savegame from %i/%i, next month=%i, ticks=%i (per month=1<<%i)",letzter_monat,letztes_jahr,next_month_ticks,ticks,karte_t::ticks_bits_per_tag);

	reset_timer();
	recalc_average_speed();
	setze_dirty();
	mute_sound(false);

	setze_maus_funktion(wkz_abfrage, skinverwaltung_t::fragezeiger->gib_bild_nr(0), Z_PLAN,  NO_SOUND, NO_SOUND );
}


// recalcs all ground tiles on the map
void karte_t::update_map()
{
	for(  int i=0;  i<cached_groesse_gitter_x*cached_groesse_gitter_y;  i++  ) {
		const int boden_count = plan[i].gib_boden_count();
		for(int schicht=0; schicht<boden_count; schicht++) {
			grund_t *gr = plan[i].gib_boden_bei(schicht);
			gr->calc_bild();
		}
	}
	setze_dirty();
}


// return an index to a halt (or creates a new one)
// only used during loading
halthandle_t
karte_t::get_halt_koord_index(koord k)
{
	if(!ist_in_kartengrenzen(k)) {
		return halthandle_t();
	}
	// already there?
	const halthandle_t h=lookup(k)->gib_halt();
	if(!h.is_bound()) {
		// no => create
		return haltestelle_t::create( this, k, NULL );
	}
	return h;
}



int
karte_t::sp2num(spieler_t *sp)
{
	for(int i=0; i<MAX_PLAYER_COUNT; i++) {
		if(spieler[i] == sp) {
			return i;
		}
	}
	return -1;
}




/**
 * Creates a map from a heightfield
 * @param sets game settings
 * @author Hansj�rg Malthaner
 */
void karte_t::load_heightfield(einstellungen_t *sets)
{
  FILE *file = fopen(sets->heightfield, "rb");
  if(file) {
    char buf [256];
    int w, h;

    read_line(buf, 255, file);

    if(strncmp(buf, "P6", 2)) {
      dbg->error("karte_t::load_heightfield()","Heightfield has wrong image type %s", buf);
      create_win( new news_img("\nHeightfield has wrong image type.\n"), w_info, magic_none );
      return;
    }

    read_line(buf, 255, file);
    sscanf(buf, "%d %d", &w, &h);
    read_line(buf, 255, file);

    if(strncmp(buf, "255", 2)) {
      dbg->error("karte_t::load_heightfield()","Heightfield has wrong color depth %s", buf);
      create_win( new news_img("\nHeightfield has wrong image type.\n"), w_info, magic_none );
    }
		else {
			sets->setze_groesse(w,h);
			// create map
			init(sets);
			fclose(file);
		}
  }
  else {
    dbg->error("karte_t::load_heightfield()","Cant open file '%s'", (const char*)sets->heightfield);
    create_win( new news_img("\nCan't open heightfield file.\n"), w_info, magic_none );
  }
}


static void warte_auf_mausklick_oder_taste(event_t *ev)
{
	do {
		win_get_event(ev);
	}while( !(IS_LEFTRELEASE(ev)  ||  (ev->ev_class == EVENT_KEYBOARD  &&  (ev->ev_code >= 32  &&  ev->ev_code < 256) ) ) );
}



/**
 * marks an area using the grund_t mark flag
 * @author prissi
 */
void
karte_t::mark_area( const koord3d pos, const uint8 radius, const bool mark )
{
	for( sint16 y=pos.y-radius;  y<pos.y+radius+1;  y++  ) {
		for( sint16 x=pos.x-radius;  x<pos.x+radius+1;  x++  ) {
			grund_t *gr = lookup_kartenboden( koord(x,y) );
			if(gr) {
				if(mark) {
					gr->set_flag(grund_t::marked);
				}
				else {
					gr->clear_flag(grund_t::marked);
				}
				gr->set_flag(grund_t::dirty);
			}
		}
	}
}



void
karte_t::reset_timer()
{
	DBG_MESSAGE("karte_t::reset_timer()","called");
	// Reset timers
	uint32 last_tick_sync = dr_time();
	intr_set_last_time(last_tick_sync);
	intr_enable();

	if(fast_forward) {
		set_frame_time( 100 );
		time_multiplier = 16;
	}
	else {
		set_frame_time( 1000/umgebung_t::fps );
	}
}



// jump one year ahead
// (not updating history!)
void
karte_t::step_year()
{
	DBG_MESSAGE("karte_t::step_year()","called");
//	ticks += 12*karte_t::ticks_per_tag;
//	next_month_ticks += 12*karte_t::ticks_per_tag;
	current_month += 12;
	letztes_jahr ++;
	reset_timer();
}



void
karte_t::do_pause()
{
	display_fillbox_wh(display_get_width()/2-100, display_get_height()/2-50,
	                           200,100, MN_GREY2, true);
	display_ddd_box(display_get_width()/2-100, display_get_height()/2-50,
	                            200,100, MN_GREY4, MN_GREY0);
	display_ddd_box(display_get_width()/2-92, display_get_height()/2-42,
	                            200-16,100-16, MN_GREY0, MN_GREY4);

	display_proportional(display_get_width()/2, display_get_height()/2-5,
	                                 translator::translate("GAME PAUSED"),
	                                 ALIGN_MIDDLE, COL_BLACK, false);

	// Pause: warten auf die n�chste Taste
	event_t ev;
	display_flush_buffer();
	warte_auf_mausklick_oder_taste(&ev);
	reset_timer();
}

zeiger_t *
karte_t::gib_zeiger() const
{
    return zeiger;
}

void karte_t::bewege_zeiger(const event_t *ev)
{
	if(zeiger) {
		const int rw1 = get_tile_raster_width();
		const int rw2 = rw1/2;
		const int rw4 = rw1/4;

		int i_alt=zeiger->gib_pos().x;
		int j_alt=zeiger->gib_pos().y;

		int screen_y = ev->my - y_off - rw2 - ((display_get_width()/rw1)&1)*rw4;
		int screen_x = (ev->mx - x_off - rw2)/2;

		if(zeiger->gib_yoff() == Z_PLAN) {
			// already ok
		}
		else {
			// shifted by a quarter tile
			screen_y += rw4;
		}

		// berechnung der basis feldkoordinaten in i und j

		/*  this would calculate raster i,j koordinates if there was no height
		*  die formeln stehen hier zur erinnerung wie sie in der urform aussehen

		int base_i = (screen_x+screen_y)/2;
		int base_j = (screen_y-screen_x)/2;

		int raster_base_i = (int)floor(base_i / 16.0);
		int raster_base_j = (int)floor(base_j / 16.0);

		*/

		// iterative naeherung fuer zeigerposition
		// iterative naehreung an gelaendehoehe

		int hgt = lookup_hgt(koord(i_alt, j_alt));

		const int i_off = ij_off.x+gib_ansicht_ij_offset().x;
		const int j_off = ij_off.y+gib_ansicht_ij_offset().y;

		bool found = false;
		if(grund_t::underground_mode) {
			static sint8 found_hgt = grundwasser;

			for( hgt = grundwasser ; hgt < 32  && !found ; hgt++) {
				const int base_i = (screen_x+screen_y + tile_raster_scale_y((hgt*TILE_HEIGHT_STEP)/Z_TILE_STEP,rw1) )/2;
				const int base_j = (screen_y-screen_x + tile_raster_scale_y((hgt*TILE_HEIGHT_STEP)/Z_TILE_STEP,rw1))/2;

				mi = ((int)floor(base_i/(double)rw4)) + i_off;
				mj = ((int)floor(base_j/(double)rw4)) + j_off;

				const planquadrat_t *plan = lookup(koord(mi,mj));
				if(plan != NULL) {
					for( unsigned i=0;  plan != NULL && i<plan->gib_boden_count();  i++  ) {
						if(!plan->gib_boden_bei(i)->ist_tunnel() && hgt == plan->gib_boden_bei(i)->gib_hoehe()) {
							found = true;
							found_hgt = hgt;
						}
					}
				}
			}
			if(!found  &&  current_mouse_funk.funk!=tunnelbauer_t::baue) {
				zeiger->change_pos( koord3d::invalid );
				return;
			}
			hgt = found_hgt;
		}
		if(!found) {
			if(current_mouse_funk.funk!=tunnelbauer_t::baue) {
				hgt = zeiger->gib_pos().z;
			}

			for(int n = 0; n < 2 && !found; n++) {

				const int base_i = (screen_x+screen_y + tile_raster_scale_y((hgt*TILE_HEIGHT_STEP)/Z_TILE_STEP,rw1) )/2;
				const int base_j = (screen_y-screen_x + tile_raster_scale_y((hgt*TILE_HEIGHT_STEP)/Z_TILE_STEP,rw1))/2;

				mi = ((int)floor(base_i/(double)rw4)) + i_off;
				mj = ((int)floor(base_j/(double)rw4)) + j_off;

				const planquadrat_t *plan = lookup(koord(mi,mj));
				if(current_mouse_funk.funk==tunnelbauer_t::baue) {
					found = true;
				} else if(plan != NULL) {
					hgt = plan->gib_kartenboden()->gib_hoehe();
				}
				else {
					hgt = grundwasser;
				}
			}
		}

		// rueckwaerttransformation um die zielkoordinaten
		// mit den mauskoordinaten zu vergleichen
		int neu_x = ((mi-i_off) - (mj-j_off))*rw2;
		int neu_y = ((mi-i_off) + (mj-j_off))*rw4;

		neu_x += display_get_width()/2 + rw2;
		neu_y += rw1;


		// pr�fe richtung d.h. welches nachbarfeld ist am naechsten
		if(ev->mx-x_off < neu_x) {
			zeiger->setze_richtung(ribi_t::west);
		}
		else {
			zeiger->setze_richtung(ribi_t::nord);
		}


		// zeiger bewegen
		if(mi >= 0 && mj >= 0 && mi<gib_groesse_x() && mj<gib_groesse_y() && (mi != i_alt || mj != j_alt)) {

			i_alt = mi;
			j_alt = mj;

			koord3d pos = lookup(koord(mi,mj))->gib_kartenboden()->gib_pos();
			if(grund_t::underground_mode) {
				pos.z = hgt;
			}
			else {
				if(zeiger->gib_yoff()==Z_GRID) {
					pos.z = lookup_hgt(koord(mi,mj));
					// pos.z = max_hgt(koord(mi,mj));
				}
			}
			zeiger->change_pos(pos);
			// resend init message, if mouse button pressed to enable dragging
			if(is_dragging  &&  ev->button_state==0) {
				is_dragging = false;
				current_mouse_funk.last_pos = INIT;
				current_mouse_funk.funk(get_active_player(), this, INIT, current_mouse_funk.param);
			}
			else if(current_mouse_funk.funk!=NULL  &&  ev->ev_class==EVENT_DRAG  &&  current_mouse_funk.last_pos!=pos.gib_2d()) {
				is_dragging = true;
				current_mouse_funk.funk(get_active_player(), this, DRAGGING, current_mouse_funk.param);
				current_mouse_funk.last_pos = pos.gib_2d();
			}
		}
	}
}



/* goes to next active player */
void karte_t::switch_active_player(uint8 new_player)
{
	// cheat: play as AI
	bool renew_menu=false;

	if(new_player>=MAX_PLAYER_COUNT) {
		new_player = 0;
	}

	// no cheating allowed?
	if(!einstellungen->gib_allow_player_change()) {
		active_player_nr = 0;
		active_player = spieler[0];
		if(new_player!=0) {
			message_t::get_instance()->add_message(translator::translate("On this map, you are not\nallowed to change player!\n"), koord::invalid, message_t::problems, get_active_player()->get_player_nr(), IMG_LEER);
		}
	}
	else {
		renew_menu = (active_player_nr==1  ||  new_player==1);
		active_player_nr = new_player;
		active_player = spieler[new_player];
		char buf[512];
		sprintf(buf, translator::translate("Now active as %s.\n"), get_active_player()->gib_name() );
		message_t::get_instance()->add_message(buf, koord::invalid, message_t::warnings, get_active_player()->get_player_nr(), IMG_LEER);
	}

	// update menue entries (we do not want player1 to run anything)
	if(renew_menu) {
		static magic_numbers all_menu[6]= { magic_railtools, magic_monorailtools, magic_tramtools, magic_roadtools, magic_shiptools, magic_airtools };
		for( int i=0;  i<6;  i++  ) {
			gui_fenster_t *gui=win_get_magic(all_menu[i]);
			if(gui) {
				menu_fill( this, all_menu[i], active_player );
			}
		}
		setze_dirty();
	}

	// open edit tools
	if(active_player_nr==1) {
		gui_fenster_t *gui=win_get_magic(magic_edittools);
		if(!gui) {
			create_win( menu_fill( this, magic_edittools, active_player), w_info|w_no_overlap, magic_edittools );
		}
	}
	else {
		// close edit tools
		gui_fenster_t *gui=win_get_magic(magic_edittools);
		if(gui) {
			destroy_win(gui);
		}
	}

	setze_maus_funktion(wkz_abfrage, skinverwaltung_t::fragezeiger->gib_bild_nr(0), Z_PLAN,  NO_SOUND, NO_SOUND );
}



void
karte_t::interactive_event(event_t &ev)
{
	extern const weg_besch_t *default_track;
	extern const weg_besch_t *default_road;
	extern const way_obj_besch_t *default_electric;
	struct sound_info click_sound;

	click_sound.index = SFX_SELECT;
	click_sound.volume = 255;
	click_sound.pri = 0;

	if(ev.ev_class == EVENT_KEYBOARD) {
		DBG_MESSAGE("karte_t::interactive_event()","Keyboard event with code %d '%c'", ev.ev_code, ev.ev_code);

		switch(ev.ev_code) {
			case ',':
				if(time_multiplier>1) {
					time_multiplier--;
					sound_play(click_sound);
				}
				if(fast_forward) {
					fast_forward = false;
					reset_timer();
				}
				break;
			case '.':
				sound_play(click_sound);
				time_multiplier++;
				if(fast_forward) {
					fast_forward = false;
					reset_timer();
				}
				break;
			case '!':
				sound_play(click_sound);
				umgebung_t::show_names = (umgebung_t::show_names+1) & 3;
				setze_dirty();
				break;
			case '"':
				sound_play(click_sound);
				umgebung_t::hide_buildings ++;
				if(umgebung_t::hide_buildings>umgebung_t::ALL_HIDDEN_BUIDLING) {
					umgebung_t::hide_buildings = umgebung_t::NOT_HIDE;
				}
				umgebung_t::hide_trees = !umgebung_t::hide_trees;
				setze_dirty();
				break;
			case '#':
				sound_play(click_sound);
				grund_t::toggle_grid();
				setze_dirty();
				break;
			case 'U':
			{
				static magic_numbers all_menu_tools[10]= { magic_slopetools, magic_railtools, magic_monorailtools, magic_tramtools, magic_roadtools, magic_shiptools, magic_airtools, magic_specialtools, magic_edittools, magic_listtools };

				sound_play(click_sound);
				grund_t::toggle_underground_mode();
				for( int i=0;  i<10;  i++  ) {
					gui_fenster_t *gui=win_get_magic(all_menu_tools[i]);
					if(gui) {
						menu_fill( this, all_menu_tools[i], active_player );
					}
				}
				setze_dirty();
				update_map();
				break;
			}
			case 167:    // Hajo: '�'
				baum_t::distribute_trees( this, 3 );
				break;
			case 'a':
				setze_maus_funktion(wkz_abfrage, skinverwaltung_t::fragezeiger->gib_bild_nr(0), Z_PLAN,  NO_SOUND, NO_SOUND );
				break;
			case 'b':
				sound_play(click_sound);
				schiene_t::show_reservations = !schiene_t::show_reservations;
				if(schiene_t::show_reservations) {
					setze_maus_funktion(wkz_clear_reservation, skinverwaltung_t::signalzeiger->gib_bild_nr(0), Z_GRID,  NO_SOUND, NO_SOUND );
				}
				setze_dirty();
				break;
			case 'B':
				sound_play(click_sound);
				create_win( new message_frame_t(this), w_info, magic_messageframe );
				break;
			case 'c':
				sound_play(click_sound);
				display_snapshot();
				create_win( new news_img("Screenshot\ngespeichert.\n"), w_time_delete, magic_none);
				break;
			case 'C':
				sound_play(click_sound);
				setze_maus_funktion(wkz_add_city, skinverwaltung_t::stadtzeiger->gib_bild_nr(0), Z_GRID,  NO_SOUND, NO_SOUND );
				break;
			case 'd':
				setze_maus_funktion(wkz_lower, skinverwaltung_t::downzeiger->gib_bild_nr(0), Z_GRID,  NO_SOUND, NO_SOUND );
				break;
			case 'e':
				if(default_electric==NULL) {
					default_electric = wayobj_t::wayobj_search(track_wt,overheadlines_wt,get_timeline_year_month());
				}
				if(default_electric) {
					setze_maus_funktion(wkz_wayobj, default_electric->gib_cursor()->gib_bild_nr(0), Z_PLAN, (long)default_electric, SFX_JACKHAMMER, SFX_FAILURE);
				}
				break;
			case 'f': /* OCR: Finances */
				sound_play(click_sound);
				get_active_player()->zeige_info();
				break;
			case 'g':
				if(skinverwaltung_t::senke) {
					setze_maus_funktion(wkz_senke, skinverwaltung_t::undoc_zeiger->gib_bild_nr(0), Z_PLAN,  NO_SOUND, NO_SOUND );
				}
				break;
			case 'G':
				create_win( new goods_frame_t(this), w_info, magic_goodslist );
				break;
			case 'H':
				if (!hausbauer_t::headquarter.empty()) {
					setze_maus_funktion(wkz_headquarter, skinverwaltung_t::undoc_zeiger->gib_bild_nr(0), Z_PLAN, SFX_JACKHAMMER, SFX_FAILURE);
				}
				break;
			case 'I':
				if(einstellungen->gib_allow_player_change()) {
					setze_maus_funktion(wkz_build_industries_land, skinverwaltung_t::undoc_zeiger->gib_bild_nr(0), Z_PLAN,  NO_SOUND, NO_SOUND );
				}
				else {
					message_t::get_instance()->add_message(translator::translate("On this map, you are not\nallowed to change player!\n"), koord::invalid, message_t::problems, get_active_player()->get_player_nr(), IMG_LEER);
				}
				break;
			case 'J':
				create_win( new jump_frame_t(this), w_info, magic_jump);
				break;
			case 'k':
				create_win( 272, 160, new ki_kontroll_t(this), w_info, magic_ki_kontroll_t);
				break;
			case 'l':
				if(wegbauer_t::leitung_besch) {
					setze_maus_funktion(wkz_wegebau,
					wegbauer_t::leitung_besch->gib_cursor()->gib_bild_nr(0),
							Z_PLAN,
							(const void *)wegbauer_t::leitung_besch,
							SFX_JACKHAMMER,
							SFX_FAILURE );
				}
				break;
			case 'L':
				sound_play(click_sound);
				create_win( new loadsave_frame_t(this, true), w_info, magic_load_t);
				break;
			case 'm':
				create_win( new map_frame_t(this), w_info, magic_reliefmap);
				break;
			case 'M':
				setze_maus_funktion(wkz_marker, skinverwaltung_t::belegtzeiger->gib_bild_nr(0), Z_PLAN,  NO_SOUND, NO_SOUND );
				break;
			case 'P':
				sound_play(click_sound);
				switch_active_player( active_player_nr+1 );
				break;
			case 'p':
				sound_play(click_sound);
				do_pause();
				break;
			case 'Q':
				create_win( new optionen_gui_t(this), w_info, magic_optionen_gui_t );
				break;
			case 'r':
				setze_maus_funktion(wkz_remover, skinverwaltung_t::killzeiger->gib_bild_nr(0), Z_PLAN, SFX_REMOVER, SFX_FAILURE);
				break;
			case 'R':
				rotate90();
				update_map();
				break;
			case 's':
				if(default_road==NULL) {
					default_road = wegbauer_t::weg_search(road_wt,90,get_timeline_year_month(),weg_t::type_flat);
				}
				setze_maus_funktion(wkz_wegebau, default_road->gib_cursor()->gib_bild_nr(0), Z_PLAN, (long)default_road, SFX_JACKHAMMER, SFX_FAILURE);
				break;
			case 'S':
				sound_play(click_sound);
				create_win(new loadsave_frame_t(this, false), w_info, magic_save_t);
				break;
			case 't':
				if(default_track==NULL) {
					default_track = wegbauer_t::weg_search(track_wt,100,get_timeline_year_month(),weg_t::type_flat);
				}
				// may be NULL, if no track exists ...
				if(default_track!=NULL) {
					setze_maus_funktion(wkz_wegebau, default_track->gib_cursor()->gib_bild_nr(0), Z_PLAN,	(long)default_track, SFX_JACKHAMMER, SFX_FAILURE);
				}
				break;
			case 'T':
				sound_play(click_sound);
				create_win(new citylist_frame_t(this), w_info, magic_citylist_frame_t);
				break;
			case 'u':
				setze_maus_funktion(wkz_raise, skinverwaltung_t::upzeiger->gib_bild_nr(0), Z_GRID,  NO_SOUND, NO_SOUND );
				break;
			case 'v':
				umgebung_t::station_coverage_show = !umgebung_t::station_coverage_show;
				setze_dirty();
				break;
			case 'V':
				sound_play(click_sound);
				create_win( new convoi_frame_t(get_active_player()), w_info, magic_convoi_t);
				break;
			case 'W':
				fast_forward ^= 1;
				reset_timer();
				break;
			case 'w':
				sound_play(click_sound);
				get_active_player()->simlinemgmt.zeige_info(get_active_player());
				break;
			case 'X':
				sound_play(click_sound);
				destroy_all_win();
				beenden(false);
				break;
			case 'z':
				wkz_undo(get_active_player());
				break;

			case '+':
				display_set_light(display_get_light()+1);
				setze_dirty();
				break;
			case '-':
				display_set_light(display_get_light()-1);
				setze_dirty();
				break;

			// shortcut system
			case SIM_KEY_F2:
			case SIM_KEY_F3:
			case SIM_KEY_F4:
			case SIM_KEY_F5:
			case SIM_KEY_F6:
			case SIM_KEY_F7:
			case SIM_KEY_F8:
			case SIM_KEY_F9:
			case SIM_KEY_F10:
			case SIM_KEY_F11:
			case SIM_KEY_F12:
			case SIM_KEY_F13:
			case SIM_KEY_F14:
			case SIM_KEY_F15:
			{
				// save/recall commands
				int num = ev.ev_code - SIM_KEY_F2;
				save_mouse_func_t *& shortcut = quick_shortcuts[num];

				if (event_get_last_control_shift() == 2) {
					if (shortcut == NULL) {
						shortcut = new save_mouse_func_t;
					}
					*shortcut = current_mouse_funk;
				}
				else if (shortcut != NULL) {
						// gab es eine alte funktion ?
						if(current_mouse_funk.funk!=NULL) {
							current_mouse_funk.funk(get_active_player(), this, EXIT, current_mouse_funk.param);
						}
						DBG_MESSAGE("karte_t()","Recall mouse_funk");
						current_mouse_funk = *shortcut;
						current_mouse_funk.last_pos = koord::invalid;
						zeiger->setze_yoff(shortcut->zeiger_versatz);
						zeiger->setze_bild(shortcut->zeiger_bild);
						zeiger->set_flag(ding_t::dirty);

						current_mouse_funk.funk(get_active_player(), this, INIT, current_mouse_funk.param);
					}
				break;
			}

			// cursor movements
			case '9':
				ij_off += koord::nord;
				dirty = true;
				break;
			case '1':
				ij_off += koord::sued;
				dirty = true;
				break;
			case '7':
				ij_off += koord::west;
				dirty = true;
				break;
			case '3':
				ij_off += koord::ost;
				dirty = true;
				break;
			case '6':
			case SIM_KEY_RIGHT:
				ij_off += koord(+1,-1);
				dirty = true;
				break;
			case '2':
			case SIM_KEY_DOWN:
				ij_off += koord(+1,+1);
				dirty = true;
				break;
			case '8':
			case SIM_KEY_UP:
				ij_off += koord(-1,-1);
				dirty = true;
				break;
			case '4':
			case SIM_KEY_LEFT:
				ij_off += koord(-1,+1);
				dirty = true;
				break;

			// zoom
			case '>':
				win_set_zoom_factor(get_zoom_factor() > 1 ? get_zoom_factor()-1 : 1);
				setze_dirty();
				break;
			case '<':
				win_set_zoom_factor(get_zoom_factor() < 4 ? get_zoom_factor()+1 : 4);
				setze_dirty();
				break;

			// closing windows
			case 27:
			case 127:
				// close topmost win
				destroy_win( win_get_top() );
				break;
			case 8:
				sound_play(click_sound);
				destroy_all_win();
				break;

			case SIM_KEY_F1:
				create_win(new help_frame_t("general.txt"), w_info, magic_mainhelp);
				break;

			// just ignore the key
			case 0:
			case 13:
				break;

			// show help dialoge for unknown keys
			default:
				if (ev.ev_code <= 255) {
					DBG_MESSAGE("karte_t::interactive_event()","key `%c` is not bound to a function.",ev.ev_code);
					create_win(new help_frame_t("keys.txt"), w_info, magic_keyhelp);
				}
		}
	}

	if(IS_LEFTRELEASE(&ev)) {

		// in main top menu?
		if(ev.my < 32  &&  ev.cy<32) {

			switch( ev.mx/32 ) {
				case 0:
					sound_play(click_sound);
					create_win(240, 120, new optionen_gui_t(this), w_info,
					magic_optionen_gui_t);
					break;
				case 1:
					sound_play(click_sound);
					create_win( new map_frame_t(this), w_info, magic_reliefmap);
					break;
				case 2:
					setze_maus_funktion(wkz_abfrage, skinverwaltung_t::fragezeiger->gib_bild_nr(0), Z_PLAN,  NO_SOUND, NO_SOUND );
					break;
				case 3:
					if(!grund_t::underground_mode) {
						werkzeug_parameter_waehler_t *wzw = menu_fill( this, magic_slopetools, active_player );
						create_win( wzw, w_info|w_no_overlap, magic_slopetools );
					}
					break;
				case 4:
					{
						werkzeug_parameter_waehler_t *wzw = menu_fill( this, magic_railtools, active_player );
						if(wzw) {
							create_win( wzw, w_info|w_no_overlap, magic_railtools );
						}
					}
					break;
				case 5:
					{
						werkzeug_parameter_waehler_t *wzw = menu_fill( this, magic_monorailtools, active_player );
						if(wzw) {
							create_win( wzw, w_info|w_no_overlap, magic_monorailtools );
						}
					}
					break;
				case 6:
					{
						werkzeug_parameter_waehler_t *wzw = menu_fill( this, magic_tramtools, active_player );
						if(wzw) {
							create_win( wzw, w_info|w_no_overlap, magic_tramtools );
						}
					}
					break;
				case 7:
					{
						werkzeug_parameter_waehler_t *wzw = menu_fill( this, magic_roadtools, active_player );
						if(wzw) {
							create_win( wzw, w_info|w_no_overlap, magic_roadtools );
						}
					}
					break;
				case 8:
					{
						werkzeug_parameter_waehler_t *wzw = menu_fill( this, magic_shiptools, active_player );
						if(wzw) {
							create_win( wzw, w_info|w_no_overlap, magic_shiptools );
						}
					}
					break;
				case 9:
					{
						werkzeug_parameter_waehler_t *wzw = menu_fill( this, magic_airtools, active_player );
						if(wzw) {
							create_win( wzw, w_info|w_no_overlap, magic_airtools );
						}
					}
					break;
				case 10:
					{
						werkzeug_parameter_waehler_t *wzw = menu_fill( this, magic_specialtools, active_player );
						if(wzw) {
							create_win( wzw, w_info|w_no_overlap, magic_specialtools );
						}
					}
					break;
				case 11:
					setze_maus_funktion(wkz_remover, skinverwaltung_t::killzeiger->gib_bild_nr(0), Z_PLAN, SFX_REMOVER, SFX_FAILURE);
					break;
				case 12:
					// left empty
					break;
				case 13:
					sound_play(click_sound);
					get_active_player()->simlinemgmt.zeige_info( get_active_player() );
					break;
				case 14:
					{
						werkzeug_parameter_waehler_t *wzw = menu_fill( this, magic_listtools, active_player );
						if(wzw) {
							create_win( wzw, w_info|w_no_overlap, magic_listtools );
						}
					}
					break;
				case 15:
					// line info
					sound_play(click_sound);
					create_win( new message_frame_t(this), w_info, magic_messageframe );
					break;
				case 16:
					sound_play(click_sound);
					get_active_player()->zeige_info();
					break;
				case 18:
					sound_play(click_sound);
					display_snapshot();
					create_win( new news_img("Screenshot\ngespeichert.\n"), w_time_delete, magic_none);
					break;
				case 19:
					// Pause: warten auf die n�chste Taste
					do_pause();
					setze_dirty();
					break;
				case 20:
					fast_forward ^= 1;
					reset_timer();
					break;
				case 21:
					sound_play(click_sound);
					create_win(new help_frame_t("general.txt"), w_info, magic_mainhelp);
					break;
			}
		}
		else {
			// do some interactive building
			if(current_mouse_funk.funk != NULL) {
				koord pos (mi,mj);

DBG_MESSAGE("karte_t::interactive_event(event_t &ev)", "calling a tool");
				bool ok = false;
				if(current_mouse_funk.last_pos!=pos) {
					ok = current_mouse_funk.funk(get_active_player(), this, pos, current_mouse_funk.param);
				}
				if(ok) {
					if(current_mouse_funk.ok_sound!=NO_SOUND) {
						struct sound_info info = {current_mouse_funk.ok_sound,255,0};
						sound_play(info);
					}
				} else {
					if(current_mouse_funk.ko_sound!=NO_SOUND) {
						struct sound_info info = {current_mouse_funk.ko_sound,255,0};
						sound_play(info);
					}
				}
			 }
		}
	}

    // mouse wheel scrolled -> rezoom
    if (ev.ev_class == EVENT_CLICK) {
    	switch (ev.ev_code) {
    		case MOUSE_WHEELUP:
	  			win_set_zoom_factor(get_zoom_factor() > 1 ? get_zoom_factor()-1 : 1);
				  setze_dirty();
    		break;
    		case MOUSE_WHEELDOWN:
	  			win_set_zoom_factor(get_zoom_factor() < 4 ? get_zoom_factor()+1 : 4);
				  setze_dirty();
    		break;
    	}
  	}
    INT_CHECK("simworld 2117");
}



void
karte_t::beenden(bool b)
{
	finish_loop=false;
	umgebung_t::quit_simutrans = b;
}



bool
karte_t::interactive()
{
	event_t ev;
	finish_loop = true;
	bool swallowed = false;
	bool cursor_hidden = false;

	do {
		// get an event
		win_poll_event(&ev);

		if(ev.ev_class==EVENT_SYSTEM  &&  ev.ev_code==SYSTEM_QUIT) {
			// Beenden des Programms wenn das Fenster geschlossen wird.
			destroy_all_win();
			umgebung_t::quit_simutrans = true;
			return false;
		}

		if(ev.ev_class!=EVENT_NONE &&  ev.ev_class!=IGNORE_EVENT) {

			swallowed = check_pos_win(&ev);

			if(IS_RIGHTCLICK(&ev)) {
				display_show_pointer(false);
				cursor_hidden = true;
			} else if(IS_RIGHTRELEASE(&ev)) {
				display_show_pointer(true);
				cursor_hidden = false;
			} else if(!swallowed  &&  ev.ev_class==EVENT_DRAG  &&  ev.ev_code==MOUSE_RIGHTBUTTON) {
				// unset following
				if(follow_convoi.is_bound()) {
					follow_convoi = convoihandle_t();
				}
				blick_aendern(&ev);
			}
			else {
				if(cursor_hidden) {
					display_show_pointer(true);
					cursor_hidden = false;
				}
			}

			if(/*!swallowed  &&*/  (ev.ev_class==EVENT_DRAG  &&  ev.ev_code==MOUSE_LEFTBUTTON)  ||  (ev.button_state==0  &&  ev.ev_class==EVENT_MOVE)) {
				bewege_zeiger(&ev);
			}
		}

		if(fast_forward) {
			sync_step( MAGIC_STEP );
			step();
		}
		else {
			step();
		}

		// we wait here for maximum 9ms
		// average is 5 ms, so we usually
		// are quite responsive
		if(this_wait_time>0) {
			if(this_wait_time<10  ||  fast_forward) {
				dr_sleep( this_wait_time );
				this_wait_time = 0;
			}
			else {
				this_wait_time -= 5;
				dr_sleep( 5 );
			}
		}

	} while(ev.button_state != 0);

	if (!swallowed) {
		interactive_event(ev);
	}
	return finish_loop;
}
