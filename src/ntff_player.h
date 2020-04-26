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
	Player(demux_t *obj, FeatureList *featureList, double fps);
	~Player();
	bool isValid() const;
	void addFile(const Interval &interval, const std::string &filename);
	int play();
	int control(int query, va_list args);
	void seek(double pos);
	void setPause(bool pause) const;
	
	bool frameIsInPlayInterval(frame_id frame) const;
	vlc_object_t *getVlcObj() const { return (vlc_object_t *)obj; }
	double getFrameLen() const { return 1000000 / fps; }
	int getFrameId(mtime_t timeInItem) const;
	frame_id getCurIntervalFirstFrame() const;
	void setIntervalsSelected();
	void showDialog();
	void hideDialog();
	frame_id getGlobalFrame() const;
	mtime_t getLength() const { return length * getFrameLen(); }
	void lockIntervals(bool lock);
	void resetIntervals(bool empty);
	void modifyIntervals(bool add, const Feature *f, int8_t minIntensity, int8_t maxIntensity, bool affectUnmarked);
	void recalcLength();
	void updateCurrentInterval();
private:
	demux_t *obj;
	FeatureList *featureList;
	std::map<frame_id, Item> items;
	OutStream *out;
	vlc_mutex_t intervalsMutex;
	std::map<frame_id, Interval> playIntervals;
	std::map<frame_id, Interval>::iterator curInterval;
	frame_id length;
	frame_id wholeDuration;
	frame_id savedFrameId;
	bool intervalsSelected;
	Dialog *dialog;
	bool needInitItems;
	double fps;
	
	void skipToCurInterval();
	const Item *getCurItem() const;
	Item *getItemAt(frame_id frame);
	const Item *getItemAt(frame_id frame) const;
	Interval getCurInterval() const;
	void seek(frame_id globalFrame, frame_id streamFrame);
	frame_id getStreamFrameByGlobal(frame_id frame) const;
	mtime_t getCurOffset() const;
};

class Player::Item
{
public:
	Item(){}
	Item(vlc_object_t *obj, const Interval &interval, es_out_t *outStream, const std::string &filename);
	const std::string &getName() const { return name; }
	bool isValid() const { return valid; }
	void skip(mtime_t time) const;
	int play() const;
	const Interval &getInterval() const { return interval; }
	frame_id globalToLocalFrame(frame_id global) const { return global - interval.in;}
	void setFirstFrameOffset(mtime_t offset) { firstFrameOffset = offset; }
	mtime_t getFirstFrameOffset() const { return firstFrameOffset; }
private:
	Interval interval;
	stream_t *stream;
	demux_t *demux;
	bool valid;
	std::string name;
	mtime_t firstFrameOffset; //if videofile has B-frames, first frame will have pts != 0
};

}

#endif // NTFF_PLAYER_H
