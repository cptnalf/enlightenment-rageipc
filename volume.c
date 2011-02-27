
#include <Eina.h>
#include <Ecore_File.h>
#include <stdio.h>
#include <stdlib.h>
#include "volume.h"
#include <string.h>
extern int debug;


Volume_Item* volume_item_new(const long long id, 
														 const char* path, 
														 const char* name, 
														 const char* genre, 
														 const char* type)
{
	Volume_Item* item = calloc(1, sizeof(Volume_Item));
	
	item->id = id;
	item->path = strdup(path);
	item->rpath = ecore_file_realpath(item->path);
	if (name) { item->name = strdup(name); }
	if (genre) { item->genre = eina_stringshare_add(genre); }
	if (type) { item->type = eina_stringshare_add(type); }
	
	return item;
}

Volume_Item* volume_item_copy(const Volume_Item* item)
{
	Volume_Item* item_copy = calloc(1, sizeof(Volume_Item));
	
	item_copy->id = item->id;
	item_copy->path = strdup(item->path);
	item_copy->rpath = ecore_file_realpath(item->rpath);
	if (item->name)  { item_copy->name = strdup(item->name); }
	if (item->genre) { item_copy->genre = eina_stringshare_add(item->genre); }
	if (item->type)  { item_copy->type = eina_stringshare_add(item->type); }
	
	item_copy->last_played = item->last_played;
	item_copy->play_count = item->play_count;
	item_copy->last_pos = item->last_pos;
	item_copy->length = item->length;

	return item_copy;
}

void volume_item_free(Volume_Item* item)
{
	if (item)
		{
			if (debug) 
				{	printf("0x%X;0x%X;0x%X;0x%X\n", 
								 (unsigned int)item, (unsigned int)item->path, 
								 (unsigned int)item->name, (unsigned int)item->genre); }
			
			free(item->path);
			free(item->rpath);
			free(item->name);
			if (item->genre) { eina_stringshare_del(item->genre); }
			if (item->type) { eina_stringshare_del(item->type); }
			
			free(item);
		}
}
