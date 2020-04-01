#include "ntff_es.h"
#include "ntff_player.h"
#include <vlc_block.h>
#include <functional>

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

static int ntff_es_send(es_out_t *ntff_out, es_out_id_t *id, block_t *block)
{
	es_out_sys_t *es = ntff_out->p_sys;
	scene_list *s = es->scenes;

	mtime_t cur_time;
	if (block->i_pts == 0)
	{
		cur_time = block->i_dts;
	}
	else
	{
		cur_time = block->i_pts;
		
	}
	
	if (cur_time < s->scene[s->cur_scene].begin || cur_time > s->scene[s->cur_scene].end)
	{
		block->i_flags |= BLOCK_FLAG_PREROLL;
		if (es->isAudio(id))
		{
			return VLC_SUCCESS;
		}
	}
	
	
	/*if (es->isVideo(id))
	{
		bool preroll = (block->i_flags & BLOCK_FLAG_PREROLL) != 0;
		static mtime_t prev_dts = 0;
		msg_Dbg( es->p_demux, "~~~~~~~~~~ES_SEND_BLOCK_VIDEO: i_pts = %li, i_dts = %li, diff_dts = %li %s frame = %li", 
			block->i_pts, block->i_dts, block->i_dts - prev_dts, preroll ? " ===== PREROLL" : "",
			(long int )(block->i_pts / es->getFrameLen()));
		prev_dts = block->i_dts;
		if (!preroll) frames_num++;
	}
	else if (es->isAudio(id))
	{
		static mtime_t prev_dts = 0;
		msg_Dbg( es->p_demux, "~~~~~~~~~~ES_SEND_BLOCK_AUDIO: i_pts = %li, i_dts = %li, diff_dts = %li", 
			block->i_pts, block->i_dts, block->i_dts - prev_dts);
		prev_dts = block->i_dts;
		//if (s->cur_scene == 1) return VLC_SUCCESS;
	}*/
	
	
	if (es->isVideo(id))
	{
		block->i_dts = es->getTime();
		block->i_pts = 0;
		
		if ((block->i_flags & BLOCK_FLAG_PREROLL) == 0)
		{
			mtime_t t = es->updateTime();
			es_out_Control(es->out, ES_OUT_SET_PCR, t);
			
			uint32_t framesInScene = (s->scene[s->cur_scene].end - s->scene[s->cur_scene].begin) / 
				es->getFrameLen();
			
			if (es->getFramesNum() == framesInScene)
			{
				s->need_skip_scene = true;
				msg_Dbg( es->p_demux, "~~~~~~~~~~ntff_es_send SKIP at %li, scene_end = %li, frames = %i", 
					cur_time, s->scene[s->cur_scene].end, es->getFramesNum());
				es->resetFramesNum();
			}
		}
	}
	else if (es->isAudio(id))
	{
		block->i_dts = es->getTime();
		block->i_pts = es->getTime();
	}
	
	struct es_out_t *out = es->out;
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
		/*va_list tmp;
		va_copy(tmp, va);
		mtime_t pcr = va_arg(tmp, mtime_t);
		static mtime_t prev_pcr = 0;
		msg_Dbg( ntff_out->p_sys->p_demux, "~~~~~~~~~~ES_OUT_SET_PCR %li, diff = %li", pcr, pcr - prev_pcr);	
		prev_pcr = pcr;*/
		
		return VLC_SUCCESS;
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
	
	curTime = 0;
	framesNum = 0;
}

mtime_t es_out_sys_t::updateTime()
{
	framesNum++;
	curTime += frameLen;
	return curTime;
}


namespace Ntff 
{

OutStream::OutStream(es_out_t *out, Player *player) : curTime(0), framesNum(0), out(out), player(player)
{
	wrapper.p_sys = (es_out_sys_t *)this;
	
	wrapper.pf_add = [] (es_out_t *out, const es_format_t *format)
	{
		return ((OutStream*)out->p_sys)->addElemental(format);
	};
	
	wrapper.pf_send = [](es_out_t *out, es_out_id_t *id, block_t *block)
	{
		return ((OutStream*)out->p_sys)->sendBlock(id, block);
	};
	
	wrapper.pf_del = [](es_out_t *out, es_out_id_t *id)
	{
		((OutStream*)out->p_sys)->removeElemental(id);
	};
	
	wrapper.pf_control = [](es_out_t *out, int i_query, va_list va)
	{
		return ((OutStream*)out->p_sys)->control(i_query, va);
	};
	
	wrapper.pf_destroy = [](es_out_t *out)
	{
		((OutStream*)out->p_sys)->destroyOutStream();
	};
}

mtime_t OutStream::updateTime()
{
	framesNum++;
	curTime += player->getFrameLen();
	return curTime;
}

es_out_id_t *OutStream::addElemental(const es_format_t *format)
{
	es_out_id_t *res = out->pf_add(out, format);
	
	switch (format->i_cat)
	{
		case VIDEO_ES: video.insert(res); break;
		case AUDIO_ES: audio.insert(res); break;
		default: break;
	}
	
	return res;
}

void OutStream::removeElemental(es_out_id_t *id)
{
	out->pf_del(out, id);
}

int OutStream::control(int i_query, va_list va)
{
	if (i_query == ES_OUT_SET_NEXT_DISPLAY_TIME || i_query == ES_OUT_SET_PCR)
	{
		return VLC_SUCCESS;
	}
	else return out->pf_control(out, i_query, va);
}

void OutStream::destroyOutStream()
{
	out->pf_destroy(out);
}

int OutStream::sendBlock(es_out_id_t *streamId, block_t *block)
{
	msg_Dbg(player->getVlcObj(), "~~~~sendBlock");
	mtime_t blockTime = (block->i_pts == 0) ? block->i_dts : block->i_pts;
	
	if (isVideo(streamId))
	{
		block->i_dts = getTime();
		block->i_pts = 0;
	}
	else if (isAudio(streamId))
	{
		block->i_dts = block->i_pts = getTime();
	}
	
	if (player->timeIsInPlayInterval(blockTime))
	{
		if (isVideo(streamId))
		{
			mtime_t time = updateTime();
			es_out_Control(out, ES_OUT_SET_PCR, time);
			
			msg_Dbg(player->getVlcObj(), "~~~~sendBlock blockTime %li, video, frames: %i, time: %li", blockTime, framesNum, time);
		}
	}
	else
	{
		block->i_flags |= BLOCK_FLAG_PREROLL;
		if (isAudio(streamId))
		{
			return VLC_SUCCESS;
		}
		else {
			msg_Dbg(player->getVlcObj(), "~~~~sendBlock blockTime %li, video PREROLL, frames: %i", blockTime, framesNum);
		}
	}
	
	return out->pf_send(out, streamId, block);
}

}
