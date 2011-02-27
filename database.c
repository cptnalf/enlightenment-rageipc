/*
 * (c) 2010 saa
 */
#include <Eina.h>
#include <stdlib.h>
#include <stdio.h>
#include "volume.h"
#include "database.h"
#include <string.h>
#include <time.h>

static void* _video_files_next(DBIterator* it);
static char db_path[4096];

void database_init(const char* path)
{
	snprintf(db_path, sizeof(db_path), "%s", path);
}

/** create a connection to a database at path.
 *  @return pointer to the database, or NULL on failure.
 */
Database* database_new()
{
	int result;
	Database* db = calloc(1, sizeof(Database));
	
	//printf("db=%s\n", path);
	
	result = sqlite3_open(db_path, &db->db);
	if (result)
		{
			fprintf(stderr, "error: %s\n", sqlite3_errmsg(db->db));
			sqlite3_close(db->db);
			free(db);
			db = 0;
		}
	
	if (db)
		{
			char* errmsg;
			result = 
				sqlite3_exec(db->db,
										 "CREATE TABLE "
										 " IF NOT EXISTS "
										 "video_files ("
										 "ID INTEGER PRIMARY KEY, "
										 "path TEXT UNIQUE,"  // the path
										 "genre TEXT,"             // genre of the file
										 "title TEXT,"             // title of the file.
										 "f_type TEXT,"            // type of file (video, audio, photo
										 "length INTEGER,"         // length in seconds.
										 "createdDate INTEGER)"        // time_t it was last played
										 , NULL, NULL, &errmsg);
			
			if (result != SQLITE_OK)
				{
					sqlite3_free(errmsg);
					/* don't care about the error message.
					fprintf(stderr, "unable to create table! :%s\n", errmsg);
					*/
				}
			result = sqlite3_exec(db->db,
														"CREATE TABLE "
														"IF NOT EXISTS "
														" PlayHistory "
														"( "
														" path TEXT PRIMARY KEY "
														",playedDate INTEGER "
														" ) "
														, NULL, NULL, &errmsg);
			if (result != SQLITE_OK)
				{
					sqlite3_free(errmsg);
				}
		}
	return db;
}

/** free a database connected to with 'database_new'
 */
void database_free(Database* db)
{
	//printf("closing the db.\n");
	sqlite3_close(db->db);
	free(db);
}

static DBIterator* _database_iterator_new(NextItemFx next_item_fx, FreeFx free_fx, char** tbl_results,
																					const int rows, const int cols)
{
	DBIterator* it = malloc(sizeof(DBIterator));
	
	it->next_fx = next_item_fx;
	it->free_fx = free_fx;
	
	it->tbl_results = tbl_results;
	it->rows = rows;
	it->cols = cols;
	it->pos = 0; 
	/* point to one before the first item. 
	 * the result sets include the header rows, so we need to include this in our row count.
	 */
	
	return it;
}
/** delete an iterator.
 */
void database_iterator_free(DBIterator* it)
{
	if (it)
		{
			if (it->free_fx) { it->free_fx(it); }
			if (it->tbl_results) { sqlite3_free_table(it->tbl_results); }
			free(it);
			it = 0;
		}
}

/** move the iterator to the next item.
 *  so:
 *     /-before this function is done.
 *  ...[record][record]...
 *             \- here after this function executes.
 *
 *  @return the item which exists at the next position
 */
void* database_iterator_next(DBIterator* it)
{
	void* result = 0;
	
	if (database_iterator_move_next(it))
		{
			result = database_iterator_get(it);
		}
	
	return result;
}

void* database_iterator_get(DBIterator* it)
{
	if (it && it->next_fx) { return it->next_fx(it); }
	return 0;
}

int database_iterator_move_next(DBIterator* it)
{
	int result = 0;
	
	/* only push to the next item if:
	 *  there is an iterator,
	 *  there is a next function,
	 *  there is another record to point to.
	 */
	if (it && it->next_fx)
		{
			it->pos += it->cols;
			/* not zero rows and 
			 * the new position is less than or equal to the number of rows
			 * get the next result.
			 */
			if((it->rows != 0) && (it->pos <= (it->rows * it->cols)))
				{
					result = 1;
				}
		}
	return result;
}

/** retrieve all the files in the database.
 *  or filters the files with the given part of the query.
 *
 *  @param query_part2  the query after the 'from' clause.
 *  @return list or NULL if error (or no files)
 */
