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
	Player(demux_t *obj, FeatureList *featureList);
	~Player();
	bool isValid() const;
	void addFile(const Interval &interval, const std::string &filename);
	int play();
	int control(int query, va_list args);
	void reset();
	void seek(double pos);
	void setPause(bool pause) const;
	
	bool timeIsInPlayInterval(mtime_t time) const;
	vlc_object_t *getVlcObj() const { return (vlc_object_t *)obj; }
	mtime_t getFrameLen() const;
	int getFrameId(mtime_t timeInItem) const;
	int getCurIntervalFirstFrame() const;
	void updatePlayIntervals();
	void setIntervalsSelected();
	void showDialog();
	void hideDialog();
	mtime_t getGlobalTime() const;
	mtime_t getLength() const { return length; }
	void lockIntervals(bool lock);
	void resetIntervals(bool empty);
	void modifyIntervals(bool add, const Feature *f, int8_t minIntensity, int8_t maxIntensity, bool affectUnmarked);
	mtime_t recalcLength();
	void updateCurrentInterval();
private:
	demux_t *obj;
	FeatureList *featureList;
	std::map<mtime_t, Item> items;
	OutStream *out;
	vlc_mutex_t intervalsMutex;
	std::map<mtime_t, Interval> playIntervals;
	std::map<mtime_t, Interval>::iterator curInterval;
	mtime_t length;
	mtime_t wholeDuration;
	mtime_t savedTime;
	bool intervalsSelected;
	Dialog *dialog;
	
	void skipToCurInterval();
	const Item *getCurItem() const;
	Item *getItemAt(mtime_t time);
	const Item *getItemAt(mtime_t time) const;
	Interval getCurInterval() const;
	int framesInPlayInterval() const;
	mtime_t globalToLocalTime(mtime_t global) const; //convert global project time to local file time
	void seek(mtime_t globalTime, mtime_t streamTime);
	mtime_t getStreamTimeByGlobal(mtime_t global) const;
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
	mtime_t globalToLocalTime(mtime_t global) const { return global - interval.in;}
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
