#include "ntff_player.h"
#include "ntff_es.h"
#include "ntff_feature.h"
#include "ntff_dialog.h"
#include <vlc_stream_extractor.h>
#include <vlc_demux.h>

namespace Ntff {

Player::Player(vlc_object_t *obj, FeatureList *featureList) : obj(obj), featureList(featureList)
{
	demux_t *demuxer = (demux_t *)obj;
	out = new OutStream(demuxer->out, this);
	dialog = new Dialog(obj, featureList);
}

Player::~Player()
{
	delete featureList;
	delete out;
	delete dialog;
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
	items[interval.in] = Item(obj, interval, out->getWrapperStream(), filename);
}

bool Player::timeIsInPlayInterval(mtime_t time) const
{
	if (curInterval == playIntervals.end()) return false;
	
	Interval &interval = (*curInterval).second;
	return interval.contains(getCurItem()->getInterval().in + time);
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

uint32_t Player::framesInPlayInterval() const
{
	if (curInterval == playIntervals.end()) return 0;
	
	Interval &interval = (*curInterval).second;
	const Player::Item *item = getItemAt(interval.in);
	return (interval.out - interval.in) / item->getFrameLen();
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

void Player::skipToCurInterval()
{
	if (curInterval == playIntervals.end()) return;
	
	Interval &interval = (*curInterval).second;
	Item *item = getItemAt(interval.in);
	if (!item) return;
	
	msg_Dbg(obj, "~~~~skipToCurInterval: %li", interval.in);
	item->skip(interval.in);
}

const Player::Item *Player::getCurItem() const
{
	if (curInterval == playIntervals.end()) return nullptr;
	
	Interval &interval = (*curInterval).second;
	return getItemAt(interval.in);
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
	demux_Control(demux, DEMUX_SET_TIME, time - interval.in, true);
}

int Player::Item::play() const
{
	return demux->pf_demux(demux);
}

int Player::play()
{
	if (dialog->wait())
	{
		reset();
	}
	
	if (framesInPlayInterval() == out->getFramesNum()) //interval handled, seek to next
	{
		curInterval++;
		out->resetFramesNum();
		skipToCurInterval();
		if (curInterval == playIntervals.end()) { return VLC_DEMUXER_EOF; }
		msg_Dbg(obj, "~~~~Player next interval: %li", (*curInterval).first);
	}
	
	const Item *item = getCurItem();
	if (!item) { return VLC_DEMUXER_EOF; }
	else 
	{
		int res = item->play();
		msg_Dbg(obj, "~~~~Player play: %s, res = %i", item->getName().c_str(), res);
		return res;
	}
}

int Player::control(int query, va_list args)
{
	bool *pbool; 
	mtime_t *ptime;
	double *pf;
	
    switch(query)
    {
        case DEMUX_CAN_SEEK:
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

void Player::reset()
{
	length = featureList->formSelectedIntervals(playIntervals); //TODO: split intervals in between files
	curInterval = playIntervals.begin();
	skipToCurInterval();
	
	msg_Dbg(obj, "~~~~Player Intervals: %li", playIntervals.size());
	for (auto p: playIntervals)
	{
		msg_Dbg(obj, "~~~~interval: %li - %li", p.second.in, p.second.out);
	}
	
	msg_Dbg(obj, "~~~~Player Items: %li", items.size());
	for (auto p: items)
	{
		msg_Dbg(obj, "~~~~item length: %li - %li", p.second.getInterval().in, p.second.getInterval().out);
	}
}


}