DBIterator* database_video_files_get(Database* db, const char* where_part, const char* orderby_part)
{
	char* error_msg;
	int result;
	int rows, cols;
	char** tbl_results=0;
	DBIterator* it = 0;
	char query[8192];
	
	const char* query_base = 
		"SELECT v.id, v.path, v.title, v.genre, v.f_type, "
		" COUNT(ph.playedDate) AS playCount, v.length, "
		" MAX(ph.playedDate) AS lastPlayed "
		"FROM video_files AS v "
		"LEFT JOIN PlayHistory AS ph ON ph.path = v.path "
		"WHERE v.f_type = 'video' ";
	
	if (! orderby_part)
		{
			orderby_part = "ORDER BY v.title, v.path ";
		}
	if (! where_part) { where_part = " "; }
	
	snprintf(query, sizeof(query), 
					 "%s %s GROUP BY v.path, v.title %s ",
					 query_base, where_part, orderby_part);
	
	result = sqlite3_get_table(db->db, query, &tbl_results, &rows, &cols, &error_msg);
	if (SQLITE_OK == result)
		{
			it =_database_iterator_new(_video_files_next, 0, /* no extra data to free. */
																 tbl_results, rows, cols);
		}
	else
		{
			printf("error: %s", error_msg);
			sqlite3_free(error_msg);
		}
	
	return it;
}

/** retrieve a file from the database given the item's id.
 *
 *  @param id   the id of the record to get.
 */
DBIterator* database_video_files_id_get(Database* db, const long long id)
{
	DBIterator* it;
	char buf[128];
	snprintf(buf, sizeof(buf), " AND v.ID = %lld ", id);
	it = database_video_files_get(db, buf, NULL);
	return it;
}

/** retrieve files from the database given part of a path.
 *  
 *  @param path  the front part of the path to search for.
 *  @return      an iterator pointing to before the first row, or null if no records.
 */
DBIterator* database_video_files_path_search(Database* db, const char* path)
{
	DBIterator* it;
	char* where_part;
	
	where_part = sqlite3_mprintf("AND v.path like '%q%s' ", path, "%") ;
	
	it = database_video_files_get(db, where_part, NULL);
	sqlite3_free(where_part);
	
	return it;
}

/** retrieves all files from the database given the genre.
 *  
 *  @param genre  genre to filter the file list for
 *  @return       an iterator or null if no files with the genre exist.
 */
DBIterator* database_video_files_genre_search(Database* db, const char* genre)
{
	DBIterator* it;
	char* query;
	
	query = sqlite3_mprintf("AND v.genre = '%q' ", genre);
	
	it = database_video_files_get(db, query, NULL);
	sqlite3_free(query);
	
	return it;
}

/** retrieves the first 50 files with a playcount greater than 0.
 *  orders the list by playcount, title then path.
 *
 *  @return an iterator or null if no files have been played.
 */
DBIterator* database_video_favorites_get(Database* db)
{
 	const char* where_clause = "AND playCount > 0 ";
	const char* orderby_part = 
		"ORDER BY playCount DESC, v.title, v.path "
		"LIMIT 50";
	
	return database_video_files_get(db, where_clause, orderby_part);
}

/** retrieve the first 50 files with a recent lastplayed date.
 *  orders by the last play date.
 *
 *  @return an iterator.
 */
DBIterator* database_video_recents_get(Database* db)
{
	const char* orderby_clause = 
		"ORDER BY lastPlayed DESC, v.title, v.path "
		" LIMIT 50";
	return database_video_files_get(db, NULL, orderby_clause);
}

DBIterator* database_video_news_get(Database* db)
{
	const char* where_clause =
		"ORDER BY v.createdDate DESC, v.title, v.path "
		" LIMIT 50 " ;
	return database_video_files_get(db, NULL, where_clause) ;
}

static void* _genre_next(DBIterator* it)
{
	Genre* genre = malloc(sizeof(Genre));
			
	genre->label = eina_stringshare_add(it->tbl_results[it->pos + 0]);
	genre->count = atoi(it->tbl_results[it->pos +1]);
	
	return genre;
}

/** get a list of the genres in the database.
 *  this excludes movies, and anime.
 */
DBIterator* database_video_genres_get(Database* db, const char* genre)
{
	DBIterator* it = 0;
	char** tbl_results = 0;
	int rows, cols;
	int result;
	char* error_msg;
	char* query = NULL;
	
	if (genre)
		{
			if (genre[0] != 0)
				{
					/* not an empty string. */
					if (!strcmp("anime", genre))
						{
							query = 
								"SELECT genre, count(path) "
								"FROM video_files "
								"WHERE genre like 'anime%' "
								"GROUP BY genre "
								"ORDER BY genre";
						}
					else if (!strcmp("movies", genre))
						{
							query = 
								"SELECT genre, count(path) "
								"FROM video_files "
								" WHERE genre like 'movies%' "
								"GROUP BY genre "
								"ORDER BY genre";
						}
				}
		}
	
	if (! query)
		{
			query = 
				"SELECT genre, count(path) "
				"FROM video_files "
				"WHERE "
				"   genre <> 'anime' "
				"    AND genre not like 'anime/%' "
				"    AND genre <> 'movies' "
				"    AND genre not like 'movies/%' "
				"GROUP BY genre "
				"ORDER BY genre";
		}

	result = sqlite3_get_table(db->db, query, &tbl_results, &rows, &cols, &error_msg);
	if (SQLITE_OK == result)
		{
			if (rows > 0)
				{
					//int max_item = rows * cols;
					
					it = _database_iterator_new(_genre_next, 0, /* nothing to free */
																			tbl_results, rows, cols);
				}
		}
	
	return it;
}

