#include "ntff_es.h"
#include "ntff_player.h"
#include <vlc_block.h>
#include <vlc_es.h>
#include <functional>

#define BLOCK_FLAG_PRIVATE_SKIP_VIDEOBLOCK (5 << BLOCK_FLAG_PRIVATE_SHIFT)

namespace Ntff 
{

OutStream::OutStream(es_out_t *out, Player *player) : 
	curTime(0),
	out(out), player(player)
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
	curTime += player->getFrameLen();
	return curTime;
}

void OutStream::setTime(mtime_t time)
{
	curTime = time;
	resetFramesNum();
	es_out_Control(out, ES_OUT_RESET_PCR);
}

int OutStream::getHandledFrameNum() const
{
	if (framesQueue.empty()) return 0;
	else return *framesQueue.begin();
}

es_out_id_t *OutStream::addElemental(const es_format_t *format)
{
	EStreamType type = EStreamCollection::typeByVlcFormat(format);
	es_out_id_t *res = streams.getNext(type);
	
	if (!res)
	{
		res = out->pf_add(out, format);
		streams.append(res, type);
		//msg_Dbg(player->getVlcObj(), "~~~~addElemental %s", (type == Video ? "VIDEO" : "AUDIO"));
	}
	
	return res;
}

void OutStream::removeElemental(es_out_id_t *id)
{
	out->pf_del(out, id);
}

int OutStream::control(int i_query, va_list va)
{
	//msg_Dbg(player->getVlcObj(), "~~~~control query: %i", i_query);
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

void OutStream::addFrame(uint32_t frame)
{
	framesQueue.insert(frame);
	if (framesQueue.size() < 2) return;
	
	for (auto it = framesQueue.begin(); it != framesQueue.end(); it++)
	{
		auto next = it;
		next++;
		if (next == framesQueue.end() || (*it + 1 != *next))
		{
			framesQueue.erase(framesQueue.begin(), it);
			break;
		}		
	}
	//fix for cases when one frame is skipped by any reason
	if (framesQueue.size() > 10) { framesQueue.erase(framesQueue.begin()); }
}

int OutStream::sendBlock(es_out_id_t *streamId, block_t *block)
{
	mtime_t blockTime = (block->i_pts == 0) ? block->i_dts : block->i_pts;
	int curFrameId = player->getFrameId(blockTime);
	int frameInInterval = curFrameId - player->getCurIntervalFirstFrame();
	
	if (isVideo(streamId))
	{
		bool dup = framesQueue.count(frameInInterval) > 0;
		addFrame(frameInInterval);
		std::string frames;
		for (auto it = framesQueue.begin(); it != framesQueue.end(); it++)
		{
			frames += std::to_string(*it) + " ";
		}
		msg_Dbg(player->getVlcObj(), "sendBlock %s cur = %li frames: %s", dup? "DUPLICATE": "", block->i_pts, frames.c_str());
		
		block->i_dts = getTime();
		if (block->i_pts != 0)
		{
			block->i_pts = 0;
		}
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
			
			//msg_Dbg(player->getVlcObj(), "sendBlock blockTime = %li, frameInInterval = %i", blockTime, frameInInterval);
		}
	}
	else
	{
		if (isAudio(streamId))
		{
			return VLC_SUCCESS;
		}
		else if (isVideo(streamId))
		{
			block->i_pts = getTime() + frameInInterval;
			block->i_flags |= BLOCK_FLAG_PRIVATE_SKIP_VIDEOBLOCK;
			//msg_Dbg(player->getVlcObj(), "sendBlock SKIP_VIDEOBLOCK");
		}
	}
	
	return out->pf_send(out, streamId, block);
}

void EStreamCollection::reuse()
{
	for (int i = 0; i < EStreamTypeNum; i++)
	{
		streamIt[i] = streams[i].begin();
	}
}

es_out_id_t *EStreamCollection::getNext(EStreamType type)
{
	if (type == Unknown || streamIt[type] == streams[type].end()) { return nullptr; }
	else 
	{
		es_out_id_t *res = *streamIt[type];
		streamIt[type]++;
		return res;
	}
}

void EStreamCollection::append(es_out_id_t *id, EStreamType type)
{
	if (type == Unknown) return;
	
	streams[type].insert(id);
	streamIt[type] = streams[type].end();
}

EStreamType EStreamCollection::getType(es_out_id_t *stream) const
{
	for (int i = 0; i < EStreamTypeNum; i++)
	{
		if (streams[i].count(stream)) { return (EStreamType)i; }
	}
	return Unknown;
}

EStreamType EStreamCollection::typeByVlcFormat(const es_format_t *format)
{
	switch (format->i_cat)
	{
		case VIDEO_ES: return Video;
		case AUDIO_ES: return Audio;
		default: return Unknown;
	}
}

}
