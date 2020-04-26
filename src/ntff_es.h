#ifndef NTFF_ES_H_INCLUDED
#define NTFF_ES_H_INCLUDED

#include <set>
#include <vlc_common.h>
#include <vlc_es_out.h>

namespace  Ntff 
{
class Player;

enum EStreamType
{
	Unknown,
	Audio,
	Video,
	EStreamTypeNum
};

class EStreamCollection
{
public:
	EStreamCollection() { reuse(); }
	void reuse();
	es_out_id_t *getNext(EStreamType type);
	void append(es_out_id_t *id, EStreamType type);
	EStreamType getType(es_out_id_t *stream) const;	
	
	static EStreamType typeByVlcFormat(const es_format_t *format);
private:
	std::set<es_out_id_t *> streams[EStreamTypeNum];
	std::set<es_out_id_t *>::iterator streamIt[EStreamTypeNum];
};

class OutStream
{
public: 
	OutStream(es_out_t *out, Player *player);
	
	bool isVideo(es_out_id_t *stream) const { return streams.getType(stream) == Video; }
	bool isAudio(es_out_id_t *stream) const { return streams.getType(stream) == Audio; }
	mtime_t updateTime();
	void resetFramesNum() { framesQueue.clear(); }
	void setTime(mtime_t time);
	mtime_t getTime() const { return curTime; }
	mtime_t getLastBlockTime() const { return lastBlockTime; }
	int64_t getHandledFrameNum() const;
	es_out_t *getWrapperStream() { return &wrapper; }
	void reuseStreams() { streams.reuse(); }
	
	es_out_id_t *addElemental(const es_format_t *format);
	void removeElemental(es_out_id_t *id);
	int sendBlock(es_out_id_t *streamId, block_t *block);
	int control(int i_query, va_list va);
	void destroyOutStream();
	void enableOutput() { outputEnabled = true; }
private:
	EStreamCollection streams;
	mtime_t curTime;
	mtime_t lastBlockTime;
	std::set<int64_t> framesQueue;
	bool outputEnabled;
	
	es_out_t *out;
	es_out_t wrapper;
	Player *player;
	
	void addFrame(int64_t frame);
};

}

#endif //NTFF_ES_H_INCLUDED
