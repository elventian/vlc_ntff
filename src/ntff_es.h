#ifndef NTFF_ES_H_INCLUDED
#define NTFF_ES_H_INCLUDED


#include "ntff.h"
#include <set>

class es_out_sys_t
{
public: 
	es_out_sys_t(demux_t *p_demux, scene_list *scenes, struct es_out_t *es);
	
	void addVideo(es_out_id_t *stream) { video.insert(stream); }
	void addAudio(es_out_id_t *stream) { audio.insert(stream); }
	bool isVideo(es_out_id_t *stream) const { return video.count(stream); }
	bool isAudio(es_out_id_t *stream) const { return audio.count(stream); }
	void setFrameLen(mtime_t len) { frameLen = len; }
	mtime_t updateTime();
	void resetFramesNum() { framesNum = 0; }
	mtime_t getTime() const { return curTime; }
	mtime_t getFrameLen() const { return frameLen; }
	uint32_t getFramesNum() const { return framesNum; }
	
	struct es_out_t *out;
	demux_t *p_demux;
	scene_list *scenes;
private:
	std::set<es_out_id_t *> video;
	std::set<es_out_id_t *> audio;
	mtime_t frameLen;
	mtime_t curTime;
	uint32_t framesNum;
};

#endif //NTFF_ES_H_INCLUDED
