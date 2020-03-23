#ifndef NTFF_PROJECT_H
#define NTFF_PROJECT_H

struct stream_t;
struct vlc_object_t;
struct xml_reader_t;

namespace Ntff {

class Project
{
	public:
		Project(vlc_object_t *obj, const char *file, stream_t *stream);
		bool isValid() const { return valid; }
		static int nextSibling(xml_reader_t *reader, const char **resNode, int curType);
	private:
		vlc_object_t *obj;
		bool valid;
};


}


#endif // NTFF_PROJECT_H
