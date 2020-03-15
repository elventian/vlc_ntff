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

#include "ntff.h"


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
 
static int Control( demux_t *p_demux, int i_query, va_list args )
{
	(void) p_demux;
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


struct demux_sys_t
{
	demux_t *fdemux;
	struct es_out_t es;
	stream_t *stream;	
	
	scene_list scenes;
};

static int Demux( demux_t * p_demux )
{
	//return VLC_DEMUXER_EOF;
    //msg_Dbg( p_demux, "~~~~NTFF Demux called");
	
	int64_t cur_time, pts_delay;
	demux_sys_t * p_sys = p_demux->p_sys;
	demux_Control(p_sys->fdemux, DEMUX_GET_TIME, &cur_time);
	demux_Control(p_sys->fdemux, DEMUX_GET_PTS_DELAY, &pts_delay);
	scene_list *s = &p_sys->scenes;
	
	if (s->need_skip_scene)
	{
		if (s->cur_scene == s->scenes_num - 1)
		{
			msg_Dbg( p_demux, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTFF DONE");
			return VLC_DEMUXER_EOF;
		}
		else 
		{
			s->need_skip_scene = false;
			s->cur_scene++;
			s->skipped_time += (s->scene[s->cur_scene].begin - s->scene[s->cur_scene-1].end);
			demux_Control(p_sys->fdemux, DEMUX_SET_TIME, s->scene[s->cur_scene].begin, true);
			
			msg_Dbg( p_demux, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTFF SEEK TO START of %i (%li -> %li)", 
				s->cur_scene, cur_time, s->scene[s->cur_scene].begin);
		}
	}
	
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
	
	ntff_register_es(p_demux, &p_sys->scenes, &p_sys->es);
	
	p_sys->scenes.need_skip_scene = false;
	p_sys->scenes.skipped_time = 0;
	
//#define COLOR
//#define EXPANSE
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
	
	p_sys->scenes.scenes_num = 3;
	p_sys->scenes.cur_scene = 0;
	time_interval *s = p_sys->scenes.scene;
#ifdef COLOR	
	s[0].begin = kdenlive_time(0, 0, 5, 5);
	s[0].end   = s[0].begin + kdenlive_time(0, 0, 0, 959) + frame_len;
	s[1].begin = s[0].end   + kdenlive_time(0, 0, 4, 4);
	s[1].end   = s[1].begin + kdenlive_time(0, 0, 0, 959) + frame_len;
	s[2].begin = s[1].end   + kdenlive_time(0, 0, 4, 4);
	s[2].end   = s[2].begin + kdenlive_time(0, 0, 0, 959) + frame_len;
#else	
#ifdef EXPANSE
	s[0].begin = kdenlive_time(0, 8, 49, 40);
	s[0].end   = s[0].begin + kdenlive_time(0, 0, 3, 360) + frame_len;
	s[1].begin = s[0].end   + kdenlive_time(0, 2, 16, 400);
	s[1].end   = s[1].begin + kdenlive_time(0, 0, 6, 80) + frame_len;
	s[2].begin = s[1].end   + kdenlive_time(0, 28, 33, 920);
	s[2].end   = s[2].begin + kdenlive_time(0, 0, 3, 360) + frame_len;
	
	//s[1].end   += 19 * frame_len;
	msg_Dbg( p_demux, "~~~~scene end = %li", s[1].end);
#else	
	s[0].begin = kdenlive_time(0, 11, 14, 400);
	s[0].end   = s[0].begin + kdenlive_time(0, 0, 2, 880) + frame_len;
	s[1].begin = s[0].end   + kdenlive_time(0, 13, 9, 840);
	s[1].end   = s[1].begin + kdenlive_time(0, 0, 2, 720) + frame_len;
	s[2].begin = s[1].end   + kdenlive_time(0, 15, 25, 0);
	s[2].end   = s[2].begin + kdenlive_time(0, 0, 4, 40) + frame_len;
	
#endif	
#endif	
	demux_Control(p_sys->fdemux, DEMUX_SET_TIME, s[0].begin, true);
	
   /*int dres = vlc_dialog_wait_question(p_this, VLC_DIALOG_QUESTION_NORMAL, "cancel", "psz_action1",
   "psz_action2", "psz_title", "");*/
	
	return VLC_SUCCESS;
}
 

static void Close(vlc_object_t *obj)
{
	(void) obj;
    /*intf_thread_t *intf = (intf_thread_t *)obj;
    intf_sys_t *sys = intf->p_sys;
 
    msg_Info(intf, "Good bye!");*/
}
