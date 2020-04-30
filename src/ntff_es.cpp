#include "ntff_es.h"
#include "ntff_player.h"
#include <vlc_block.h>
#include <vlc_es.h>
#include <vlc_codec.h>
#include <vlc_input.h>
#include <functional>
using atomic_bool = bool;
#include <input/input_internal.h>

#define BLOCK_FLAG_PRIVATE_SKIP_VIDEOBLOCK (5 << BLOCK_FLAG_PRIVATE_SHIFT)

struct es_out_pgrm_t;
struct es_out_id_t
{
    /* ES ID */
    int       i_id;
    es_out_pgrm_t *p_pgrm;

    /* */
    bool b_scrambled;

    /* Channel in the track type */
    int         i_channel;
    es_format_t fmt;
    char        *psz_language;
    char        *psz_language_code;

    decoder_t   *p_dec;
    decoder_t   *p_dec_record;

    /* Fields for Video with CC */
    struct
    {
        vlc_fourcc_t type;
        uint64_t     i_bitmap;    /* channels bitmap */
        es_out_id_t  *pp_es[64]; /* a max of 64 chans for CEA708 */
    } cc;

    /* Field for CC track from a master video */
    es_out_id_t *p_master;

    /* ID for the meta data */
    int         i_meta_id;
};

