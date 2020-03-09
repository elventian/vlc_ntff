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
#include <vlc_block.h>
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
	struct es_out_t es;
	stream_t *stream;	
	
	time_interval scenes[3];
	int scenes_num;
	int cur_scene;
	bool need_skip_scene;
};

struct es_out_sys_t
{
	struct es_out_t *out;
	demux_t *p_demux;
};

#include <vlc_es.h>
static es_out_id_t *ntff_es_add(es_out_t *ntff_out, const es_format_t *format)
{
	struct es_out_t *out = ntff_out->p_sys->out;
	es_out_id_t *res = out->pf_add(out, format);
	
	char *fstr = (format->i_cat == VIDEO_ES) ? "video" : (format->i_cat == AUDIO_ES) ? "audio" : "unknown";
	msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~ntff_es_add %s = %i", fstr, *((int *)res));
	return res;
}

static int ntff_es_send(es_out_t *ntff_out, es_out_id_t *id, block_t *block)
{
	msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~ntff_es_send (%i): i_pts = %li, i_dts = %li", 
		*((int *)id), block->i_pts, block->i_dts);
	
	
	int64_t cur_time = block->i_pts;
	if (cur_time == 0)
	{
		cur_time = block->i_dts;
	}
	
	demux_sys_t *d_sys = ntff_out->p_sys->p_demux->p_sys;
	if (cur_time > d_sys->scenes[d_sys->cur_scene].end)
	{
		d_sys->need_skip_scene = true;
		return VLC_SUCCESS;
	}
	
	struct es_out_t *out = ntff_out->p_sys->out;
	return out->pf_send(out, id, block);
}

static int ntff_es_control(es_out_t *ntff_out, int i_query, va_list va)
{
	struct es_out_t *out = ntff_out->p_sys->out;
	return out->pf_control(out, i_query, va);
}

static void ntff_es_del(es_out_t *ntff_out, es_out_id_t *id)
{
	struct es_out_t *out = ntff_out->p_sys->out;
	out->pf_del(out, id);
}

static void ntff_es_destroy(es_out_t *ntff_out)
{
	struct es_out_t *out = ntff_out->p_sys->out;
	out->pf_destroy(out);
}

static void ntff_register_es(demux_t *p_demux, struct es_out_t *es)
{
	es->pf_add = ntff_es_add;
	es->pf_send = ntff_es_send;
	es->pf_del = ntff_es_del;
	es->pf_control = ntff_es_control;
	es->pf_destroy = ntff_es_destroy;
	
	es->p_sys = calloc( 1, sizeof( es_out_sys_t ) );
	es->p_sys->out = p_demux->out;
	es->p_sys->p_demux = p_demux;
	
}

