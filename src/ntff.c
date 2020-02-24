#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define DOMAIN  "vlc-myplugin"
#define _(str)  dgettext(DOMAIN, str)
#define N_(str) (str)
 
#include <stdlib.h>
/* VLC core API headers */
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_input_item.h>
#include <vlc_dialog.h>
#include <vlc_demux.h>
#include <vlc_modules.h>
#include <vlc_stream_extractor.h>
 
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
 
/*static int playlist_item_appended(vlc_object_t *obj, const char *name, 
	vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
	playlist_item_t *item = (playlist_item_t *)newval.p_address;
	intf_thread_t *intf = (intf_thread_t *)p_data;
	
	msg_Info(intf, "playlist_item_appended %s, %s, %i", item->p_input->psz_name, item->p_input->psz_uri, 
		item->i_children);
	return VLC_SUCCESS;
}*/


static int Control( demux_t *p_demux, int i_query, va_list args )
{
	//msg_Dbg( p_demux, "~~~~NTFF Control called query = %i", i_query);
    
	bool *pb_bool; 
	int64_t *pi64;
	double *pf;
	
    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            return VLC_SUCCESS;

        case DEMUX_GET_META:
            return VLC_SUCCESS;

        case DEMUX_HAS_UNSUPPORTED_META:
            pb_bool = va_arg( args, bool* );
            *pb_bool = true;
            return VLC_SUCCESS;

        case DEMUX_SET_NEXT_DEMUX_TIME:
            return VLC_EGENERIC;

        case DEMUX_GET_TIME:
			pi64 = va_arg( args, int64_t * );
			*pi64 = 32 * 1000000;
			return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            return VLC_SUCCESS;

        case DEMUX_GET_ATTACHMENTS:
			return VLC_EGENERIC;

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            *pf = 0.0;
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            pi64 = va_arg( args, int64_t * );
            *pi64 = 42 * 1000000;
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE_INFO:
			return VLC_EGENERIC;
			
        case DEMUX_SET_TITLE:
			return VLC_EGENERIC;
			
        case DEMUX_SET_SEEKPOINT:
			return VLC_EGENERIC;

        default: return VLC_EGENERIC;
    }
}

typedef struct
{
	int64_t begin;
	int64_t end;
} time_interval;

struct demux_sys_t
{
	demux_t *fdemux;
	stream_t *stream;
	time_interval scenes[3];
	int scenes_num;
	int cur_scene;
};