namespace Ntff 
{

OutStream::OutStream(es_out_t *out, Player *player) : 
	BaseStream(out, player)
{
	curTime = 0;
	outputEnabled = false;
	lastBlockTime = 0;
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

frame_id OutStream::getHandledFrameNum() const
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

void OutStream::addFrame(frame_id frame)
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
	frame_id curFrameId = player->getFrameId(blockTime);
	frame_id frameInInterval = curFrameId - player->getCurIntervalFirstFrame();
	
	if (isVideo(streamId))
	{
		if (outputEnabled) { addFrame(frameInInterval); }		
		block->i_dts = getTime();
		if (block->i_pts != 0)
		{
			block->i_pts = 0;
		}
		lastBlockTime = blockTime;
	}
	else if (isAudio(streamId))
	{
		block->i_dts = block->i_pts = getTime();
	}
	
	if (player->frameIsInPlayInterval(curFrameId))
	{
		if (isVideo(streamId))
		{
			mtime_t time = updateTime();
			es_out_Control(out, ES_OUT_SET_PCR, time);
			
			//msg_Dbg(player->getVlcObj(), "sendBlock blockTime = %li, curFrameId = %li", blockTime, curFrameId);
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
			//msg_Dbg(player->getVlcObj(), "sendBlock blockTime = %li, curFrameId = %li SKIP", blockTime, curFrameId);
		}
	}
	
	if (outputEnabled)
	{
		return out->pf_send(out, streamId, block);
	}
	else { return VLC_SUCCESS; }
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

PreloadVideoStream::PreloadVideoStream(es_out_t *out, Player *player) : BaseStream(out, player)
{
	videoStream = nullptr;
	wrapper.p_sys = (es_out_sys_t *)this;
	
	wrapper.pf_add = [] (es_out_t *out, const es_format_t *format)
	{
		return ((PreloadVideoStream*)out->p_sys)->addElemental(format);
	};
	wrapper.pf_send = [](es_out_t *out, es_out_id_t *id, block_t *block)
	{
		return ((PreloadVideoStream*)out->p_sys)->sendBlock(id, block);
	};
	wrapper.pf_control = [](es_out_t *out, int i_query, va_list va)
	{
		return ((PreloadVideoStream*)out->p_sys)->control(i_query, va);
	};
	
	wrapper.pf_del = [](es_out_t *, es_out_id_t *) {};	
	wrapper.pf_destroy = [](es_out_t *) {};
}

es_out_id_t *PreloadVideoStream::addElemental(const es_format_t *format)
{
	EStreamType type = EStreamCollection::typeByVlcFormat(format);
	if (type == Video)
	{
		if (!videoStream) {
			videoStream = out->pf_add(out, format); 
			
			/*input_DecoderNew( p_input, &p_es->fmt, p_es->p_pgrm->p_clock, input_priv(p_input)->p_sout );
			input_DecoderNew( input_thread_t *, es_format_t *, input_clock_t *,
			sout_instance_t * )*/
			//int current = var_GetInteger(player->getDemuxer()->p_input, "video-es");
			decoder = input_DecoderCreate(player->getVlcObj(), format, input_priv(player->getDemuxer()->p_input)->p_resource);
			//es_out_Control(out, ES_OUT_SET_ES_DEFAULT, videoStream);
			//decoder = videoStream->p_dec;
			//videoStream->p_dec = nullptr;
			decoder->pf_queue_video = []( decoder_t *, picture_t * ) { return 0; }; //disable video output
		}
		msg_Dbg(player->getVlcObj(), "PreloadVideoStream create dec = 0x%lx", (long int)decoder);
		//es_out_Control(out, ES_OUT_SET_ES, videoStream);
		return videoStream;
	}
	else return out->pf_add(out, format);
}

struct AVCodecContext;
struct AVCodec;
struct decoder_sys_tt
{
    AVCodecContext *p_context;
    const AVCodec  *p_codec;

    /* Video decoder specific part */
    date_t  pts;

    /* Closed captions for decoders */
    //cc_data_t cc;

    /* for frame skipping algo */
    bool b_hurry_up;
    bool b_show_corrupted;
    bool b_from_preroll;
    //enum AVDiscard i_skip_frame;

    /* how many decoded frames are late */
    int     i_late_frames;
    mtime_t i_late_frames_start;
    mtime_t i_last_late_delay;

    /* for direct rendering */
    bool        b_direct_rendering;
    //atomic_bool b_dr_failure;

    /* Hack to force display of still pictures */
    bool b_first_frame;


    /* */
    bool palette_sent;

    /* VA API */
    //vlc_va_t *p_va;
    //enum PixelFormat pix_fmt;
    int profile;
    int level;

    vlc_sem_t sem_mt;
};

int PreloadVideoStream::sendBlock(es_out_id_t *streamId, block_t *block)
{
	if (streamId == videoStream && !done)
	{
		mtime_t blockTime = (block->i_pts == 0) ? block->i_dts : block->i_pts;
		frame_id curFrameId = player->getFrameId(blockTime);
		if (curFrameId >= targetFrame - 1) { done = true; msg_Dbg(player->getVlcObj(), "PreloadVideoStream DONE");}
		else 
		{
			msg_Dbg(player->getVlcObj(), "PreloadVideoStream PROCESS block 0x%lx frame id = %li, target = %li", block, curFrameId, targetFrame);
			block->i_flags |= BLOCK_FLAG_PRIVATE_SKIP_VIDEOBLOCK;
			block->i_pts = 42;
			int res = decoder->pf_decode(decoder, block);
			//((decoder_sys_tt *)decoder->p_sys)->pts.date
			msg_Dbg(player->getVlcObj(), "PreloadVideoStream decode res = %s",  (res == 0? "ok":"error"));
		}
	}
	else {
		msg_Dbg(player->getVlcObj(), "PreloadVideoStream discard stream");
	}
	return VLC_SUCCESS;
}

int PreloadVideoStream::control(int i_query, va_list va)
{
	if (i_query == ES_OUT_GET_ES_STATE)
	{
		es_out_id_t *streamId = va_arg(va, es_out_id_t *);
		*va_arg(va, bool *) = (streamId == videoStream);
	}	
	else if (i_query == ES_OUT_SET_ES_DEFAULT)
	{
		es_out_id_t *es = va_arg( va, es_out_id_t * );
		if (videoStream == es)
		{
			msg_Dbg(player->getVlcObj(), "PreloadVideoStream ES_OUT_SET_ES_DEFAULT 0x%lx", (long int)es);
		}
	}
	else if (i_query == ES_OUT_SET_ES_CAT_POLICY || i_query == ES_OUT_SET_NEXT_DISPLAY_TIME) {}
	else {msg_Dbg(player->getVlcObj(), "PreloadVideoStream control %i", i_query);}

	return VLC_SUCCESS;
}

void PreloadVideoStream::setTargetTime(mtime_t time) 
{
	targetFrame = player->getFrameId(time);
	done = false;
}

}
