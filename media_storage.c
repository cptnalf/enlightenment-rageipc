/* filename: media_storage.c
 *  chiefengineer
 *  Sat Feb 26 15:47:07 PST 2011
 */

#include "media_storage.h"

Rage_Ipc_VolItem* rage_ipc_volitem_new(const long long id,
																			 const char* path,
																			 const char* name,
																			 const char* genre,
																			 const char* type,
																			 time_t created)
{
	Rage_Ipc_VolItem* item = calloc(1, sizeof(Rage_Ipc_VolItem));
	
	item->id = id;
	strncpy(item->path, path, sizeof(item->path));
	strncpy(item->name, name, sizeof(item->name));
	strncpy(item->genre, genre, sizeof(item->genre));
	strncpy(item->type, type, sizeof(item->type));
	item->created_date = created;
	
	return item;
}
