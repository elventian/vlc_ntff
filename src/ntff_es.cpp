#include "ntff_es.h"
#include <vlc_block.h>

static es_out_id_t *ntff_es_add(es_out_t *ntff_out, const es_format_t *format)
{
	struct es_out_t *out = ntff_out->p_sys->out;
	es_out_id_t *res = out->pf_add(out, format);
	
	switch (format->i_cat)
	{
		case VIDEO_ES: ntff_out->p_sys->addVideo(res); break;
		case AUDIO_ES: ntff_out->p_sys->addAudio(res); break;
		default: break;
	}
	
	return res;
}

static int64_t auto_pcr = 0;
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
	
	static int frames_num = 0;
	if (cur_time < s->scene[s->cur_scene].begin)
	{
		block->i_flags |= BLOCK_FLAG_PREROLL;
		if (ntff_out->p_sys->isAudio(id))
		{
			return VLC_SUCCESS;
		}
	}
	else {
		if (ntff_out->p_sys->isVideo(id))
		{
			static int64_t prev_dts = 0;
			msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~ES_SEND_BLOCK_VIDEO: i_pts = %li, i_dts = %li, diff_dts = %li", 
				block->i_pts, block->i_dts, block->i_dts - prev_dts);
			prev_dts = block->i_dts;
			frames_num++;
		}
		else if (ntff_out->p_sys->isAudio(id))
		{
			static int64_t prev_dts = 0;
			msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~ES_SEND_BLOCK_AUDIO: i_pts = %li, i_dts = %li, diff_dts = %li", 
				block->i_pts, block->i_dts, block->i_dts - prev_dts);
			prev_dts = block->i_dts;
		}
	}
	if (cur_time > s->scene[s->cur_scene].end)
	{
		s->need_skip_scene = true;
		msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~ntff_es_send SKIP at %li, frames = %i", cur_time, frames_num);
		frames_num = 0;
		
		return VLC_SUCCESS;
	}
	
	block->i_dts = auto_pcr;
	block->i_pts = auto_pcr;
	struct es_out_t *out = ntff_out->p_sys->out;
	return out->pf_send(out, id, block);
}

static int ntff_es_control(es_out_t *ntff_out, int i_query, va_list va)
{
	struct es_out_t *out = ntff_out->p_sys->out;
	
	if (i_query == ES_OUT_SET_NEXT_DISPLAY_TIME)
	{
		return VLC_SUCCESS;
	}
	else if (i_query == ES_OUT_SET_PCR)
	{
		va_list tmp;
		va_copy(tmp, va);
		int64_t pcr = va_arg(tmp, int64_t);
		static int64_t prev_pcr = 0;
		msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~ES_OUT_SET_PCR %li, diff = %li", pcr, pcr - prev_pcr);	
		prev_pcr = pcr;
		
		auto_pcr += 25000;
		return es_out_Control(out, ES_OUT_SET_PCR, auto_pcr);
	}
	else {
		//msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~CONTROL with query = %i", i_query);	
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

es_out_sys_t::es_out_sys_t(demux_t *p_demux, scene_list *scenes, struct es_out_t *es):
	out(p_demux->out), p_demux(p_demux), scenes(scenes)
{
	es->pf_add = ntff_es_add;
	es->pf_send = ntff_es_send;
	es->pf_del = ntff_es_del;
	es->pf_control = ntff_es_control;
	es->pf_destroy = ntff_es_destroy;
}
