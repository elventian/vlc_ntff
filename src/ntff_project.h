#ifndef NTFF_PROJECT_H
#define NTFF_PROJECT_H

struct stream_t;
struct vlc_object_t;

namespace Ntff {

class Project
{
	public:
		Project(vlc_object_t *obj, const char *file, stream_t *stream);
		bool isValid() const { return valid; }
	private:
		bool valid;
};


}


#endif // NTFF_PROJECT_H
