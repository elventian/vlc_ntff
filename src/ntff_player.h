#ifndef NTFF_PLAYER_H
#define NTFF_PLAYER_H

#include <string>
#include <list>
#include <vlc_common.h>

namespace Ntff {

class Player
{
	class Item;
public:
	Player(vlc_object_t *obj);
	void addFile(mtime_t in, mtime_t out, const std::string &filename);
private:
	std::list<Item> items;
	vlc_object_t *obj;
};

class Player::Item
{
public:
	Item(vlc_object_t *obj, mtime_t in, mtime_t out, const std::string &filename);
private:
	mtime_t in;
	mtime_t out;
	stream_t *stream;
	demux_t *fdemux;
	bool valid;
};

}

#endif // NTFF_PLAYER_H
