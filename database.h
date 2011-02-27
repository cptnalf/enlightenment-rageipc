
/* (c) 2010 saa
 */
#ifndef _DATABASE_H_
#define _DATABASE_H_

#include <sqlite3.h>
#include "volume.h"

typedef struct _Database Database;
struct _Database
{
	sqlite3* db;
};

typedef struct _Genre Genre;

/** info about a genre. */
struct _Genre
{
	const char *label;
	int count;
};

typedef struct _DBIterator DBIterator;
typedef void* (NextItemFx)(DBIterator* it);
typedef void (FreeFx)(DBIterator* it);

struct _DBIterator
{
	char** tbl_results;
	int cols;
	int rows;
	int pos;
	
	NextItemFx* next_fx;
	FreeFx* free_fx;
};

extern void database_init(const char* path);
extern Database* database_new();
extern void database_free(Database* db);
extern void* database_iterator_next(DBIterator* it);
extern void* database_iterator_get(DBIterator* it);
extern int database_iterator_move_next(DBIterator* it);
extern void database_iterator_free(DBIterator* it);
extern DBIterator* database_video_genres_get(Database* db, const char* genre);
extern void database_iterator_free(DBIterator* it);
extern DBIterator* database_video_files_get(Database* db, const char* where_part, const char* orderby_part);
extern DBIterator* database_video_files_id_get(Database* db, const long long id);
extern DBIterator* database_video_files_genre_search(Database* db, const char* genre);
extern DBIterator* database_video_files_path_search(Database* db, const char* path);
extern void database_video_file_del(Database* db, const char* path);
extern void database_video_file_add(Database* db, const Volume_Item* item);
extern DBIterator* database_video_favorites_get(Database* db);
extern DBIterator* database_video_recents_get(Database* db);
extern DBIterator* database_video_news_get(Database* db);

#endif
