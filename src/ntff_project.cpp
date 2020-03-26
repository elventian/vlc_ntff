#include "ntff_project.h"
#include <list>
#include <sstream>
#include <vlc_common.h>
#include <vlc_xml.h>

namespace Ntff {

class  Playlist
{
public:
	 Playlist(xml_reader_t *reader);
private:
	 std::string id;
	 bool hasProducer;
	 void skipToEnd(xml_reader_t *reader) const;
	 mtime_t parseTime(const char *time) const;
};

vlc_object_t *print_obj;
Playlist::Playlist(xml_reader_t *reader)
{
	hasProducer = false;
	
	if (xml_ReaderIsEmptyElement(reader)) { return; }
	
	const char *value;
	xml_ReaderNextAttr(reader, &value);
	id = std::string(value); 
	
	if (id == "main_bin") { skipToEnd(reader); return; }
	//if (id == "playlist0") { return; }
	
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
	
	do
	{
		if (node == "blank")
		{
			const char *length;
			
			xml_ReaderNextAttr(reader, &length);
			parseTime(length);
			
			/*type = Project::nextSibling(reader, node, xml_ReaderIsEmptyElement(reader), node);
			msg_Dbg(print_obj, "~~~~~~Project playlist: id = %s, type = %i, node = %s, empty = %i", 
				id.c_str(), type, node.c_str(), xml_ReaderIsEmptyElement(reader));
			return;*/
		}
		else if (node == "entry")
		{
			//skipToEnd(reader);
			//return;
		}
		
		type = Project::nextSibling(reader, node, empty, node);
		empty = xml_ReaderIsEmptyElement(reader);
		
		msg_Dbg(print_obj, "~~~~~~Project playlist: id = %s, type = %i, node = %s, empty = %i", 
			id.c_str(), type, node.c_str(), xml_ReaderIsEmptyElement(reader));
	}
	while (!(type == XML_READER_ENDELEM && node == "playlist"));
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
	std::istringstream iss(time);
	msg_Dbg(print_obj, "~~~~~~~~Project parseTime: %s, %li", time, 42l);
	return 0;
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
	std::list<Playlist *>playlists;
	while (!(type == XML_READER_ENDELEM && node == "mlt"))
	{
		type = nextSibling(reader, node, xml_ReaderIsEmptyElement(reader), node);
		
		bool isEmpty = xml_ReaderIsEmptyElement(reader);
		msg_Dbg(obj, "~~~~Project type = %i, node = %s, empty = %i", 
			type, node.c_str(), isEmpty);
		
		if (node == "playlist")
		{
			playlists.push_back(new Playlist(reader));
			type = XML_READER_ENDELEM;
		}
		
		if (playlists.size() == 4) break;
	}
	
	/*
	int type = XML_READER_NONE;
    do
    {
        type = xml_ReaderNextNode(reader, &node );
        if( type <= 0 )
        {
            msg_Err( p_demux, "can't read xml stream" );
            goto end;
        }
    }
    while( type != XML_READER_STARTELEM );
	*/
	
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
	
	/*if (curType == XML_READER_STARTELEM && !curEmpty)
	{
		int startNum = 1, endNum = 0;
		while (startNum > endNum)
		{
			int nextType = xml_ReaderNextNode(reader, resNode);
			if (nextType == XML_READER_ENDELEM)	{ endNum++;	}
			else if (nextType == XML_READER_STARTELEM && !xml_ReaderIsEmptyElement(reader)) { startNum++; }
			
			//msg_Dbg(obj, "~~~~Project nextSibling type = %i, node = %s, empty = %i", 
			//	nextType, *resNode, xml_ReaderIsEmptyElement(reader));
		}
	}
	
	return xml_ReaderNextNode(reader, resNode);*/
}



}
