#include "ntff_player.h"
#include "ntff_es.h"
#include "ntff_feature.h"
#include "ntff_dialog.h"
#include <vlc_stream_extractor.h>
#include <vlc_demux.h>
#include <vlc_actions.h>
#include <vlc_input.h>
#include <vlc_threads.h>
#include <vlc_codec.h>
#include <vlc_block.h>
#include <utility>
using namespace std;
#include <atomic>
struct input_clock_t;
struct decoder_owner_sys_t
{
    input_thread_t  *p_input;
    input_resource_t*p_resource;
    input_clock_t   *p_clock;
    int             i_last_rate;

    vout_thread_t   *p_spu_vout;
    int              i_spu_channel;
    int64_t          i_spu_order;

    sout_instance_t         *p_sout;
    sout_packetizer_input_t *p_sout_input;

    vlc_thread_t     thread;

    void (*pf_update_stat)( decoder_owner_sys_t *, unsigned decoded, unsigned lost );

    /* Some decoders require already packetized data (ie. not truncated) */
    decoder_t *p_packetizer;
    bool b_packetizer;

    /* Current format in use by the output */
    es_format_t    fmt;

    /* */
    bool           b_fmt_description;
    vlc_meta_t     *p_description;
    atomic_int     reload;

    /* fifo */
    block_fifo_t *p_fifo;
	/* not interested in next members*/
};

