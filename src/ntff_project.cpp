#include "ntff_project.h"
#include <string>
#include <vlc_common.h>
#include <vlc_xml.h>

namespace Ntff {

Project::Project(vlc_object_t *obj, const char *file, stream_t *stream)
{
	valid = false;
	std::string filename(file);
	
	std::size_t found = filename.find(".kdenlive");
	if (found == std::string::npos) return;
	
	
	xml_reader_t *reader = xml_ReaderCreate(obj, stream);
	if (!reader) return;
	
	int i = 4;
	
	int type = XML_READER_NONE;
	//while (type != XML_READER_STARTELEM)
	while (i--)
	{
		const char *node;
		int type = xml_ReaderNextNode(reader, &node);
		msg_Dbg(obj, "~~~~Project type = %i, node = %s", 
			type, node);
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



}
