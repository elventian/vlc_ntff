#include "ntff.h"

#include <vlc_block.h>

struct es_out_sys_t
{
	struct es_out_t *out;
	scene_list *scenes;
	demux_t *p_demux;
};

es_out_id_t *audio_es = NULL;
static es_out_id_t *ntff_es_add(es_out_t *ntff_out, const es_format_t *format)
{
	struct es_out_t *out = ntff_out->p_sys->out;
	es_out_id_t *res = out->pf_add(out, format);
	
	const char *fstr = (format->i_cat == VIDEO_ES) ? "video" : (format->i_cat == AUDIO_ES) ? "audio" : "unknown";
	if (format->i_cat == AUDIO_ES && audio_es == NULL)
	{
		audio_es = res;
	}
	msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~ntff_es_add %s = %i", fstr, *((int *)res));
	return res;
}

static int ntff_es_send(es_out_t *ntff_out, es_out_id_t *id, block_t *block)
{
	scene_list *s = ntff_out->p_sys->scenes;

	int64_t cur_time;
	if (block->i_pts == 0)
	{
		cur_time = block->i_dts;
	}
	else
	{
		cur_time = block->i_pts;
		
	}
	
	cur_time = block->i_dts;
	
	static int frames_num = 0;
	if (cur_time < s->scene[s->cur_scene].begin)
	{
		block->i_flags |= BLOCK_FLAG_PREROLL;
		if (id == audio_es)
		{
			return VLC_SUCCESS;
		}
	}
	else {
		if (id != audio_es)
		{
			msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~ntff_es_send (%i): i_pts = %li, i_dts = %li, flags = 0x%x", 
				*((int *)id), block->i_pts, block->i_dts, block->i_flags);
			frames_num++;
		}
	}
	if (cur_time > s->scene[s->cur_scene].end)
	{
		s->need_skip_scene = true;
		msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~ntff_es_send SKIP at %li, frames = %i", cur_time, frames_num);
		frames_num = 0;
		
		return VLC_SUCCESS;
	}
	
	struct es_out_t *out = ntff_out->p_sys->out;
	return out->pf_send(out, id, block);
}

static int ntff_es_control(es_out_t *ntff_out, int i_query, va_list va)
{
	struct es_out_t *out = ntff_out->p_sys->out;
	
	//msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~CONTROL with query = %i", i_query);
	if (i_query == ES_OUT_SET_NEXT_DISPLAY_TIME)
	{
		return VLC_SUCCESS;
	}
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

void ntff_register_es(demux_t *p_demux, scene_list *scenes, struct es_out_t *es)
{
	es->pf_add = ntff_es_add;
	es->pf_send = ntff_es_send;
	es->pf_del = ntff_es_del;
	es->pf_control = ntff_es_control;
	es->pf_destroy = ntff_es_destroy;
	
	es->p_sys = new es_out_sys_t();
	es->p_sys->out = p_demux->out;
	es->p_sys->p_demux = p_demux;
	es->p_sys->scenes = scenes;
}
