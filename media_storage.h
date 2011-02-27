/* filename: media_storage.h
 *  chiefengineer
 *  Sat Feb 26 12:53:16 PST 2011
 */

#include <time.h>
#include <Eina.h>

#include "rage_ipc_structs.h"

typedef struct _Media_Storage Media_Storage;
struct _Media_Storage
{
	const char* name;
	
	Eina_Bool (*init)(void* data);
	void (*shutdown)();
	
	Eina_Bool (*item_add)(Rage_Ipc_VolItem* item);
	Eina_Bool (*item_del)(const char* item);
	Rage_Ipc_VolItem* (*item_details_get)(ID_Item* item);
	
	Genre_Result* (*genre_list)(const char* genre);
	Query_Result* (*media_query)(MQ_Type query_type, void* data, int size);
};

Rage_Ipc_VolItem* rage_ipc_volitem_new(const long long id,
																			 const char* path,
																			 const char* name,
																			 const char* genre,
																			 const char* type,
																			 time_t created);
