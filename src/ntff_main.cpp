#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define DOMAIN  "vlc-myplugin"
#define _(str)  dgettext(DOMAIN, str)
#define N_(str) (str)
 
#include <stdlib.h>
#include <sstream>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_dialog.h>

#include "ntff_project.h"
#include "ntff_player.h"

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);
 
vlc_module_begin ()
    set_shortname ( "NTFF" )
    set_description( N_("NTFF demuxer" ) )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 90 )
    set_callbacks( Open, Close )
    add_shortcut( "ntff" )
vlc_module_end ()

struct demux_sys_t
{
	Ntff::Player *player;
};

static int Control(demux_t *p_demux, int i_query, va_list args)
{
	return p_demux->p_sys->player->control(i_query, args);
}

static int Demux(demux_t * p_demux)
{
	return p_demux->p_sys->player->play();
}

static int Open(vlc_object_t *p_this)
{	
	demux_t *p_demux = (demux_t *)p_this;
	
	demux_sys_t *p_sys = new demux_sys_t();
	p_demux->p_sys = p_sys;
	p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
		
	Ntff::Project project(p_this, p_demux->psz_file, p_demux->s);
	if (!project.isValid()) { return VLC_EGENERIC; }
	
	p_sys->player = project.createPlayer();
	if (!p_sys->player->isValid()) { return VLC_EGENERIC; }
	
	return VLC_SUCCESS;
}
 

static void Close(vlc_object_t *obj)
{
	demux_t *p_demux = (demux_t *)obj;
	delete p_demux->p_sys->player;
}
