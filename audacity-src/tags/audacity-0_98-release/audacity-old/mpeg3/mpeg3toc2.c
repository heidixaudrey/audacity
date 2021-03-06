#include "libmpeg3.h"
#include "mpeg3protos.h"

#define TOCVERSION 3
#define TOCVIDEO 4

int mpeg3_generate_toc(FILE *output, char *path, int timecode_search, int print_streams)
{
	mpeg3_t *file = mpeg3_open(path);
	mpeg3_demuxer_t *demuxer;
	int i;

	if(file)
	{
		demuxer = mpeg3_new_demuxer(file, 0, 0, -1);
		if(file->is_ifo_file)
		{
			int i;
			mpeg3io_open_file(file->fs);
			mpeg3demux_read_ifo(file, demuxer, 1);
			mpeg3io_close_file(file->fs);

			for(i = 0; i < demuxer->total_titles; i++)
			{
				fprintf(output, "TOCVERSION %d\n" 
					"PATH: %s\n"
					"SIZE: %ld\n"
					"PACKETSIZE: %ld\n",
					TOCVERSION,
					demuxer->titles[i]->fs->path,
					demuxer->titles[i]->total_bytes,
					demuxer->packet_size);
				
/* Just print the first title's streams */
				if(i == 0)
					mpeg3demux_print_streams(demuxer, output);

				mpeg3demux_print_timecodes(demuxer->titles[i], output);
			}

			return 0;
		}
		else
		{
			char complete_path[MPEG3_STRLEN];
			mpeg3io_complete_path(complete_path, path);

			fprintf(output, "TOCVERSION %d\n" 
				"PATH: %s\n", TOCVERSION, complete_path);
			mpeg3demux_create_title(demuxer, timecode_search, output);
/* Just print the first title's streams */
			if(print_streams) mpeg3demux_print_streams(demuxer, output);

			if(file->is_transport_stream || file->is_program_stream)
			{
				fprintf(output, "SIZE: %ld\n", demuxer->titles[demuxer->current_title]->total_bytes);
				fprintf(output, "PACKETSIZE: %ld\n", demuxer->packet_size);
			}

			mpeg3demux_print_timecodes(demuxer->titles[demuxer->current_title], output);
			return 0;
		}

		mpeg3_delete_demuxer(demuxer);
		mpeg3_close(file);
	}
	return 1;
}

/* Read the title information from a toc */
static int read_titles(mpeg3_demuxer_t *demuxer, int version)
{
	char string1[MPEG3_STRLEN], string2[MPEG3_STRLEN];
	long start_byte, end_byte;
	float start_time, end_time;
	float program;
	mpeg3_title_t *title = 0;
	mpeg3_t *file = demuxer->file;

// Eventually use IFO file to generate titles
	while(!feof(file->fs->fd))
	{
		fscanf(file->fs->fd, "%s %s %ld %f %f %f %f", 
			string1,
			string2,
			&end_byte, 
			&start_time, 
			&end_time,
			&program);

		if(!strncasecmp(string1, "PATH:", 5))
		{
			title = demuxer->titles[demuxer->total_titles++] = mpeg3_new_title(file, string2);
		}
		else
		if(title)
		{
			start_byte = atol(string2);
			if(!strcasecmp(string1, "REGION:"))
			{
				mpeg3_append_timecode(demuxer, 
					title, 
					0, 
					0, 
					0, 
					0,
					1,
					0);
				title->timecode_table[title->timecode_table_size - 1].start_byte = start_byte;
				title->timecode_table[title->timecode_table_size - 1].end_byte = end_byte;
				title->timecode_table[title->timecode_table_size - 1].start_time = start_time;
				title->timecode_table[title->timecode_table_size - 1].end_time = end_time;
				title->timecode_table[title->timecode_table_size - 1].program = program;
			}
			else
			if(!strcasecmp(string1, "ASTREAM:"))
				demuxer->astream_table[start_byte] = end_byte;
			else
			if(!strcasecmp(string1, "VSTREAM:"))
				demuxer->vstream_table[start_byte] = end_byte;
			else
			if(!strcasecmp(string1, "SIZE:"))
				title->total_bytes = start_byte;
			else
			if(!strcasecmp(string1, "PACKETSIZE:"))
				demuxer->packet_size = start_byte;
		}
	}

	mpeg3demux_assign_programs(demuxer);
	mpeg3demux_open_title(demuxer, 0);
	return 0;
}

int mpeg3_read_toc(mpeg3_t *file)
{
	char string[MPEG3_STRLEN];
	int version;

/* Test version number */
	mpeg3io_seek(file->fs, 0);
	fscanf(file->fs->fd, "%s %d", string, &version);
	if(version != TOCVERSION && version != TOCVIDEO) return 1;
	switch(version)
	{
		case TOCVERSION:
			file->is_program_stream = 1;
			break;
		case TOCVIDEO:
			file->is_video_stream = 1;
			break;
	}

/* Read titles */
	read_titles(file->demuxer, version);
	return 0;
}
