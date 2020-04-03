#ifndef NTFF_PLAYER_H
#define NTFF_PLAYER_H

#include <string>
#include <map>
#include <vlc_common.h>
#include "ntff_feature.h"

namespace Ntff {

class OutStream;
class Dialog;

class Player
{
	class Item;
public:
	Player(vlc_object_t *obj, FeatureList *featureList);
	~Player();
	bool isValid() const;
	void addFile(const Interval &interval, const std::string &filename);
	int play();
	int control(int query, va_list args);
	void reset();
	
	bool timeIsInPlayInterval(mtime_t time) const;
	vlc_object_t *getVlcObj() const { return obj; }
	mtime_t getFrameLen() const;
private:
	vlc_object_t *obj;
	FeatureList *featureList;
	std::map<mtime_t, Item> items;
	OutStream *out;
	std::map<mtime_t, Interval> playIntervals;
	std::map<mtime_t, Interval>::iterator curInterval;
	mtime_t length;
	Dialog *dialog;
	
	void skipToCurInterval();
	const Item *getCurItem() const;
	Item *getItemAt(mtime_t time);
	const Item *getItemAt(mtime_t time) const;
	uint32_t framesInPlayInterval() const;
};

class Player::Item
{
public:
	Item(){}
	Item(vlc_object_t *obj, const Interval &interval, es_out_t *outStream, const std::string &filename);
	const std::string &getName() const { return name; }
	bool isValid() const { return valid; }
	double getFrameLen() const { return frameLen; }
	void skip(mtime_t time) const;
	int play() const;
	const Interval &getInterval() const { return interval; }
private:
	Interval interval;
	double frameLen;
	stream_t *stream;
	demux_t *demux;
	bool valid;
	std::string name;
};

}

#endif // NTFF_PLAYER_H