/** delete a file from the database.
 */
void database_video_file_del(Database* db, const char* path)
{
	int result;
	char* error_msg;
	char* query = sqlite3_mprintf("DELETE FROM video_files WHERE path = %Q",
																path);
	
	//printf("%s\n", query);
	result = sqlite3_exec(db->db, query, NULL, NULL, &error_msg);
	if (result != SQLITE_OK)
		{
			fprintf(stderr, "db: delete error: %s; %s\n", query, error_msg);
			sqlite3_free(error_msg);
		}
	
	sqlite3_free(query);
}

/** add a new file to the database
 */
void database_video_file_add(Database* db, const Volume_Item* item)
{
	int result;
	char* error_msg =0;
	time_t lp = time(0);
	char queryBuf[8192];
	
	char* query = sqlite3_mprintf(
		 "INSERT INTO video_files (path, title, genre, f_type, length, createddate) "
		 "VALUES(%Q, %Q, %Q, %Q, %d",
		 item->path, item->name, item->genre, item->type,
		 item->length);
	
	snprintf(queryBuf, sizeof(queryBuf), "%s, %ld)", query, lp);
	
	result = sqlite3_exec(db->db, queryBuf, NULL, NULL, &error_msg);
	if (result != SQLITE_OK)
		{
			fprintf(stderr, "db: insert error: \"%s\"; %s\n", query, error_msg);
			sqlite3_free(error_msg);
		}
	
	sqlite3_free(query);
}

/* played the file.
 */
void database_video_file_update(Database* db, const Volume_Item* item)
{
	int result;
	char* error_msg;
	char* query;
	time_t lp = time(0);
	char buf[8192];

	/* update the values so they can be written to the database. */	
	query = sqlite3_mprintf(
													"INSERT INTO PlayHistory "
													" ( path, playedDate ) "
													" VALUES ( '%q', ",
													item->path);
	snprintf(buf, sizeof(buf), "%s %ld)", query, lp);
	printf ("%s;%s;%s\n", query, item->path, item->name);
	
	result = sqlite3_exec(db->db, buf, NULL, NULL, &error_msg);
	if (SQLITE_OK != result)
		{
			fprintf(stderr, "db: update error:\"%s\"; %s\n", query, error_msg);
			sqlite3_free(error_msg);
		}
	sqlite3_free(query);
}

/* you HAVE TO SELECT AT LEAST (IN THIS ORDER)
 * path, title, genre, f_type, playcount, length, lastplayed
 */
static void* _video_files_next(DBIterator* it)
{
#define COL_ID         0
#define COL_PATH       1
#define COL_TITLE      2
#define COL_GENRE      3
#define COL_FTYPE      4
#define COL_PLAYCOUNT  5
#define COL_LENGTH     6
#define COL_LASTPLAYED 7

	Volume_Item* item;
	long long id ;
	/*
	printf("%dx%d\n", it->rows, it->cols);
	printf ("%s;%s;%s\n", it->tbl_results[it->pos + COL_PATH], it->tbl_results[it->pos + COL_TITLE],
					it->tbl_results[it->pos + COL_GENRE]);
	*/
	
	if (it->tbl_results[it->pos + COL_ID]) { id = atoi(it->tbl_results[it->pos + COL_ID]); }

	item = volume_item_new(id,
												 it->tbl_results[it->pos + COL_PATH], 
												 it->tbl_results[it->pos + COL_TITLE],
												 it->tbl_results[it->pos +COL_GENRE], 
												 it->tbl_results[it->pos + COL_FTYPE]);
	if (it->tbl_results[it->pos + COL_PLAYCOUNT])
		{ item->play_count = atoi(it->tbl_results[it->pos+COL_PLAYCOUNT]); }
	else { item->play_count = 0; }
	
	item->length = atoi(it->tbl_results[it->pos+COL_LENGTH]);
	
	if (it->tbl_results[it->pos + COL_LASTPLAYED])
		{ item->last_played = atoi(it->tbl_results[it->pos+COL_LASTPLAYED]); }
	else { item->play_count = 0; }
	
	return item;
}
