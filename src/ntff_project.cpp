#include "ntff_project.h"
#include <string>
#include <list>
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
};

Playlist::Playlist(xml_reader_t *reader)
{
	hasProducer = false;
	
	if (xml_ReaderIsEmptyElement(reader)) { return; }
	
	const char *value;
	xml_ReaderNextAttr(reader, &value);
	id = std::string(value); 
	
	if (id == "main_bin") { return; }
	
	return;
	const char *node;
	int type = xml_ReaderNextNode(reader, &node);
	do
	{
		if (std::string(node) == "blank")
		{
			
		}
		else if (std::string(node) == "entry")
		{
			
		}
		else 
		{
			skipToEnd(reader);
			return;
		}
		
		type = Project::nextSibling(reader, &node, type);
	}
	while (type != XML_READER_ENDELEM && std::string(node) != "playlist");
}

void Playlist::skipToEnd(xml_reader_t *reader) const
{
	const char *node;
	int type = XML_READER_NONE;
	while (type != XML_READER_ENDELEM && std::string(node) != "playlist")
	{
		xml_ReaderNextNode(reader, &node);
	}
}

Project::Project(vlc_object_t *obj, const char *file, stream_t *stream): obj(obj)
{
	valid = false;
	std::string filename(file);
	
	std::size_t found = filename.find(".kdenlive");
	if (found == std::string::npos) return;
	
	
	xml_reader_t *reader = xml_ReaderCreate(obj, stream);
	if (!reader) return;
	
	int i = 2;
	
	
	const char *node;
	int type;
	while (i--)
	{
		
		type = xml_ReaderNextNode(reader, &node);
	}
	
	std::list<Playlist *>playlists;
	while (type != XML_READER_ENDELEM)
	{
		type = nextSibling(reader, &node, type);
		bool isEmpty = xml_ReaderIsEmptyElement(reader);
		msg_Dbg(obj, "~~~~Project type = %i, node = %s, empty = %i", 
			type, node, isEmpty);
		
		if (std::string(node) == "playlist")
		{
			playlists.push_back(new Playlist(reader));
		}
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

int Project::nextSibling(xml_reader_t *reader, const char **resNode, int curType)
{
	if (curType == XML_READER_STARTELEM && !xml_ReaderIsEmptyElement(reader))
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
	
	return xml_ReaderNextNode(reader, resNode);
}



}
