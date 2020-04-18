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

Player::Player(demux_t *obj, FeatureList *featureList) : obj(obj), featureList(featureList)
{
	demux_t *demuxer = (demux_t *)obj;
	out = new OutStream(demuxer->out, this);
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
	delete dialog;
	vlc_mutex_destroy(&intervalsMutex);
	var_DelCallback( obj->obj.libvlc, "key-action", ActionEvent, this);
}

bool Player::isValid() const
{
	if (items.empty()) return false;
	
	for (auto it: items)
	{
		if (!it.second.isValid()) return false;
	}
	
	return true;
}

void Player::addFile(const Interval &interval, const std::string &filename)
{
	out->reuseStreams();
	items[interval.in] = Item((vlc_object_t *)obj, interval, out->getWrapperStream(), filename);
	length = wholeDuration = interval.out;
	playIntervals[0] = Interval(0, wholeDuration);
	curInterval = playIntervals.begin();
}

bool Player::timeIsInPlayInterval(mtime_t time) const
{
	return getCurInterval().contains(getCurItem()->getInterval().in + time);
}

mtime_t Player::getFrameLen() const
{
	const Item *item = getCurItem();
	if (!item) return 0;
	else
	{
		return item->getFrameLen();
	}
}

int Player::getFrameId(mtime_t timeInItem) const
{
	return round((double)timeInItem / getFrameLen());
}

int Player::getCurIntervalFirstFrame() const
{
	mtime_t timeInItem = getCurInterval().in - getCurItem()->getInterval().in;
	return getFrameId(timeInItem);
}

void Player::setIntervalsSelected()
{
	Interval &newInterval = curInterval->second;
	mtime_t newTime = newInterval.contains(savedTime) ? savedTime : newInterval.in;
	
	msg_Dbg(obj, "Seek to %li", newTime);
	seek(newTime, getStreamTimeByGlobal(newTime));

	intervalsSelected = true;
	setPause(false);
}

void Player::showDialog() 
{
	setPause(true);
	intervalsSelected = false; 
	savedTime = getGlobalTime();
	dialog->show();
}

void Player::hideDialog()
{
	dialog->hide();
}

mtime_t Player::getGlobalTime() const
{
	return getCurInterval().in + out->getHandledFrameNum() * getFrameLen();
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

void Player::modifyIntervals(bool add, const Feature *f, int8_t minIntensity, int8_t maxIntensity, bool affectUnmarked)
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
		mtime_t prevOut = 0; 
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

mtime_t Player::recalcLength()
{
	length = 0;
	for (auto &p: playIntervals)
	{
		length += p.second.length();
	}
	msg_Dbg(obj, "~~~~Player Intervals: %li", playIntervals.size());
	for (auto p: playIntervals)
	{
		msg_Dbg(obj, "~~~~interval: %li - %li", p.second.in, p.second.out);
	}
	
	return length;
}

void Player::updateCurrentInterval()
{
	if (savedTime)
	{
		auto closestIt = playIntervals.lower_bound(savedTime);
		if (closestIt == playIntervals.end()) 
		{
			closestIt = playIntervals.begin(); 
		}
		else if (closestIt != playIntervals.begin())
		{
			closestIt--; //select prev interval, if it contains our timestamp
			if (closestIt->second.out <= savedTime)
			{
				closestIt++;
			}
		}
		curInterval = closestIt;
	}
	else { curInterval = playIntervals.begin(); }
}

int Player::framesInPlayInterval() const
{
	Interval interval = getCurInterval();
	const Player::Item *item = getItemAt(interval.in);
	return (interval.out - interval.in) / item->getFrameLen();
}

mtime_t Player::globalToLocalTime(mtime_t global) const 
{
	const Item *item = getItemAt(global);
	if (!item) return 0;
	
	return item->globalToLocalTime(global);
}

Player::Item *Player::getItemAt(mtime_t time)
{
	return const_cast<Item *>(static_cast<const Player&>(*this).getItemAt(time));
}

const Player::Item *Player::getItemAt(mtime_t time) const
{
	auto it = items.lower_bound(time);
	if (it == items.end())
	{
		it--;
		if (it->second.getInterval().out > time) { return &it->second;}
		else return nullptr;
	}
	else if (it->first == time) { return &it->second; }
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
	
	item->skip(item->globalToLocalTime(interval.in));
}

const Player::Item *Player::getCurItem() const
{
	return getItemAt(getCurInterval().in);
}

Player::Item::Item(vlc_object_t *obj, const Interval &interval, 
	es_out_t *outStream, const std::string &filename):
	interval(interval), valid(false)
{	
	stream = vlc_stream_NewMRL(obj, ("file://" + filename).c_str());
	if (!stream) { return; }
	
	demux = demux_New(obj, "any", filename.c_str(), stream, outStream);
	if (!demux) { return; }
	
	double fps;
	demux_Control(demux, DEMUX_GET_FPS, &fps);
	frameLen = 1000000 / fps;
	name = filename;
	
	//msg_Dbg(obj, "~~~~Player::Item: frameLen = %f", frameLen);	
	valid = true;
}

void Player::Item::skip(mtime_t time) const
{
	demux_Control(demux, DEMUX_SET_TIME, time, true);
}

int Player::Item::play() const
{
	return demux->pf_demux(demux);
}

int Player::play()
{
	int res = VLC_DEMUXER_SUCCESS;
	//msg_Dbg(obj, "Player");
	if (!intervalsSelected && !dialog->isShown()) { showDialog(); }
	else if (intervalsSelected && dialog->isShown()) { hideDialog(); }
	else
	{
		vlc_mutex_lock(&intervalsMutex);
	
		if (framesInPlayInterval() <= out->getHandledFrameNum()) //interval handled, seek to next
		{
			curInterval++;
			if (curInterval == playIntervals.end()) { res = VLC_DEMUXER_EOF; }
			else 
			{
				out->resetFramesNum();
				skipToCurInterval();
				msg_Dbg(obj, "~~~~Player next interval: %li", (*curInterval).first);
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
            *pf = (double)out->getTime() / length;
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
			seek(va_arg(args, double));
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            ptime = va_arg( args, mtime_t *);
            *ptime = length;
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
	const mtime_t streamTime = length * pos;
	mtime_t skippedTime = 0;
	
	for (auto it = playIntervals.begin(); it != playIntervals.end(); it++)
	{
		Interval &interval = (*it).second;
		if (skippedTime + interval.length() < streamTime) //found target interval
		{
			skippedTime += interval.length();
		}
		else 
		{
			curInterval = it;
			mtime_t globalTime = streamTime - skippedTime + interval.in;
			seek(globalTime, streamTime);
			return;
		}
	}
}

void Player::seek(mtime_t globalTime, mtime_t streamTime)
{
	Item *item = getItemAt(globalTime);
	if (!item) return;
	
	item->skip(item->globalToLocalTime(globalTime));
	out->setTime(streamTime);
}

mtime_t Player::getStreamTimeByGlobal(mtime_t global) const
{
	mtime_t streamTime = 0;
	for (auto it = playIntervals.begin(); it != playIntervals.end(); it++)
	{
		const Interval &interval = (*it).second;
		if (interval.in > global) { return streamTime; }
		if (interval.out <= global) { streamTime += interval.length(); }
	}
	return streamTime;
}

void Player::setPause(bool pause) const
{
	var_SetInteger( obj->p_input, "state", pause? PAUSE_S: PLAYING_S);
}


}
