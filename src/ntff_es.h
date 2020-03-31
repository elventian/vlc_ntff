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

namespace  Ntff 
{
class Player;

class OutStream
{
public: 
	OutStream(es_out_t *out, Player *player);
	
	bool isVideo(es_out_id_t *stream) const { return video.count(stream); }
	bool isAudio(es_out_id_t *stream) const { return audio.count(stream); }
	mtime_t updateTime()
	{
		framesNum++;
		curTime += frameLen;
		return curTime;
	}
	void resetFramesNum() { framesNum = 0; }
	mtime_t getTime() const { return curTime; }
	mtime_t getFrameLen() const { return frameLen; }
	uint32_t getFramesNum() const { return framesNum; }
	es_out_t *getFakeOutStream() { return &fakeOut; }
	
	es_out_id_t *addElemental(const es_format_t *format);
	void removeElemental(es_out_id_t *id);
	int sendBlock(es_out_id_t *streamId, block_t *block);
	int control(int i_query, va_list va);
	void destroyOutStream();
private:
	std::set<es_out_id_t *> video;
	std::set<es_out_id_t *> audio;
	mtime_t frameLen;
	mtime_t curTime;
	uint32_t framesNum;
	
	es_out_t *out;
	es_out_t fakeOut;
	Player *player;
};

}

#endif //NTFF_ES_H_INCLUDED
