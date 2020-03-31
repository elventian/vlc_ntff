#include "ntff_player.h"
#include <vlc_stream_extractor.h>
#include <vlc_demux.h>

namespace Ntff {

Player::Player(vlc_object_t *obj) : obj(obj)
{
	
}

void Player::addFile(mtime_t in, mtime_t out, const std::string &filename)
{
	items.push_back(Item(obj, in, out, filename));
}

Player::Item::Item(vlc_object_t *obj, mtime_t in, mtime_t out, const std::string &filename):
	in(in), out(out), valid(false)
{
	//msg_Dbg(obj, "~~~~Player::Item: %s", ("file://" + filename).c_str());
	
	stream = vlc_stream_NewMRL(obj, ("file://" + filename).c_str());
	if (!stream) { return; }
	
	valid = true;
	
	/*const char *psz_filepath = "/home/elventian/Expanse.mkv";
	const char *psz_name = "any";*/
}


}
