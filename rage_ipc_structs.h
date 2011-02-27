/* filename: rage_ipc_structs.h
 *  chiefengineer
 *  Sat Feb 26 17:28:52 PST 2011
 */

#include <time.h>

typedef struct _Rage_Ipc_VolItem Rage_Ipc_VolItem;
struct _Rage_Ipc_VolItem
{
	long long id;
	char path[256];
	char name[128];
	char genre[128];
	char type[8];
	time_t created_date;
};

typedef enum _MQ_Type
	{
		MQ_TYPE_NONE,
		MQ_TYPE_GENRE,
		MQ_TYPE_PATH,
		
	} MQ_Type;

typedef struct _IDItem ID_Item;
struct _IDItem
{
	int size;
	unsigned int count;
	long long ids[0];
};

typedef struct _Genre_Result_Item Genre_Result_Item;
struct _Genre_Result_Item
{
	char label[128];
	int count;
};

typedef struct _Genre_Result Genre_Result;
struct _Genre_Result
{
	int size;
	unsigned int count;
	Genre_Result_Item recs[0];
};

typedef struct _Query_Result Query_Result;
struct _Query_Result
{
	unsigned int size;
	unsigned int count;
	Rage_Ipc_VolItem recs[0];
};

typedef enum _Op
	{
		OP_VERSION,
		OP_VERSION_ERROR,
		OP_SYNC,
		OP_NODE_ADD,
		OP_USER_AUTH,
		OP_USER_AUTH_ERROR,
		OP_USER_WHO,
		OP_USER_IDENT,

		OP_MEDIA_ADD, /* media added notification */
		OP_MEDIA_DEL, /* media deleted notification */
		
		OP_MEDIA_DETAILS_GET,
		OP_MEDIA_DETAILS,
		
		OP_MEDIA_QUERY,
		OP_MEDIA_LIST, /* data return from the query. */
		OP_GENRES_GET,
		OP_GENRE_LIST, /* dadta return from the query. */
		
		OP_MEDIA_LOCK_NOTIFY, 
		OP_MEDIA_UNLOCK_NOTIFY,
		OP_MEDIA_LOCK,
		OP_MEDIA_UNLOCK,
		OP_MEDIA_GET,
		OP_MEDIA_GET_DATA, /* media data, response to above message */
		OP_MEDIA_PUT,
		OP_MEDIA_PUT_DATA,
		OP_MEDIA_DELETE,
		OP_THUMB_GET,
		OP_THUMB_GET_DATA /* thumb data, response to above message. */
	} Op;

