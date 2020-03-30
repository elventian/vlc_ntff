#ifndef NTFF_PROJECT_H
#define NTFF_PROJECT_H

#include <string>
#include <list>
struct stream_t;
struct vlc_object_t;
struct xml_reader_t;

namespace Ntff {

class Playlist;
class Producer;
class FeatureList;
class FeatureTrack;

class Project
{
	public:
		Project(vlc_object_t *obj, const char *file, stream_t *stream);
		bool isValid() const { return valid; }
		float getFrameLen() const { return 1000000 / fps;}
		FeatureList *generateFeatureList() const;
		
		static int nextSibling(xml_reader_t *reader, const std::string &curNode, 
			bool curEmpty, std::string &resNode);
	private:
		vlc_object_t *obj;
		bool valid;
		float fps;
		std::list<Playlist *>playlists;
		std::list<Producer *>producers;
		std::list<FeatureTrack *>tracks;
		Playlist *mainPlaylist;
		
		bool bindProducers();
		bool bindTracks();
};


}


#endif // NTFF_PROJECT_H
