#include "ntff_project.h"
#include "ntff_feature.h"
#include "ntff_player.h"
#include <list>
#include <cstdio>
#include <sstream>
#include <filesystem>
#include <vlc_common.h>
#include <vlc_xml.h>

namespace Ntff {

vlc_object_t *Project::obj;

class Producer
{
	friend std::ostream &operator<<(std::ostream &out, Producer const &producer);
public:
	 Producer(xml_reader_t *reader);
	 bool isFeature() const { return feature; }
	 const std::string &getName() const { return id; }
	 const std::string &getResource() const { return resource; }
	 bool updatePath(const std::string &parentPath);
private:
	 std::string id;
	 std::string resource;
	 bool feature;
};

class Entry
{
	friend std::ostream &operator<<(std::ostream &out, Entry const &entry);
public:
	Entry(mtime_t in, mtime_t out, int8_t intensity, const char *producer): 
		interval(in, out, intensity), producer(producer), producerPtr(nullptr) {}
	const std::string &getProducerName() const { return producer; }
	void setProducer(Producer *producer) { producerPtr = producer; }
	bool updatePath(const std::string &parentPath);
	const Interval &getInterval() const { return interval; }
	const std::string &getResource() const { return producerPtr->getResource(); }
private:
	Interval interval;
	std::string producer;
	Producer *producerPtr;
};

class  Playlist
{
	friend std::ostream &operator<<(std::ostream &out, Playlist const &playlist);
public:
	 Playlist(xml_reader_t *reader, float frameLen);
	 bool isEmpty() { return entries.empty(); }
	 bool isFeature() const { return feature; }
	 void bindProducers(const std::list<Producer *>&producers);
	 bool updatePath(const std::string &parentPath);
	 std::string getName()const {return id;}
	 const std::list<Entry> &getEntries() const { return entries; }
private:
	 std::string id;
	 std::list<Entry> entries;
	 bool feature;
	 void skipToEnd(xml_reader_t *reader) const;
	 mtime_t parseTime(const char *time) const;
};

class FeatureTrack
{
public:
	FeatureTrack(xml_reader_t *reader);
	bool isValid() const { return feature != nullptr;}
	Feature *getFeature() const { return feature; }
	void setPlaylist(Playlist *p) { playlist = p; }
	const Playlist *getPlaylist() const { return playlist; }
	const std::list<std::string> &getPlaylists() const { return playlists; }
private:
	Feature *feature;
	Playlist *playlist;
	std::list<std::string> playlists;
};

std::ostream &operator<<(std::ostream &out, Entry const &entry) 
{
	out << entry.producer << ", in: " << entry.interval.in << ", out: " << entry.interval.out;
	return out;
}

std::ostream &operator<<(std::ostream &out, Playlist const &playlist) 
{
	out << "~~~~~" << playlist.id;
	if (playlist.entries.empty())
	{
		out << " empty" << std::endl;
	}
	else 
	{
		out << " entries:" << std::endl;
	}
	for (const Entry &e: playlist.entries)
    {
		out << "~~~~~~~" << e << std::endl;
	}
	return out;
}

std::ostream &operator<<(std::ostream &out, Producer const &producer) 
{
	out << "~~~~~" << producer.id << ", " << producer.resource;
	return out;
}

Playlist::Playlist(xml_reader_t *reader, float frameLen)
{
	if (xml_ReaderIsEmptyElement(reader)) { return; }
	
	const char *value;
	xml_ReaderNextAttr(reader, &value);
	id = std::string(value); 
	
	if (id == "main_bin") { skipToEnd(reader); return; }
	
	const char *nodeC;
	int type = xml_ReaderNextNode(reader, &nodeC);
	bool empty = xml_ReaderIsEmptyElement(reader);
	std::string node(nodeC);
#ifdef DEBUG_PROJECT_PARSING
	msg_Dbg(Project::getVlcObj(), "~~~~~~Project playlist: id = %s, type = %i, node = %s, empty = %i", 
		id.c_str(), type, nodeC, empty);
#endif	
	if (node != "blank" && node != "entry")
	{
		skipToEnd(reader);
		return;
	}
	
	mtime_t prevEndTime = 0;
	do
	{
		if (node == "blank")
		{
			const char *length;
			xml_ReaderNextAttr(reader, &length);
			prevEndTime += parseTime(length);
		}
		else if (node == "entry")
		{
			const char *producer;
			const char *in;
			const char *out;
			xml_ReaderNextAttr(reader, &producer);
			xml_ReaderNextAttr(reader, &in);
			xml_ReaderNextAttr(reader, &out);
			
			mtime_t beginTime = prevEndTime;
			mtime_t endTime = prevEndTime + parseTime(out) + frameLen;
			prevEndTime = endTime;
			int8_t intensity = 0;
			
			if (!empty)
			{
				const char *inodeC;
				type = xml_ReaderNextNode(reader, &inodeC);
				empty = xml_ReaderIsEmptyElement(reader);
				std::string inode(inodeC);
				
#ifdef DEBUG_PROJECT_PARSING		
		msg_Dbg(Project::getVlcObj(), "~~~~~~~~Project playlist: id = %s, type = %i, node = %s, empty = %i", 
			id.c_str(), type, inode.c_str(), empty);
#endif
				do
				{
					if (inode == "property" && !empty)
					{
						const char *pName;
						xml_ReaderNextAttr(reader, &pName);
						if (std::string(pName) == "kdenlive:intensity")
						{
							const char *data;
							xml_ReaderNextNode(reader, &data);
							intensity = atoi(data);
						}				
					}
					type = Project::nextSibling(reader, inode, empty, inode);
					empty = xml_ReaderIsEmptyElement(reader);
				} while (!(type == XML_READER_ENDELEM && inode == "entry"));
				empty = true;
			}
			
			entries.push_back(Entry(beginTime, endTime, intensity, producer));
		}
		
		type = Project::nextSibling(reader, node, empty, node);
		empty = xml_ReaderIsEmptyElement(reader);
#ifdef DEBUG_PROJECT_PARSING		
		msg_Dbg(Project::getVlcObj(), "~~~~~~Project playlist: id = %s, type = %i, node = %s, empty = %i", 
			id.c_str(), type, node.c_str(), xml_ReaderIsEmptyElement(reader));
#endif
	}
	while (!(type == XML_READER_ENDELEM && node == "playlist"));
}

void Playlist::bindProducers(const std::list<Producer *> &producers)
{
	feature = false;
	for (Entry &e: entries)
	{
		for (Producer *p: producers)
		{
			if (p->getName() == e.getProducerName())
			{
				e.setProducer(p);
				if (p->isFeature()) { feature = true; }
				break;
			}
		}
	}
}

bool Playlist::updatePath(const std::string &parentPath)
{
	bool res = true;
	for (Entry &e: entries)
	{
		res &= e.updatePath(parentPath);
	}
	return res;
}

void Playlist::skipToEnd(xml_reader_t *reader) const
{
	const char *node;
	int type;
	while (true)
	{
		type = xml_ReaderNextNode(reader, &node);
#ifdef DEBUG_PROJECT_PARSING
		msg_Dbg(Project::getVlcObj(), "~~~~~~~~Project skip:     type = %i, node = %s, empty = %i", 
			type, node, xml_ReaderIsEmptyElement(reader));
#endif
		if (type == XML_READER_ENDELEM && std::string(node) == "playlist") { return; }
	}
}

mtime_t Playlist::parseTime(const char *time) const
{
	unsigned hours, min, sec, msec;
	int num = sscanf(time, "%d:%d:%d.%d", &hours, &min, &sec, &msec);
	if (num != 4) return 0;
	return ((hours * 60 * 60 + min * 60 + sec) * 1000 + msec) * 1000;
}

FeatureTrack::FeatureTrack(xml_reader_t *reader): feature(nullptr), playlist(nullptr)
{
	if (xml_ReaderIsEmptyElement(reader)) { return; }
	
	const char *nodeC;
	int type = xml_ReaderNextNode(reader, &nodeC);
	bool empty = xml_ReaderIsEmptyElement(reader);
	std::string node(nodeC);
	
	std::string name, description;
	int recMin = 0, recMax = 0;
	bool isFeature = false;
	
	do
	{
		if (node == "property" && !empty)
		{
			const char *pName;
			xml_ReaderNextAttr(reader, &pName);
			std::string property(pName);
			
			const char *data;
			xml_ReaderNextNode(reader, &data);
			
			if (property == "kdenlive:feature_rec_min")
			{
				recMin = atoi(data);
			}
			else if (property == "kdenlive:feature_rec_max")
			{
				recMax = atoi(data);
			}
			else if (property == "kdenlive:track_name")
			{
				name = std::string(data);
			}
			else if (property == "kdenlive:feature_description")
			{
				description = std::string(data);
			}
			else if (property == "kdenlive:feature_track")
			{
				isFeature = true;
			}
			
#ifdef DEBUG_PROJECT_PARSING		
		msg_Dbg(Project::getVlcObj(), "~~~~~~Project track: property = %s, data = %s", 
			property.c_str(), data);
#endif
		}
		else if (node == "track")
		{
			const char *value, *attr;
			while ((attr = xml_ReaderNextAttr(reader, &value)) != NULL)
			{
				if (std::string(attr) == "producer") { playlists.push_back(value); }
			}
		}	
		
#ifdef DEBUG_PROJECT_PARSING		
		msg_Dbg(Project::getVlcObj(), "~~~~~~Project track: node = %s, empty = %i", 
			node.c_str(), empty);
#endif
		type = Project::nextSibling(reader, node, empty, node);
		empty = xml_ReaderIsEmptyElement(reader);
	}
	while (!(type == XML_READER_ENDELEM && node == "tractor"));
	
	if (isFeature)
	{
		feature  = new Feature(name, description, recMin, recMax);
	}
	
#ifdef DEBUG_PROJECT_PARSING		
		msg_Dbg(Project::getVlcObj(), "~~~~~~~~~~~~~~FeatureTrack: %s %s %i %i", 
			name.c_str(), description.c_str(), recMin, recMax);
#endif
}

Producer::Producer(xml_reader_t *reader)
{
	feature = false;
	if (xml_ReaderIsEmptyElement(reader)) { return; }
	
	const char *value, *attr;
	while ((attr = xml_ReaderNextAttr(reader, &value)) != NULL)
	{
		if (std::string(attr) == "id") { id = std::string(value); break; }
	}
	if (id.empty()) return;
	
	const char *nodeC;
	int type = xml_ReaderNextNode(reader, &nodeC);
	bool empty = xml_ReaderIsEmptyElement(reader);
	std::string node(nodeC);
	
	do
	{
		if (node == "property")
		{
			const char *pName;
			xml_ReaderNextAttr(reader, &pName);
			std::string property(pName);
			
			if (property == "resource")
			{
				const char *resourceC;
				xml_ReaderNextNode(reader, &resourceC);
				resource = std::string(resourceC);
			}
			else if (property == "kdenlive:clipname" && !empty)
			{
				const char *clipname;
				xml_ReaderNextNode(reader, &clipname);
				resource = std::string(clipname);
				if (resource == "feature_binclip") { feature = true; }
			}
		}
		
		type = Project::nextSibling(reader, node, empty, node);
		empty = xml_ReaderIsEmptyElement(reader);
#ifdef DEBUG_PROJECT_PARSING		
		msg_Dbg(Project::getVlcObj(), "~~~~~~Project producer: id = %s, type = %i, node = %s, empty = %i", 
			id.c_str(), type, node.c_str(), xml_ReaderIsEmptyElement(reader));
#endif
	}
	while (!(type == XML_READER_ENDELEM && node == "producer"));
}

bool Producer::updatePath(const std::string &parentPath)
{
	std::filesystem::path resourcePath(resource);
	
	resource = parentPath/resourcePath.filename();
	if (FILE *file = fopen(resource.c_str(), "r")) 
	{
		fclose(file);
		return true;
	}
	
	std::filesystem::path resDirectory;
	for(const std::filesystem::path &e: resourcePath.parent_path())
	{
		resDirectory = e;
	}
	
	resource = parentPath/resDirectory/resourcePath.filename();
	if (FILE *file = fopen(resource.c_str(), "r")) 
	{
		fclose(file);
		return true;
	}
	
	return false;
}

Project::Project(vlc_object_t *nobj, const char *file, stream_t *stream)
{
	obj = nobj;
	valid = false;
	mainPlaylist = nullptr;

	if (std::filesystem::path(file).extension() != ".kdenlive") return;
	
	xml_reader_t *reader = xml_ReaderCreate(obj, stream);
	if (!reader) return;
	
	const char *nodeC;
	int type;
	std::string node;
	do
	{
		type = xml_ReaderNextNode(reader, &nodeC);
		node = std::string(nodeC);
	}
	while (node != "profile");
	bool isEmpty = xml_ReaderIsEmptyElement(reader);
	
	const char *value, *attr;
	int frame_rate_num = 0;
	int frame_rate_den = 1;
	while ((attr = xml_ReaderNextAttr(reader, &value)) != NULL)
	{
		if (std::string(attr) == "frame_rate_num") { frame_rate_num = atoi(value); }
		else if (std::string(attr) == "frame_rate_den") { frame_rate_den = atoi(value); }
	}
	fps = (float)frame_rate_num / frame_rate_den;
	
	while (!(type == XML_READER_ENDELEM && node == "mlt"))
	{
		type = nextSibling(reader, node, isEmpty || type == XML_READER_ENDELEM, node);
		
		isEmpty = xml_ReaderIsEmptyElement(reader);
#ifdef DEBUG_PROJECT_PARSING
		msg_Dbg(obj, "~~~~Project type = %i, node = %s, empty = %i", 
			type, node.c_str(), isEmpty);
#endif
		
		if (node == "playlist")
		{
			Playlist *p = new Playlist(reader, getFrameLen());
			if (!p->isEmpty())
			{
				playlists.push_back(p);
			}
			else { delete p; }
			type = XML_READER_ENDELEM;
		}
		else if (node == "producer")
		{
			Producer *p = new Producer(reader);
			producers.push_back(p);
			type = XML_READER_ENDELEM;
		}
		else if (node == "tractor")
		{
			FeatureTrack *f = new FeatureTrack(reader);
			if (f->isValid()) { tracks.push_back(f); }
			type = XML_READER_ENDELEM;
		}
	}
	
	valid = bindProducers();
	valid &= bindTracks();
	if (mainPlaylist)
	{
		valid &= mainPlaylist->updatePath(std::filesystem::path(file).parent_path());
	}
	
	
	std::stringstream ss;
	ss << std::endl;
	ss << "~~~~" << getFrameLen() << std::endl;
	for (Playlist *p: playlists)
	{
		ss << *p;
	}
	
	for (Producer *p: producers)
	{
		ss << *p << std::endl;
	}
	
	msg_Dbg(obj, "%s", ss.str().c_str());
	
	xml_ReaderDelete(reader);
}

FeatureList *Project::generateFeatureList() const
{
	FeatureList *flist = new FeatureList();
	
	for (FeatureTrack *track: tracks)
	{
		Feature *feature = track->getFeature();
		for (const Entry &entry: track->getPlaylist()->getEntries())
		{
			feature->appendInterval(entry.getInterval());
		}
		flist->push_back(feature);
	}
	
	std::stringstream ss;
	ss << "~~~~Features num: " << flist->size() << std::endl; 
	for (Ntff::Feature *f: *flist)
	{
		ss << "~~~~~~" << *f;
	}
	msg_Dbg(obj, "%s", ss.str().c_str());
	
	return flist;
}

Player *Project::createPlayer() const
{
	Player *player = new Player(obj, generateFeatureList());
	for (const Entry &entry: mainPlaylist->getEntries())
	{
		player->addFile(entry.getInterval(), entry.getResource());
	}
	player->reset();
	return player;
}

int Project::nextSibling(xml_reader_t *reader, const std::string &curNode, bool curEmpty, std::string &resNode)
{
	const char *node;
	int type;
	if (!curEmpty)
	{
		do
		{
			type = xml_ReaderNextNode(reader, &node);
		}
		while (!(type == XML_READER_ENDELEM && std::string(node) == curNode));
	}
	
	type = xml_ReaderNextNode(reader, &node);
	resNode = std::string(node);
	return type;
}

bool Project::bindProducers()
{
	for (Playlist *p: playlists)
	{
		p->bindProducers(producers);
		if (!p->isFeature()) 
		{ 
			if (mainPlaylist != nullptr)
			{
				msg_Err(obj, "Multiple main playlists");
				return false;
			}
			mainPlaylist = p; 
		}
	}
	
	if (mainPlaylist == nullptr)
	{
		msg_Err(obj, "Main playlist not found");
		return false;
	}
	
	return true;
}

bool Project::bindTracks()
{
	for (FeatureTrack *track: tracks)
	{
		bool found = false;
		for (const std::string &playlistName: track->getPlaylists())
		{
			for (Playlist *playlist: playlists)
			{
				if (playlist->getName() == playlistName)
				{
					track->setPlaylist(playlist);
					found = true;
					break;
				}
			}
		}
		if (!found)
		{
			msg_Err(obj, "No playlist for feature track");
			return false;
		}
	}
	return true;
}

bool Entry::updatePath(const std::string &parentPath) 
{ 
	return producerPtr->updatePath(parentPath);
}



}
