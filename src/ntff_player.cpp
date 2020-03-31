#include "ntff_player.h"
#include "ntff_es.h"
#include "ntff_feature.h"
#include <vlc_stream_extractor.h>
#include <vlc_demux.h>

namespace Ntff {

Player::Player(vlc_object_t *obj, FeatureList *featureList) : obj(obj), featureList(featureList)
{
	demux_t *demuxer = (demux_t *)obj;
	out = new OutStream(demuxer->out, this);
	playIntervals = featureList->formSelectedIntervals(); //TODO: split intervals in between files
	curInterval = playIntervals.begin();
	skipToCurInterval();
	
	msg_Dbg(obj, "~~~~Player Intervals: %li", playIntervals.size());
	for (auto p: playIntervals)
	{
		msg_Dbg(obj, "~~~~interval: %li - %li", p.second.in, p.second.out);
	}
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
	items[interval.in] = Item(obj, interval, out->getFakeOutStream(), filename);
}

int Player::play()
{
	if (framesInPlayInterval() == out->getFramesNum())
	{
		//Seek to next interval
		out->resetFramesNum(); //WARNING
	}
}

bool Player::timeIsInPlayInterval(mtime_t time) const
{
	if (curInterval == playIntervals.end()) return false;
	
	Interval &interval = (*curInterval).second;
	return interval.contains(time);
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
	if (it == items.end()) { return nullptr; }
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
	
	item->skip(interval.in);
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
	
	//msg_Dbg(obj, "~~~~Player::Item: frameLen = %f", frameLen);	
	valid = true;
}

void Player::Item::skip(mtime_t time)
{
	demux_Control(demux, DEMUX_SET_TIME, time, true);
}


}
