
#ifndef _VOLUME_H_
#define _VOLUME_H_

typedef struct _Volume_Item Volume_Item;

struct _Volume_Item
{
	long long id;
	char       *path;
	char       *rpath;
	char       *name;
	const char *genre;
	const char *type;
	double      last_played;
	int         play_count;
	double      last_pos;
	double      length;
	char       *artist;
	char       *album;
	int         track;
};

Volume_Item* volume_item_new(const long long id, 
														 const char* path, 
														 const char* name, 
														 const char* genre, 
														 const char* type);
Volume_Item* volume_item_copy(const Volume_Item* item);
void volume_item_free(Volume_Item* item);

#endif
