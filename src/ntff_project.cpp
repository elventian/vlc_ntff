#include "ntff_project.h"
#include <list>
#include <cstdio>
#include <sstream>
#include <vlc_common.h>
#include <vlc_xml.h>

namespace Ntff {

class Entry
{
	friend std::ostream &operator<<(std::ostream &out, Entry const &entry);
public:
	Entry(mtime_t in, mtime_t out, const char *producer): 
		in(in), out(out), producer(producer), producerPtr(nullptr) {}
	const std::string &getProducerName() const { return producer; }
	void setProducer(Producer *producer) { producerPtr = producer; }
private:
	mtime_t in;
	mtime_t out;
	std::string producer;
	Producer *producerPtr;
};

class  Playlist
{
	friend std::ostream &operator<<(std::ostream &out, Playlist const &playlist);
public:
	 Playlist(xml_reader_t *reader);
	 bool isEmpty() { return entries.empty(); }
	 void bindProducers(const std::list<Producer *>&producers);
	 bool isFeature() const { return feature; }
private:
	 std::string id;
	 std::list<Entry> entries;
	 bool feature;
	 void skipToEnd(xml_reader_t *reader) const;
	 mtime_t parseTime(const char *time) const;
};

class Producer
{
	friend std::ostream &operator<<(std::ostream &out, Producer const &producer);
public:
	 Producer(xml_reader_t *reader);
	 bool isFeature() const { return feature; }
	 const std::string &getName() const { return id; }
private:
	 std::string id;
	 std::string resource;
	 bool feature;
};

std::ostream &operator<<(std::ostream &out, Entry const &entry) 
{
	out << entry.producer << ", in: " << entry.in << ", out: " << entry.out;
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

vlc_object_t *print_obj;
Playlist::Playlist(xml_reader_t *reader)
{
	if (xml_ReaderIsEmptyElement(reader)) { return; }
	
	const char *value;
	xml_ReaderNextAttr(reader, &value);
	id = std::string(value); 
	
	if (id == "main_bin") { skipToEnd(reader); return; }
	
	const char *nodeC;
	int type = xml_ReaderNextNode(reader, &nodeC);
	bool empty = xml_ReaderIsEmptyElement(reader);
	
	msg_Dbg(print_obj, "~~~~~~Project playlist: id = %s, type = %i, node = %s, empty = %i", 
		id.c_str(), type, nodeC, empty);
	
	std::string node(nodeC);
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
			mtime_t endTime = prevEndTime + parseTime(out);
			
			entries.push_back(Entry(beginTime, endTime, producer));
			prevEndTime = endTime;
		}
		
		type = Project::nextSibling(reader, node, empty, node);
		empty = xml_ReaderIsEmptyElement(reader);
		
		msg_Dbg(print_obj, "~~~~~~Project playlist: id = %s, type = %i, node = %s, empty = %i", 
			id.c_str(), type, node.c_str(), xml_ReaderIsEmptyElement(reader));
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

void Playlist::skipToEnd(xml_reader_t *reader) const
{
	const char *node;
	int type;
	while (true)
	{
		type = xml_ReaderNextNode(reader, &node);
		msg_Dbg(print_obj, "~~~~~~~~Project skip:     type = %i, node = %s, empty = %i", 
			type, node, xml_ReaderIsEmptyElement(reader));
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
		
		msg_Dbg(print_obj, "~~~~~~Project producer: id = %s, type = %i, node = %s, empty = %i", 
			id.c_str(), type, node.c_str(), xml_ReaderIsEmptyElement(reader));
	}
	while (!(type == XML_READER_ENDELEM && node == "producer"));
}

Project::Project(vlc_object_t *obj, const char *file, stream_t *stream): obj(obj)
{
	print_obj = obj;
	valid = false;
	std::string filename(file);
	
	std::size_t found = filename.find(".kdenlive");
	if (found == std::string::npos) return;
	
	
	xml_reader_t *reader = xml_ReaderCreate(obj, stream);
	if (!reader) return;
	
	int i = 2;
	
	
	const char *nodeC;
	int type;
	while (i--)
	{
		
		type = xml_ReaderNextNode(reader, &nodeC);
	}
	
	std::string node(nodeC);
	while (!(type == XML_READER_ENDELEM && node == "mlt"))
	{
		type = nextSibling(reader, node, xml_ReaderIsEmptyElement(reader) || type == XML_READER_ENDELEM, node);
		
		bool isEmpty = xml_ReaderIsEmptyElement(reader);
		msg_Dbg(obj, "~~~~Project type = %i, node = %s, empty = %i", 
			type, node.c_str(), isEmpty);
		
		if (node == "playlist")
		{
			Playlist *p = new Playlist(reader);
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
	}
	
	updatePlaylistEntries();
	
	std::stringstream ss;
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
	//valid = true;
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

void Project::updatePlaylistEntries()
{
	for (Playlist *p: playlists)
	{
		p->bindProducers(producers);
		if (!p->isFeature()) { main = p; }
	}
	playlists.remove(main);
}



}
