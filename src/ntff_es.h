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
	mtime_t getTime() const { return curTime; }
	
	struct es_out_t *out;
	scene_list *scenes;
	demux_t *p_demux;
private:
	std::set<es_out_id_t *> video;
	std::set<es_out_id_t *> audio;
	mtime_t frameLen;
	mtime_t curTime;
};

#endif //NTFF_ES_H_INCLUDED