namespace Ntff {

static int ActionEvent( vlc_object_t *, char const *, vlc_value_t, vlc_value_t newval, void *p_data )
{
	if (newval.i_int == ACTIONID_INTF_TOGGLE_FSC)
	{
		Player *player = (Player *)p_data;
		msg_Dbg(player->getVlcObj(), "ActionEvent");
		player->showDialog();
	}
	return VLC_SUCCESS;
}

Player::Player(demux_t *obj, FeatureList *featureList, double fps) : 
	obj(obj), featureList(featureList), fps(fps)
{
	demux_t *demuxer = (demux_t *)obj;
	out = new OutStream(demuxer->out, this);
	preload = new PreloadVideoStream(demuxer->out, this, out);
	intervalsSelected = false;
	length = 0;
	curInterval = playIntervals.begin();
	vlc_mutex_init(&intervalsMutex);
	dialog = new Dialog(this, featureList);
	var_AddCallback( obj->obj.libvlc, "key-action", ActionEvent, this);
}

Player::~Player()
{
	delete featureList;
	delete out;
	delete preload;
	delete dialog;
	vlc_mutex_destroy(&intervalsMutex);
	var_DelCallback( obj->obj.libvlc, "key-action", ActionEvent, this);
}

bool Player::isValid() const
{
	if (items.empty()) return false;
	
	for (auto it = items.begin(); it != items.end(); it++)
	{
		if (!(*it).second.isValid()) return false;
	}
	
	return true;
}

void Player::addFile(const Interval &interval, const std::string &filename)
{
	out->reuseStreams();
	items[interval.in] = Item(this, interval, out->getWrapperStream(), preload, filename);
	length = wholeDuration = interval.out;
	playIntervals[0] = Interval(0, wholeDuration);
	curInterval = playIntervals.begin();
	needInitItems = true;
}

bool Player::frameIsInPlayInterval(frame_id frame) const
{
	return getCurInterval().contains(frame);
}

mtime_t Player::getCurOffset() const
{
	const Item *item = getCurItem();
	if (!item) return 0;
	else
	{
		return item->getFirstFrameOffset();
	}
}

void Player::prepareNextInterval()
{
	Interval nextInterval = getNextInterval();
	if (nextInterval.length() > 0)
	{
		getItemAt(nextInterval.in)->prepare(nextInterval.in);
	}
}

int Player::getFrameId(mtime_t timeInItem) const
{
	return round((double)(timeInItem - getCurOffset()) / getFrameLen());
}

frame_id Player::getCurIntervalFirstFrame() const
{
	return getCurInterval().in - getCurItem()->getInterval().in;
}

void Player::setIntervalsSelected()
{
	Interval &newInterval = curInterval->second;
	frame_id targetFrame = newInterval.contains(savedFrameId) ? savedFrameId : newInterval.in;
	
	msg_Dbg(obj, "Seek to %li", targetFrame);
	seek(targetFrame, getStreamFrameByGlobal(targetFrame));
	prepareNextInterval();

	intervalsSelected = true;
	setPause(false);
}

void Player::showDialog() 
{
	setPause(true);
	intervalsSelected = false; 
	savedFrameId = getGlobalFrame();
	dialog->show();
}

void Player::hideDialog()
{
	dialog->hide();
}

frame_id Player::getGlobalFrame() const
{
	return getCurInterval().in + out->getHandledFrameId();
}

void Player::lockIntervals(bool lock)
{
	if (lock) { vlc_mutex_lock(&intervalsMutex); }
	else {vlc_mutex_unlock(&intervalsMutex);}
}

void Player::resetIntervals(bool empty)
{
	playIntervals.clear();
	if (!empty)
	{
		Interval res(0, wholeDuration);
		playIntervals[res.in] = res;
	}
}

void Player::modifyIntervals(bool add, const Feature *f, 
	int8_t minIntensity, int8_t maxIntensity, bool affectUnmarked)
{
	auto modify = add ? &FeatureList::insertInterval : &FeatureList::removeInterval;
	
	const std::vector<Interval> &featureIntervals = f->getIntervals();
	for (const Interval &interval: featureIntervals)
	{
		if (interval.intensity >= minIntensity && interval.intensity <= maxIntensity)
		{
			modify(playIntervals, interval);
		}
	}
	if (affectUnmarked)
	{
		frame_id prevOut = 0; 
		for (const Interval &interval: featureIntervals)
		{
			msg_Dbg(obj, "%s %li - %li", add ? "add" : "remove", prevOut, interval.in);
			modify(playIntervals, Interval(prevOut, interval.in));
			prevOut = interval.out;
		}
		msg_Dbg(obj, "%s %li - %li", add ? "add" : "remove", prevOut, wholeDuration);
		modify(playIntervals, Interval(prevOut, wholeDuration));
	}
}

void Player::recalcLength()
{
	length = 0;
	for (auto &p: playIntervals)
	{
		length += p.second.length();
	}
	msg_Dbg(obj, "Player Intervals (%li)", playIntervals.size());
	for (auto p: playIntervals)
	{
		msg_Dbg(obj, "~~~~interval: %li - %li", p.second.in, p.second.out);
	}
}

void Player::updateCurrentInterval()
{
	if (savedFrameId)
	{
		auto closestIt = playIntervals.lower_bound(savedFrameId);
		if (closestIt == playIntervals.end()) 
		{
			closestIt = playIntervals.begin(); 
		}
		else if (closestIt != playIntervals.begin())
		{
			closestIt--; //select prev interval, if it contains our timestamp
			if (closestIt->second.out <= savedFrameId)
			{
				closestIt++;
			}
		}
		curInterval = closestIt;
	}
	else { curInterval = playIntervals.begin(); }
}

frame_id Player::getStreamLengthTo(frame_id targetFrame) const
{
	frame_id res = 0;
	for (auto &p: playIntervals)
	{
		if (p.first >= targetFrame) return res;
		res += p.second.length();
	}
	return res;
}

decoder_t *Player::getVideoDecoder() const
{
	int currentVideoTrack = var_GetInteger(obj->p_input, "video-es");
	decoder_t *videoDecoder;
	input_GetEsObjects(obj->p_input, currentVideoTrack, (vlc_object_t **)&videoDecoder, nullptr, nullptr);
	return videoDecoder;
}

Player::Item *Player::getItemAt(frame_id frame)
{
	return const_cast<Item *>(static_cast<const Player&>(*this).getItemAt(frame));
}

const Player::Item *Player::getItemAt(frame_id frame) const
{
	auto it = items.lower_bound(frame);
	if (it == items.end())
	{
		it--;
		if (it->second.getInterval().out > frame) { return &it->second;}
		else return nullptr;
	}
	else if (it->first == frame) { return &it->second; }
	else
	{
		it--;
		return &it->second;
	}
}

Interval Player::getCurInterval() const
{
	if (curInterval == playIntervals.end()) { return Interval(); }
	return curInterval->second;
}

Interval Player::getNextInterval() const
{
	auto next = curInterval; next++;
	if (next != playIntervals.end())
	{
		return next->second;
	}
	return Interval();
}

void Player::skipToCurInterval()
{
	Interval interval = getCurInterval();
	Item *item = getItemAt(interval.in);
	if (!item) return;
	
	item->skip(interval.in);
}

const Player::Item *Player::getCurItem() const
{
	return getItemAt(getCurInterval().in);
}

Player::Item::Item(Player *player, const Interval &interval, 
	es_out_t *outStream, PreloadVideoStream *preloadStream, const std::string &filename):
	player(player), interval(interval), valid(false)
{
	name = filename;
	demux = createDemuxer(filename, outStream);
	demux_t *preloadDemux = createDemuxer(filename, preloadStream->getWrapperStream());
	preloader.setDemuxer(preloadDemux);
	preloader.setVideoStream(preloadStream);
	if (!demux || !preloadDemux) { return; }
	valid = true;
}

void Player::Item::skip(frame_id globalFrame) const
{
	mtime_t time = globalToLocalFrame(globalFrame) * player->getFrameLen();
	demux_Control(demux, DEMUX_SET_TIME, time, true);
}

int Player::Item::play() const
{
	return demux->pf_demux(demux);
}

demux_t *Player::Item::createDemuxer(const std::string &filename, es_out_t *outStream) const
{
	stream_t *stream = vlc_stream_NewMRL(player->getVlcObj(), ("file://" + filename).c_str());
	if (!stream) { return nullptr; }
	return demux_New(player->getVlcObj(), "any", filename.c_str(), stream, outStream);
}

int Player::play()
{
	if (needInitItems)
	{
		needInitItems = false;
		for (auto it = items.begin(); it != items.end(); it++)
		{
			Item &item = (*it).second;
			item.play();
			item.setFirstFrameOffset(out->getLastBlockTime());
			item.skip(item.getInterval().in);
		}
		out->enableOutput();
	}
	int res = VLC_DEMUXER_SUCCESS;

	if (!intervalsSelected && !dialog->isShown()) { showDialog(); }
	else if (intervalsSelected && dialog->isShown()) { hideDialog(); }
	else
	{
		vlc_mutex_lock(&intervalsMutex);
		//decoder_owner_sys_t *p_owner = p_dec->p_owner;
	
		if (getCurInterval().length() <= out->getHandledFrameId()) //interval handled, seek to next
		{
			Interval next = getNextInterval();
			if (next.length() == 0) { res = VLC_DEMUXER_EOF; }
			else
			{
				decoder_t *videoDecoder = getVideoDecoder();
				//bool fifoIsEmpty = input_DecoderIsEmpty(videoDecoder);
				block_fifo_t *blockQueue = videoDecoder->p_owner->p_fifo;
				vlc_fifo_Lock(blockQueue);
				int queueSize = vlc_fifo_GetCount(blockQueue);
				vlc_fifo_Unlock(blockQueue);
				//msg_Dbg(obj, "Player fifo size = %i", queueSize);
				
				if (queueSize > 0) { res = VLC_DEMUXER_SUCCESS; }
				else
				{
					Item *nextItem = getItemAt(next.in);
					nextItem->waitPrepared();
					curInterval++;
					out->resetFramesNum();
					decoder_t *preloadDecoder = preload->getDecoder();
					std::swap(videoDecoder->p_sys, preloadDecoder->p_sys);
					nextItem->applyPrepared();
					res = VLC_DEMUXER_SUCCESS;
					//skipToCurInterval();
					msg_Dbg(obj, "Player next interval: %li", (*curInterval).first);
					msg_Dbg(obj, "Player dec: 0x%lx", (long unsigned)videoDecoder);
				}
				vlc_object_release(videoDecoder);
			}
		}
		else 
		{
			const Item *item = getCurItem();
			if (!item) { res = VLC_DEMUXER_EOF; }
			if (res != VLC_DEMUXER_EOF)
			{
				//msg_Dbg(obj, "Play");
				res = item->play();
			}
		}		
		vlc_mutex_unlock(&intervalsMutex);
	}
	return res;
}

int Player::control(int query, va_list args)
{
	bool *pbool; 
	mtime_t *ptime;
	double *pf;
	
    switch(query)
    {
        case DEMUX_CAN_SEEK:
			*va_arg(args, bool *) = true;
            return VLC_SUCCESS;

        case DEMUX_GET_META:
            return VLC_SUCCESS;

        case DEMUX_HAS_UNSUPPORTED_META:
            pbool = va_arg(args, bool *);
            *pbool = false;
            return VLC_SUCCESS;

        case DEMUX_SET_NEXT_DEMUX_TIME:
            return VLC_EGENERIC;

        case DEMUX_GET_TIME:
			ptime = va_arg(args, mtime_t *);
			*ptime = out->getTime();
			return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            return VLC_SUCCESS;

        case DEMUX_GET_ATTACHMENTS:
			return VLC_EGENERIC;

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double *);
            *pf = (double)out->getTime() / getLength();
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
			seek(va_arg(args, double));
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            ptime = va_arg( args, mtime_t *);
            *ptime = getLength();
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

void Player::seek(double pos)
{
	const frame_id targetFrame = round(length * pos);
	frame_id skippedFrames = 0;
	
	for (auto it = playIntervals.begin(); it != playIntervals.end(); it++)
	{
		Interval &interval = (*it).second;
		if (skippedFrames + interval.length() < targetFrame) //found target interval
		{
			skippedFrames += interval.length();
		}
		else 
		{
			curInterval = it;
			frame_id globalFrame = (targetFrame - skippedFrames) + interval.in;
			seek(globalFrame, targetFrame);
			return;
		}
	}
}

void Player::seek(frame_id globalFrame, frame_id streamFrame)
{
	Item *item = getItemAt(globalFrame);
	if (!item) return;
	
	item->skip(globalFrame);
	out->setTime(streamFrame * getFrameLen());
}

frame_id Player::getStreamFrameByGlobal(frame_id frame) const
{
	frame_id streamFrames = 0;
	for (auto it = playIntervals.begin(); it != playIntervals.end(); it++)
	{
		const Interval &interval = (*it).second;
		if (interval.in > frame) { break; }
		if (interval.out <= frame) { streamFrames += interval.length(); }
	}
	return streamFrames;
}

void Player::setPause(bool pause) const
{
	var_SetInteger( obj->p_input, "state", pause? PAUSE_S: PLAYING_S);
}

vlc_object_t *print;
void Player::Item::prepare(frame_id frame)
{
	mtime_t time = globalToLocalFrame(frame) * player->getFrameLen();
	msg_Dbg(player->getVlcObj(), "Prepare frame %li (time %li)", frame, time);
	print = player->getVlcObj();
	preloader.load(time);
}

int Player::Item::waitPrepared()
{
	return preloader.wait();
}

void Player::Item::applyPrepared()
{
	demux_sys_t *sys = demux->p_sys;
	demux->p_sys = preloader.getDemuxer()->p_sys;
	preloader.getDemuxer()->p_sys = sys;
}

void Preloader::load(mtime_t time)
{
	done = false;
	target = time;
	
	auto loadFunc = [] (void *preloader) -> void *
	{
		((Preloader *)preloader)->loadInThread();
		return nullptr;
	};
	
	int res = vlc_clone(&thread, loadFunc, this, VLC_THREAD_PRIORITY_LOW);
	if (res != 0) { done = true; }
}
#include <time.h>
void Preloader::loadInThread()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	double timestamp = ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
	stream->setTargetTime(target);
	demux_Control(demux, DEMUX_SET_TIME, target, true);
	
	while (!stream->ready())
	{
		int res = demux->pf_demux(demux);
		if (res != VLC_DEMUXER_SUCCESS) {return;}
	}
	
	done = stream->ready();
	clock_gettime(CLOCK_MONOTONIC, &ts);
	double timestamp2 = ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
	msg_Dbg(print, "Done in %f msec", timestamp2 - timestamp);
}

bool Preloader::wait()
{
	vlc_join(thread, nullptr);
	return done;
}


}
