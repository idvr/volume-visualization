#ifndef filename_utility_h
#define filename_utility_h

#include <cstring>

#ifdef MAX_PATH
#define MAX_STR_SIZE MAX_PATH
#else
#define MAX_STR_SIZE 320
#endif

namespace filename_utility
{
	const char *get_file_path_separator_position(const char *filename)
	{
		const char *p = strrchr(filename, '\\');
		if (NULL == p)
		{
			p = strrchr(filename, '/');
		}
		return p;
	}

	void get_raw_filename_from_dat_filename(const char *dat_filename, const char *raw_filename, char *dest_str_buffer)
	{
		// get the raw file path and filename
		strcpy(dest_str_buffer, dat_filename);
		const char *p = get_file_path_separator_position(dest_str_buffer);
		if (NULL == p)
		{
			strcpy(dest_str_buffer, raw_filename);
		}else
		{
			strcpy((char *)(p) + 1, raw_filename);
		}
	}

	void print_about(int argc, char* argv[])
	{
		char about[MAX_STR_SIZE * 4] = 
"//////////////////////////////////////////////////////////////////////////\n\
GPU raycasting demo\n\
Copyright (c) 2010 Shengzhou Luo, Shenzhen Institute of Advanced Technology, Chinese Academy of Sciences.\n\
\n\
Usage:\n\
%s [source]\n\
source - File to load.\n\
//////////////////////////////////////////////////////////////////////////\n\n";
		const char *p = filename_utility::get_file_path_separator_position(argv[0]);
		if (NULL == p)
		{
			printf(about, argv[0]);
		}else
		{
			printf(about, p + 1);
		}
	}
}

#endif // filename_utility_h