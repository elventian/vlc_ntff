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
	vlc_sem_t demuxSem;
	bool dialogActive;
};

static int Control(demux_t *p_demux, int i_query, va_list args)
{
	return p_demux->p_sys->player->control(i_query, args);
}

static int Demux(demux_t * p_demux)
{
	demux_sys_t *p_sys = p_demux->p_sys;
	if (p_sys->dialogActive) { vlc_sem_wait(&p_sys->demuxSem); } 
	return p_sys->player->play();
}

#include <vlc_extensions.h>


static int ActionEvent(vlc_object_t *obj, char const *, vlc_value_t, vlc_value_t, void *)
{
	msg_Dbg(obj, "~~~~~ActionEvent");
	return VLC_SUCCESS;
}


static int DialogCallback( vlc_object_t *p_this,char const *, vlc_value_t, vlc_value_t newval, void *)
{
    extension_dialog_command_t *command = (extension_dialog_command_t *)newval.p_address;
	msg_Dbg( command->p_dlg->p_object, "~~~~~DialogCallback");
	if (command == NULL || command->p_dlg == NULL) { return VLC_SUCCESS; }

    extension_widget_t *p_widget = (extension_widget_t *)command->p_data;

    switch( command->event )
    {
        case EXTENSION_EVENT_CLICK:
		if (p_widget == NULL) { return VLC_SUCCESS; }
            msg_Dbg( p_this, "~~~~~Received EXTENSION_EVENT_CLICK");
            break;
        case EXTENSION_EVENT_CLOSE:
			msg_Dbg( p_this, "~~~~~~Received EXTENSION_EVENT_CLOSE %s", command->p_dlg->widgets.p_elems[2]->psz_text);
			((demux_t *)p_this)->p_sys->dialogActive = false;
			vlc_sem_post(&((demux_t *)p_this)->p_sys->demuxSem);
            break;
        default:
            msg_Dbg( p_this, "~~~~~~Received unknown UI event %d, discarded",
                     command->event );
            break;
    }

    return VLC_SUCCESS;
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
	
	vlc_sem_init(&p_sys->demuxSem, 0);
	
	/***** */
	extension_dialog_t *dialog = new extension_dialog_t();
	dialog->p_object = p_this;
	dialog->psz_title = (char*)"Ntff Settings";
	dialog->i_width = 400;
	dialog->i_height = 800;
	dialog->b_hide = false;
	dialog->p_sys = NULL;
	dialog->b_kill = false;
	dialog->p_sys_intf = NULL;
	dialog->widgets.i_size = 3;
	dialog->widgets.p_elems = new extension_widget_t *[dialog->widgets.i_size];
	
	extension_widget_t *widget = new extension_widget_t();
	widget->type = EXTENSION_WIDGET_LABEL;
	widget->psz_text = (char*)"Text. May be NULL or modified by the UI";
	widget->p_dialog = dialog;
	dialog->widgets.p_elems[0] = widget;
	
	widget = new extension_widget_t();
	widget->type = EXTENSION_WIDGET_BUTTON;
	widget->psz_text = (char*)"Button text";
	widget->p_dialog = dialog;
	dialog->widgets.p_elems[1] = widget;
	
	
	widget = new extension_widget_t();
	widget->type = EXTENSION_WIDGET_DROPDOWN;
	widget->psz_text = new char[50];
	strcpy(widget->psz_text, "value2");
	widget->p_values = new extension_widget_t::extension_widget_value_t[2];
	int combo_id = 0;
	widget->p_values[0].i_id = combo_id++;
	widget->p_values[0].psz_text = new char[50];
	strcpy(widget->p_values[0].psz_text , "value1");
	widget->p_values[0].b_selected = false;
	widget->p_values[0].p_next = &widget->p_values[1];
	
	widget->p_values[1].i_id = combo_id++;
	widget->p_values[1].psz_text = new char[50];
	strcpy(widget->p_values[1].psz_text , "value2");
	widget->p_values[1].b_selected = true;
	widget->p_values[1].p_next = NULL;
	
	widget->p_dialog = dialog;
	dialog->widgets.p_elems[2] = widget;
	
	p_sys->dialogActive = true;
	vlc_ext_dialog_update(p_this, dialog);
	
	var_Create( p_this, "dialog-event", VLC_VAR_ADDRESS );
	var_AddCallback(p_this, "dialog-event", DialogCallback, NULL);
	//var_AddCallback( p_this->obj.libvlc, "key-action", ActionEvent, nullptr);
	
	return VLC_SUCCESS;
}
 

static void Close(vlc_object_t *obj)
{
	demux_t *p_demux = (demux_t *)obj;
	delete p_demux->p_sys->player;
}
