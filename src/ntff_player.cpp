#include "ntff_player.h"
#include "ntff_es.h"
#include "ntff_feature.h"
#include "ntff_dialog.h"
#include <vlc_stream_extractor.h>
#include <vlc_demux.h>
#include <vlc_actions.h>
#include <vlc_input.h>
#include <vlc_threads.h>

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
	preload = new PreloadVideoStream(demuxer->out, this);
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
	lockIntervals(true);
	auto next = curInterval; next++;
	if (next != playIntervals.end())
	{
		frame_id intervalBegin = next->first;
		Item *item = getItemAt(intervalBegin);
		if (item)
		{
			item->prepare(intervalBegin);
		}
	}
	lockIntervals(false);
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
	return getCurInterval().in + out->getHandledFrameNum();
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
	
		if (getCurInterval().length() <= out->getHandledFrameNum()) //interval handled, seek to next
		{
			curInterval++;
			if (curInterval == playIntervals.end()) { res = VLC_DEMUXER_EOF; }
			else 
			{
				out->resetFramesNum();
				skipToCurInterval();
				msg_Dbg(obj, "Player next interval: %li", (*curInterval).first);
			}
		}
		
		const Item *item = getCurItem();
		if (!item) { res = VLC_DEMUXER_EOF; }
		if (res != VLC_DEMUXER_EOF)
		{
			res = item->play();
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

void Preloader::load(mtime_t time)
{
	done = false;
	target = time;
	
	auto loadFunc = [] (void *preloader)
	{
		((Preloader *)preloader)->loadInThread();
		return preloader;
	};
	
	vlc_clone(&thread, loadFunc, this, VLC_THREAD_PRIORITY_LOW);
}

void Preloader::loadInThread()
{
	stream->setTargetTime(target);
	demux_Control(demux, DEMUX_SET_TIME, target, true);
	
	while (!stream->ready())
	{
		int res = demux->pf_demux(demux);
		if (res != VLC_DEMUXER_SUCCESS) {return;}
	}
	
	done = stream->ready();
}

bool Preloader::wait()
{
	vlc_join(thread, nullptr);
	return done;
}


}
