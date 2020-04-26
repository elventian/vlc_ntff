#ifndef NTFF_PROJECT_H
#define NTFF_PROJECT_H

#include <string>
#include <list>
struct stream_t;
struct vlc_object_t;
struct xml_reader_t;
struct demux_t;

namespace Ntff {

class Playlist;
class Producer;
class FeatureList;
class FeatureTrack;
class Player;

class Project
{
	public:
		Project(vlc_object_t *obj, const char *file, stream_t *stream);
		bool isValid() const { return valid; }
		Player *createPlayer(demux_t *demux) const;
		
		static int nextSibling(xml_reader_t *reader, const std::string &curNode, 
			bool curEmpty, std::string &resNode);
		static vlc_object_t *getVlcObj() { return obj; }
	private:
		static vlc_object_t *obj;
		bool valid;
		double fps;
		std::list<Playlist *>playlists;
		std::list<Producer *>producers;
		std::list<FeatureTrack *>tracks;
		Playlist *mainPlaylist;
		
		bool bindProducers();
		bool bindTracks();
		FeatureList *generateFeatureList() const;
};


}


#endif // NTFF_PROJECT_H
