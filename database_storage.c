/* filename: database_storage.c
 *  chiefengineer
 *  Sat Feb 26 12:54:43 PST 2011
 */

#include <Eina.h>
#include <Ecore_Ipc.h>

#include "media_storage.h"
#include "database.h"

#include <stdio.h>

static Database* db = NULL;

static Eina_Bool _init(void* data)
{
	const char* db_path = data;
	database_init(db_path);
	db = database_new();
	
	return (db != NULL ? EINA_TRUE : EINA_FALSE);
}

static void _shutdown()
{
	if (db) { database_free(db); }
}

static Eina_Bool _media_add(Rage_Ipc_VolItem* item)
{
	Volume_Item* vol;
	printf("adding %s:%s %ld\n", item->genre, item->name, item->created_date);
	
	vol = volume_item_new(0, item->path, item->name, item->genre, item->type);
	database_video_file_add(db, vol);
	volume_item_free(vol);
	
	return EINA_TRUE;
}

static Eina_Bool _media_del(const char* path)
{
	printf("deleting %s\n", path);
	database_video_file_del(db, path);
	return EINA_TRUE;
}

static Rage_Ipc_VolItem* _media_details_get(ID_Item* item)
{
	Rage_Ipc_VolItem* res;
	DBIterator* db_it = database_video_files_id_get(db, item->ids[0]);
	
	if (db_it)
		{
			Volume_Item* db_item = database_iterator_next(db_it);
			if (db_item)
				{
					res = rage_ipc_volitem_new(db_item->id,
																		 db_item->path,
																		 db_item->name,
																		 db_item->genre,
																		 db_item->type,
																		 0);
					
					volume_item_free(db_item);
				}
			
		}
	
	return res;
}

static Genre_Result* _genre_list(const char* filter)
{
	Genre_Result* gr = NULL;
	Eina_List* lst = NULL;
	Genre* g = NULL;
	unsigned int len = 0;
	
	DBIterator* db_it;
	db_it = database_video_genres_get(db, filter);
	
	printf("filter: %s\n", filter);
	
	if (db_it)
		{
			printf("foo!\n");
			g = database_iterator_next(db_it);
			while(g)
				{
					lst = eina_list_append(lst, g);
					
					g = database_iterator_next(db_it);
				}
			database_iterator_free(db_it);
			
			len = eina_list_count(lst);
			gr = calloc(1, sizeof(Genre_Result) + len * sizeof(Genre_Result_Item));
			if (gr)
				{
					memset(gr, 0, sizeof(Genre_Result) + len * sizeof(Genre_Result_Item));
					gr->size = sizeof(Genre_Result) + len * sizeof(Genre_Result_Item);
					gr->count = len;
				}
			
			{
				Eina_List* ptr;
				Genre* gPtr;
				unsigned int i = 0;
				
				EINA_LIST_FOREACH(lst, ptr, gPtr)
					{
						if (gr)
							{
								strncpy(gr->recs[i].label, gPtr->label, sizeof(gr->recs[i].label));
								gr->recs[i].count = gPtr->count;
								++i;
							}
						
						eina_stringshare_del(gPtr->label);
						free(gPtr);
					}
			}
		}
	
	return gr;
}

static Query_Result* _media_query(MQ_Type t, void* data, int size)
{
	Query_Result* items;
	DBIterator* db_it;
	Eina_List* tmp_list = NULL;
	
	switch (t)
		{
		case (MQ_TYPE_PATH):
			{
				const char* vol_root = data;
				if (size == 0) { vol_root = NULL; }
				
				db_it = database_video_files_path_search(db, vol_root);
				break;
			}
			
		case (MQ_TYPE_GENRE):
			{
				const char* genre = data;
				if (size == 0) { genre = NULL; }
				
				db_it = database_video_files_genre_search(db, genre);
				break;
			}
			
		default:
			break;
		}
	
	if (db_it)
		{
			Volume_Item* dbitem = database_iterator_next(db_it);
			unsigned int len = 0;
			Eina_List* ptr = NULL;
			int i = 0;
			
			while (dbitem)
				{
					tmp_list = eina_list_append(tmp_list, dbitem);
					dbitem = database_iterator_next(db_it);
				}
			database_iterator_free(db_it);
			
			len = eina_list_count(tmp_list);
			items = calloc(1, sizeof(Query_Result) + len * sizeof(Rage_Ipc_VolItem));
			
			if (items)
				{
					memset(items, 0, sizeof(Query_Result) + len * sizeof(Rage_Ipc_VolItem));
					
					items->size = sizeof(Query_Result) + len * sizeof(Rage_Ipc_VolItem);
					items->count = len;
				}
			
			EINA_LIST_FOREACH(tmp_list, ptr, dbitem)
				{
					if (items)
						{
							items->recs[i].id = dbitem->id;
							strncpy(items->recs[i].path, dbitem->path, sizeof(items->recs[i].path));
							strncpy(items->recs[i].name, dbitem->name, sizeof(items->recs[i].name));
							strncpy(items->recs[i].genre, dbitem->genre, sizeof(items->recs[i].genre));
							strncpy(items->recs[i].type, dbitem->type, sizeof(items->recs[i].type));
						}
					
					volume_item_free(dbitem);
					++i;
				}
			eina_list_free(tmp_list);
		}
	
	return items;
}

Media_Storage DB_Store =
	{
		"Database Store",
		_init,
		_shutdown,
		
		_media_add,
		_media_del,
		_media_details_get,
		
		_genre_list,
		_media_query,
	}
	;

