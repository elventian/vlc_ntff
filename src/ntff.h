#ifndef NTFF_H_INCLUDED
#define NTFF_H_INCLUDED

#include <vlc_es.h>
#include <vlc_es_out.h>
#include <vlc_demux.h>

typedef struct
{
	int64_t begin;
	int64_t end;
} time_interval;

typedef struct
{
	time_interval scene[3];
	int scenes_num;
	int cur_scene;
	bool need_skip_scene;
	int64_t skipped_time;
} scene_list;

#endif