static int Demux( demux_t * p_demux )
{
    //msg_Dbg( p_demux, "~~~~NTFF Demux called");
	
	int64_t cur_time, pts_delay;
	demux_Control(p_demux->p_sys->fdemux, DEMUX_GET_TIME, &cur_time);
	demux_Control(p_demux->p_sys->fdemux, DEMUX_GET_PTS_DELAY, &pts_delay);
	time_interval *cur_scene = &p_demux->p_sys->scenes[p_demux->p_sys->cur_scene];
	
	if (p_demux->p_sys->cur_scene == p_demux->p_sys->scenes_num - 1 && cur_time >= cur_scene->end)
	{
		msg_Dbg( p_demux, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTFF DONE");
		return VLC_DEMUXER_EOF;
	}
	
	static int seek_num = 0;
	static int calls = 0;
	
	if (cur_time >= cur_scene->end)
	{
		p_demux->p_sys->cur_scene++;
		cur_scene = &p_demux->p_sys->scenes[p_demux->p_sys->cur_scene];
		
		msg_Dbg( p_demux, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTFF SEEK TO START of %i (%li -> %li)", 
			p_demux->p_sys->cur_scene, cur_time, cur_scene->begin);
		
		es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
		es_out_Control(p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, cur_scene->begin);
		demux_Control(p_demux->p_sys->fdemux, DEMUX_SET_TIME, cur_scene->begin, true);
	}
	
	/*if (((cur_time < cur_scene->begin - pts_delay)) && 
		!seek_num)
	{
		int res = demux_Control(p_demux->p_sys->fdemux, DEMUX_SET_TIME, cur_scene->begin, true);
		es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
		msg_Dbg( p_demux, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTFF SEEK TO START of %i (%li -> %li), res = %i", 
			p_demux->p_sys->cur_scene, cur_time, cur_scene->begin, res);
		//demux_Control(p_demux->p_sys->fdemux, DEMUX_GET_TIME, &cur_time);
		//seek_num++;
	}*/
	
	calls++;
	int demux_res = p_demux->p_sys->fdemux->pf_demux(p_demux->p_sys->fdemux);
	msg_Dbg( p_demux, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTFF Demux cur_time = %li, res = %i", cur_time, demux_res);
	return demux_res;
}

int64_t kdenlive_time(int h, int m, int s, int ms)
{
	return ((h * 60 * 60 + m * 60 + s) * 1000 + ms) * 1000;
}

static int Open(vlc_object_t *p_this)
{	
	demux_t *p_demux = (demux_t *)p_this;
	p_demux->p_sys = calloc( 1, sizeof( demux_sys_t ) );
	p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
	
	const char *mrl = "file:///home/elventian/Expanse.mkv";
	const char *psz_name = "any";
	const char *psz_filepath = "/home/elventian/Expanse.mkv";
	p_demux->p_sys->stream = vlc_stream_NewMRL(p_this, mrl);
	
	
	msg_Dbg( p_demux, "~~~~NTFF Open = %s, %s", p_demux->p_sys->stream->psz_name, 
		p_demux->p_sys->stream->psz_filepath);
	
	p_demux->p_sys->fdemux = demux_New(p_this, psz_name,
		psz_filepath, p_demux->p_sys->stream, p_demux->out);
	
	p_demux->p_sys->scenes_num = 2;
	p_demux->p_sys->cur_scene = 0;
	
	p_demux->p_sys->scenes[0].begin = kdenlive_time(0, 8, 49, 040);
	p_demux->p_sys->scenes[0].end   = p_demux->p_sys->scenes[0].begin + kdenlive_time(0, 0, 03, 360);
	p_demux->p_sys->scenes[1].begin = p_demux->p_sys->scenes[0].end   + kdenlive_time(0, 2, 16, 400);
	p_demux->p_sys->scenes[1].end   = p_demux->p_sys->scenes[1].begin + kdenlive_time(0, 0, 2, 440);
	//3
	//p_demux->p_sys->scenes[1].begin = p_demux->p_sys->scenes[1].end + 52280 * 1000;
	//p_demux->p_sys->scenes[1].end   = p_demux->p_sys->scenes[1].begin + 17040 * 1000;
	//4
	//p_demux->p_sys->scenes[1].begin = p_demux->p_sys->scenes[1].end + 1702960 * 1000;
	//p_demux->p_sys->scenes[1].end   = p_demux->p_sys->scenes[1].begin + 3440 * 1000;
	
	//manual
	/*p_demux->p_sys->scenes[1].begin = 1708512689;
	p_demux->p_sys->scenes[1].end   = p_demux->p_sys->scenes[1].begin + 2000 * 1000;
	
	p_demux->p_sys->scenes[2].begin = 402450586;
	p_demux->p_sys->scenes[2].end   = p_demux->p_sys->scenes[2].begin + 2000 * 1000;*/
	
	
	//p_demux->p_sys->scenes[1].begin = 1534623355;
	//p_demux->p_sys->scenes[1].end   = 1572536279;
	
	demux_Control(p_demux->p_sys->fdemux, DEMUX_SET_TIME, p_demux->p_sys->scenes[0].begin, true);
	
	return VLC_SUCCESS;
	//return VLC_EGENERIC;
}
 

static void Close(vlc_object_t *obj)
{
	(void) obj;
    /*intf_thread_t *intf = (intf_thread_t *)obj;
    intf_sys_t *sys = intf->p_sys;
 
    msg_Info(intf, "Good bye!");*/
}