static int Demux( demux_t * p_demux )
{
    //msg_Dbg( p_demux, "~~~~NTFF Demux called");
	
	int64_t cur_time, pts_delay;
	demux_sys_t * p_sys = p_demux->p_sys;
	demux_Control(p_sys->fdemux, DEMUX_GET_TIME, &cur_time);
	demux_Control(p_sys->fdemux, DEMUX_GET_PTS_DELAY, &pts_delay);
	time_interval *cur_scene = &p_sys->scenes[p_sys->cur_scene];
	
	if (p_sys->cur_scene == p_sys->scenes_num - 1 && cur_time >= cur_scene->end)
	{
		msg_Dbg( p_demux, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTFF DONE");
		return VLC_DEMUXER_EOF;
	}
	
	static int seek_num = 0;
	static int calls = 0;
	
	if (0)
	//if (cur_time >= cur_scene->end)
	{
		mtime_t pi_system, pi_delay;
		mtime_t ppi_system, ppi_delay;
		
		es_out_ControlGetPcrSystem(p_demux->out, &ppi_system, &ppi_delay);
		//es_out_Control(p_demux->out, ES_OUT_MODIFY_PCR_SYSTEM, true, ppi_system + 6031513 /*cur_scene->end - cur_scene->begin*/);
		//es_out_Control(p_demux->out, ES_OUT_SET_PCR, ppi_system + cur_scene->end - cur_scene->begin);
		es_out_ControlGetPcrSystem(p_demux->out, &pi_system, &pi_delay);
		
		msg_Dbg( p_demux, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTFF PCR sys = %li - > %li, scene len = %li", 
			ppi_system, pi_system, cur_scene->end - cur_scene->begin);
		
		p_sys->cur_scene++;
		cur_scene = &p_sys->scenes[p_sys->cur_scene];
		
		msg_Dbg( p_demux, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTFF SEEK TO START of %i (%li -> %li)", 
			p_sys->cur_scene, cur_time, cur_scene->begin);
		
		//es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
		demux_Control(p_sys->fdemux, DEMUX_SET_TIME, cur_scene->begin, true);
		//es_out_Control(p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, cur_scene->begin);
		//msg_Dbg( p_demux, "~~~~~~~ES_OUT_MODIFY_PCR_SYSTEM res = %i", res);
		//es_out_ControlModifyPcrSystem( es_out_t *out, bool b_absolute, mtime_t i_system )
		//es_out_Control(p_demux->out, ES_OUT_SET_PCR, cur_scene->begin);
		//mtime_t time1, time2;
		//es_out_Control(p_demux->out, ES_OUT_GET_PCR_SYSTEM, &time1, &time2);
		//msg_Dbg( p_demux, "~~~~~~~~~~~ES_OUT_GET_PCR_SYSTEM = %li, %li", time1, time2);
	}
	
	/*if (((cur_time < cur_scene->begin - pts_delay)) && 
		!seek_num)
	{
		int res = demux_Control(p_sys->fdemux, DEMUX_SET_TIME, cur_scene->begin, true);
		es_out_Control(p_demux->out, ES_OUT_RESET_PCR);
		msg_Dbg( p_demux, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTFF SEEK TO START of %i (%li -> %li), res = %i", 
			p_sys->cur_scene, cur_time, cur_scene->begin, res);
		//demux_Control(p_sys->fdemux, DEMUX_GET_TIME, &cur_time);
		//seek_num++;
	}*/
	
	calls++;
	int demux_res = p_sys->fdemux->pf_demux(p_sys->fdemux);
	msg_Dbg( p_demux, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTFF Demux cur_time = %li", cur_time);
	return demux_res;
}

int64_t kdenlive_time(int h, int m, int s, int ms)
{
	return ((h * 60 * 60 + m * 60 + s) * 1000 + ms) * 1000;
}

static int Open(vlc_object_t *p_this)
{	
	demux_t *p_demux = (demux_t *)p_this;
	demux_sys_t *p_sys = calloc( 1, sizeof( demux_sys_t ) );
	p_demux->p_sys = p_sys;
	p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
	
	ntff_register_es(p_demux, &p_sys->es);
	p_sys->need_skip_scene = false;
	
//#define COLOR
//#define EXPANSE
#ifdef COLOR
	const char *mrl = "file:///home/elventian/colors-2397.mp4";
	const char *psz_filepath = "/home/elventian/colors-2397.mp4";
#else
#ifdef EXPANSE
	const char *mrl = "file:///home/elventian/Expanse.mkv";
	const char *psz_filepath = "/home/elventian/Expanse.mkv";
#else
	const char *mrl = "file:///home/elventian/s01e01_Something.Wicca.This.Way.Comes.avi";
	const char *psz_filepath = "/home/elventian/s01e01_Something.Wicca.This.Way.Comes.avi";
#endif
#endif
	const char *psz_name = "any";
	p_sys->stream = vlc_stream_NewMRL(p_this, mrl);
	
	
	msg_Dbg( p_demux, "~~~~NTFF Open = %s, %s", p_sys->stream->psz_name, 
		p_sys->stream->psz_filepath);
	
	p_sys->fdemux = demux_New(p_this, psz_name,
		psz_filepath, p_sys->stream, &p_sys->es);
	
	double fps;
	demux_Control(p_sys->fdemux, DEMUX_GET_FPS, &fps);
	double frame_len = 1000000 / fps;
	
	msg_Dbg( p_demux, "~~~~FPS = %f", frame_len);
	//msg_Dbg( p_demux, "~~~~FPS = %f", fps);
	
	p_sys->scenes_num = 3;
	p_sys->cur_scene = 0;
#ifdef COLOR	
	p_sys->scenes[0].begin = kdenlive_time(0, 0, 5, 5);
	p_sys->scenes[0].end   = p_sys->scenes[0].begin + kdenlive_time(0, 0, 0, 959) + frame_len;
	p_sys->scenes[1].begin = p_sys->scenes[0].end   + kdenlive_time(0, 0, 4, 4);
	p_sys->scenes[1].end   = p_sys->scenes[1].begin + kdenlive_time(0, 0, 0, 959) + frame_len;
	p_sys->scenes[2].begin = p_sys->scenes[1].end   + kdenlive_time(0, 0, 4, 4);
	p_sys->scenes[2].end   = p_sys->scenes[2].begin + kdenlive_time(0, 0, 0, 959) + frame_len;
#else	
#ifdef EXPANSE
	p_sys->scenes[0].begin = kdenlive_time(0, 8, 49, 40);
	p_sys->scenes[0].end   = p_sys->scenes[0].begin + kdenlive_time(0, 0, 3, 360) + frame_len;
	p_sys->scenes[1].begin = p_sys->scenes[0].end   + kdenlive_time(0, 2, 16, 400);
	p_sys->scenes[1].end   = p_sys->scenes[1].begin + kdenlive_time(0, 0, 6, 80) + frame_len;
	p_sys->scenes[2].begin = p_sys->scenes[1].end   + kdenlive_time(0, 28, 33, 920);
	p_sys->scenes[2].end   = p_sys->scenes[2].begin + kdenlive_time(0, 0, 3, 360) + frame_len;
	
	//p_sys->scenes[1].end   += 19 * frame_len;
	msg_Dbg( p_demux, "~~~~scene end = %li", p_sys->scenes[1].end);
#else	
	p_sys->scenes[0].begin = kdenlive_time(0, 11, 14, 400);
	p_sys->scenes[0].end   = p_sys->scenes[0].begin + kdenlive_time(0, 0, 2, 880) + frame_len;
	p_sys->scenes[1].begin = p_sys->scenes[0].end   + kdenlive_time(0, 13, 9, 840);
	p_sys->scenes[1].end   = p_sys->scenes[1].begin + kdenlive_time(0, 0, 2, 720) + frame_len;
	p_sys->scenes[2].begin = p_sys->scenes[1].end   + kdenlive_time(0, 15, 25, 0);
	p_sys->scenes[2].end   = p_sys->scenes[2].begin + kdenlive_time(0, 0, 4, 40) + frame_len;
	
#endif	
#endif	
	demux_Control(p_sys->fdemux, DEMUX_SET_TIME, p_sys->scenes[0].begin, true);
	
   /*int dres = vlc_dialog_wait_question(p_this, VLC_DIALOG_QUESTION_NORMAL, "cancel", "psz_action1",
   "psz_action2", "psz_title", "");*/
	
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
