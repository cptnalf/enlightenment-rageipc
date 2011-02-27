/*
#include "e.h"
#include "sha1.h"
*/

#include <Ecore.h>
#include <Ecore_Ipc.h>
#include <Ecore_File.h>
#include <Ecore_Con.h>
#include <Eet.h>

#include "media_storage.h"

extern Media_Storage DB_Store;
int debug = 0;

#define __UNUSED__

/* FIXME: this protocol is NOT secure. it has no ecryption and no
 * public/private key checks to make sure who you connect to is really a
 * network member and not a honeypot key harvester. the fact that this is
 * an unknown service without any visibility as of now means you are pretty
 * safe - at worst someone will be able to read your videos as your private
 * key should never leave your home
 */

typedef struct _Client Client;
typedef struct _Node Node;

struct _Client
{
	Ecore_Ipc_Client *client;
	char *ident;
	unsigned char local : 1;
	unsigned char version_ok : 1;
	unsigned char auth_ok : 1;
	unsigned char who_ok : 1;
	unsigned char private_ok : 1;
};

struct _Node
{
	Ecore_Ipc_Server *server;
	char *name;
	char *key;
	char *ident;
	unsigned char connected : 1;
	unsigned char version_ok : 1;
	unsigned char who_ok : 1;
};

static Media_Storage* _store;

static int _version = 10; /* current proto version */
static int _version_magic1 = 0x14f8ec67; /* magic number for version check */
static int _version_magic2 = 0x3b45ef56; /* magic number for version check */
static int _version_magic3 = 0x8ea9fca0; /* magic number for version check */
static char *_key_private = "private"; /* auth key for anyone with read and write access - i.e. your own home and devices only - hardcoded for now */
static char *_key_public = "public"; /* public members of the network - read and borrow rights only - hardcoded for now */
static char *_ident_info = "user=x;location=y;"; /* send as ident in response to a who - for now hardcoded */
static Ecore_Ipc_Server *_server_local = NULL;
static Ecore_Ipc_Server *_server_remote = NULL;
static Eina_List *_clients = NULL;
static Eina_List *_nodes = NULL;

static void
_client_del(Client *cl)
{
   ecore_ipc_client_data_set(cl->client, NULL);
   _clients = eina_list_remove(_clients, cl->client);
   ecore_ipc_client_del(cl->client);
   if (cl->ident) free(cl->ident);
   free(cl);
}

static void
_node_del(Node *nd)
{
   _nodes = eina_list_remove(_nodes, nd);
   if (nd->server) ecore_ipc_server_del(nd->server);
   if (nd->name) free(nd->name);
   if (nd->key) free(nd->key);
   free(nd);
}

static Eina_Bool
_client_cb_add(void *data __UNUSED__, int type __UNUSED__, void *event)
{
	Ecore_Ipc_Event_Server_Add *e;
	Node *nd;
	
	printf("server added!\n") ;
	
	e = event;
	nd = ecore_ipc_server_data_get(e->server);
	if (!nd) return EINA_TRUE;
	nd->connected = 1;
	return EINA_TRUE;
}

static Eina_Bool
_client_cb_del(void *data __UNUSED__, int type __UNUSED__, void *event)
{
	Ecore_Ipc_Event_Server_Del *e;
	Node *nd;
   
	e = event;
	nd = ecore_ipc_server_data_get(e->server);
	if (!nd) return EINA_TRUE;
	_node_del(nd);
	return EINA_TRUE;
}

static Eina_Bool
_client_cb_data(void *data __UNUSED__, int type __UNUSED__, void *event)
{
	Ecore_Ipc_Event_Server_Data *e;
	Node *nd;
  
	printf("got data from teh server!\n");
	
	e = event;
	nd = ecore_ipc_server_data_get(e->server);
	
	printf("server data: 0x%p\n", nd);
	if (!nd) return EINA_TRUE;
	
	printf("0x%x\n", e->major);
	
	switch (e->major)
		{
		case OP_VERSION: /* version info from client */
			printf ("version?\n");
			if ((e->minor != _version) ||
					(e->ref != _version_magic1) ||
					(e->ref_to != _version_magic2) ||
					(e->response != _version_magic3))
				/* client version not matching ours or magic wrong */
				{
					ecore_ipc_server_send(nd->server, OP_VERSION_ERROR, _version,
																0, 0, 0, NULL, 0);
					ecore_ipc_server_flush(nd->server);
					_node_del(nd);
					printf("bad version.\n");
				}
			else
				{
					nd->version_ok = 1;
					ecore_ipc_server_send(nd->server, OP_USER_AUTH, 0,
																0, 0, 0, nd->key, strlen(nd->key) + 1);
					ecore_ipc_server_send(nd->server, OP_USER_WHO, 0,
																0, 0, 0, NULL, 0);
					printf("good version!\n");
				}
			break;
		case OP_VERSION_ERROR: /* client does not like our version */
			printf("version error!\n");
			ecore_ipc_server_flush(nd->server);
			_node_del(nd);
			break;
		case OP_SYNC: /* client requested a sync - reply with sync in e->minor */
			printf("sync!\n");
			ecore_ipc_server_send(nd->server, OP_SYNC, e->minor,
														0, 0, 0, NULL, 0);
			break;
		case OP_NODE_ADD: /* client lists all network nodes it knows of */
			printf("node add\n");
			if ((e->data) && (e->size > 1) && (((char *)e->data)[e->size - 1] == 0))
				{
					Node *nd2;
	     
					nd2 = calloc(1, sizeof(Node));
					if (nd2)
						{
							/* FIXME: need to avoid self-connect */
							nd2->server = ecore_ipc_server_connect(ECORE_IPC_REMOTE_SYSTEM,
																										 e->data, 9889, nd2);
							if (!nd2->server)
								{
									free(nd2);
								}
							else
								{
									nd2->name = strdup(e->data);
									nd2->key = strdup(nd->key);
									_nodes = eina_list_append(_nodes, nd2);
								}
						}
				}
			break;
		case OP_USER_AUTH: /* client should not get this */
			break;
		case OP_USER_AUTH_ERROR:
			printf("user auth error\n");
			_node_del(nd);
			break;
		case OP_USER_WHO:
			printf("who?\n");
			ecore_ipc_server_send(nd->server, OP_USER_IDENT, 0,
														0, 0, 0, _ident_info, strlen(_ident_info) + 1);
			break;
		case OP_USER_IDENT:
			printf("ident?\n");
			if ((e->data) && (e->size > 1) && (((char *)e->data)[e->size - 1] == 0))
				{
					if (nd->ident) free(nd->ident);
					nd->ident = strdup(e->data);
				}
			break; 
		case OP_MEDIA_ADD:
			break;
		case OP_MEDIA_DEL:
			break;
		case OP_MEDIA_LOCK_NOTIFY:
			break;
		case OP_MEDIA_UNLOCK_NOTIFY:
			break;
		case OP_MEDIA_LOCK: /* client should not get this */
			break;
		case OP_MEDIA_UNLOCK:
			break;
		case OP_MEDIA_GET: /* client should not get this */
			break;
		case OP_MEDIA_GET_DATA:
			break;
		case OP_MEDIA_PUT: /* client should not get this */
			break;
		case OP_MEDIA_PUT_DATA: /* client should not get this */
			break;
		case OP_MEDIA_DELETE: /* client should not get this */
			break;
		case OP_THUMB_GET: /* client should not get this */
			break;
		case OP_THUMB_GET_DATA:
			break;
		default:
			break;
		}
	return EINA_TRUE;
}

static Eina_Bool
_server_cb_add(void *data __UNUSED__, int type __UNUSED__, void *event)
{
	Ecore_Ipc_Event_Client_Add *e;
	Client *cl;
	
	printf("got a client!\n");

	e = event;
	printf("ec: 0x%p\n", ecore_ipc_client_server_get(e->client));
	printf("sl: 0x%p\n", _server_remote);
	
	if (!((ecore_ipc_client_server_get(e->client) == _server_local) ||
				(ecore_ipc_client_server_get(e->client) == _server_remote))) 
		return EINA_TRUE;
	cl = calloc(1, sizeof(Client));
	if (!cl) return EINA_TRUE;
   
   cl->client = e->client;
   if (ecore_ipc_client_server_get(e->client) == _server_local)
     cl->local = 1;
   ecore_ipc_client_data_set(e->client, cl);
   _clients = eina_list_append(_clients, cl);
	 
	 printf("added a client... sending version info...\n");
	ecore_ipc_client_send(cl->client, OP_VERSION, _version,
												_version_magic1, _version_magic2, _version_magic3,
												NULL, 0);
	return EINA_TRUE;
}

static Eina_Bool
_server_cb_del(void *data __UNUSED__, int type __UNUSED__, void *event)
{
	Ecore_Ipc_Event_Client_Del *e;
	Client *cl;
	
	printf("deleting client!\n");
	e = event;
	if (!((ecore_ipc_client_server_get(e->client) == _server_local) ||
				(ecore_ipc_client_server_get(e->client) == _server_remote))) 
		return EINA_TRUE;
	/* delete client sruct */
	cl = ecore_ipc_client_data_get(e->client);
	if (!cl) return EINA_TRUE;
	_client_del(cl);
	return EINA_TRUE;
}              

static Eina_Bool
_server_cb_data(void *data __UNUSED__, int type __UNUSED__, void *event)
{
	Ecore_Ipc_Event_Client_Data *e;
	Client *cl;
	
	e = event;
	if (!((ecore_ipc_client_server_get(e->client) == _server_local) ||
				(ecore_ipc_client_server_get(e->client) == _server_remote))) 
		return EINA_TRUE;
	cl = ecore_ipc_client_data_get(e->client);
	if (!cl) return EINA_TRUE;
	switch (e->major)
		{
		case OP_VERSION: /* version info from client */
			printf("version?.\n");
			if ((e->minor != _version) ||
					(e->ref != _version_magic1) ||
					(e->ref_to != _version_magic2) ||
					(e->response != _version_magic3))
				/* client version not matching ours or magic wrong */
				{
					ecore_ipc_client_send(cl->client, OP_VERSION_ERROR, _version,
																0, 0, 0, NULL, 0);
					ecore_ipc_client_flush(cl->client);
					_client_del(cl);
				}
			else
				{
					cl->version_ok = 1;
				}
			break;
		case OP_VERSION_ERROR: /* client does not like our version */
			printf("version error\n");
			ecore_ipc_client_flush(cl->client);
			_client_del(cl);
			break;
		case OP_SYNC: /* client requested a sync - reply with sync in e->minor */
			printf("sync.\n");
			ecore_ipc_client_send(cl->client, OP_SYNC, e->minor,
														0, 0, 0, NULL, 0);
			break;
		case OP_NODE_ADD: /* client lists all network nodes it knows of */
			printf("node add.\n");
			if ((e->data) && (e->size > 1) && (((char *)e->data)[e->size - 1] == 0))
				{
					Node *nd;
					
					nd = calloc(1, sizeof(Node));
					if (nd)
						{
		  /* FIXME: need to avoid self-connect */
/* no need to connect - just list known nodes - we are a server
 * clients would do this though - above client code is a snippet
 *
		  nd->server = ecore_ipc_server_connect(ECORE_IPC_REMOTE_SYSTEM,
							e->data, 9889, nd);
		  if (!nd->server)
		    {
		       free(nd);
		    }
		  else
*/ 		    
							{
								nd->name = strdup(e->data);
								if (cl->private_ok) nd->key = strdup(_key_private);
								else nd->key = strdup(_key_public);
								_nodes = eina_list_append(_nodes, nd);
							}
						}
				}
			break;
		case OP_USER_AUTH:
			printf("user auth.\n");
			if (!cl->version_ok)
				{
					printf("deleted!\n");
					_client_del(cl);
				}
			else
				{
					if ((e->data) && (e->size > 1) && (((char *)e->data)[e->size - 1] == 0))
						{
							if (!strcmp(e->data, _key_private))
								{
									cl->auth_ok = 1;
									cl->private_ok = 1;
									ecore_ipc_client_send(cl->client, OP_USER_WHO, 0,
																				0, 0, 0, NULL, 0);
								}
							else if (!strcmp(e->data, _key_public))
								{
									cl->auth_ok = 1;
									ecore_ipc_client_send(cl->client, OP_USER_WHO, 0,
																				0, 0, 0, NULL, 0);
								}
							else
								{
									ecore_ipc_client_send(cl->client, OP_USER_AUTH_ERROR, 0,
																				0, 0, 0, NULL, 0);
									ecore_ipc_client_flush(cl->client);
									_client_del(cl);
								}
						}
					else
						{
							ecore_ipc_client_send(cl->client, OP_USER_AUTH_ERROR, 0,
																		0, 0, 0, NULL, 0);
							ecore_ipc_client_flush(cl->client);
							_client_del(cl);
						}
				}
			break;
		case OP_USER_AUTH_ERROR: /* server should never get this */
			break;
		case OP_USER_WHO:
			printf("user who\n");
			ecore_ipc_client_send(cl->client, OP_USER_IDENT, 0,
														0, 0, 0, _ident_info, strlen(_ident_info) + 1);
			break;
		case OP_USER_IDENT:
			printf("user ident.\n");
			if ((e->data) && (e->size > 1) && (((char *)e->data)[e->size - 1] == 0))
				{
					if (cl->ident) free(cl->ident);
					cl->ident = strdup(e->data);
				}
			break;
		case OP_MEDIA_ADD: /* server should never get this */
			{
				Rage_Ipc_VolItem* item = e->data;
				
				if (e->size != sizeof(struct _Rage_Ipc_VolItem)) { printf("wrongsize!\n"); }
				else
					{
						_store->item_add(item);
					}
				break;
			}
		case OP_MEDIA_DEL: /* server should never get this */
			{
				ID_Item* item = e->data;
				_store->item_del(item);
				break;
			}
			
		case (OP_MEDIA_DETAILS_GET):
			{
				ID_Item* item = e->data;
				Rage_Ipc_VolItem* volitem = _store->item_details_get(item);
				
				ecore_ipc_client_send(cl->client, OP_MEDIA_DETAILS, 0, 0, 0, 0,
															volitem, sizeof(Rage_Ipc_VolItem));
				free(volitem);
				break;
			}
			
		case (OP_GENRES_GET):
			{
				Genre_Result* items = _store->genre_list();
				ecore_ipc_client_send(cl->client, OP_GENRE_LIST, 0, 0, 0, 0, 
															items, items->size);
				free(items);
				break; 
			}
		case (OP_MEDIA_QUERY): 
			{
				Query_Result* items = _store->media_query((MQ_Type)e->minor, e->data, e->size);
				
				ecore_ipc_client_send(cl->client, OP_MEDIA_LIST, 0,
																		0, 0, 0, items, items->size);
				free(items);
				break;
			}
			
		case OP_MEDIA_LOCK_NOTIFY: /* server should never get this */
			break;
		case OP_MEDIA_UNLOCK_NOTIFY: /* server should never get this */
			break;
		case OP_MEDIA_LOCK:
			break;
		case OP_MEDIA_UNLOCK:
			break;
		case OP_MEDIA_GET:
			break;
		case OP_MEDIA_GET_DATA: /* server should never get this */
			break;
		case OP_MEDIA_PUT:
			break;
		case OP_MEDIA_PUT_DATA:
			break;
		case OP_MEDIA_DELETE:
			break;
		case OP_THUMB_GET:
			break;
		case OP_THUMB_GET_DATA: /* server should never get this */
			break;
		default:
			break;
		}
	return EINA_TRUE;
}


static int
_server_init(void)
{
	_server_local = ecore_ipc_server_add(ECORE_IPC_LOCAL_USER, "rage", 0, NULL);
	if (!_server_local) return 0;
	_server_remote = ecore_ipc_server_add(ECORE_IPC_REMOTE_SYSTEM, "localhost", 9889, NULL);
	if (!_server_remote)
		{
			ecore_ipc_server_del(_server_local);
			_server_local = NULL;
			return 0;
		}
	ecore_event_handler_add(ECORE_IPC_EVENT_CLIENT_ADD, _server_cb_add, NULL);
	ecore_event_handler_add(ECORE_IPC_EVENT_CLIENT_DEL, _server_cb_del, NULL);
	ecore_event_handler_add(ECORE_IPC_EVENT_CLIENT_DATA, _server_cb_data, NULL);
	ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_ADD, _client_cb_add, NULL);
	ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_DEL, _client_cb_del, NULL);
	ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_DATA, _client_cb_data, NULL);
	return 1;
}

static void
_server_shutdown(void)
{
	if (_server_local)
		{
			ecore_ipc_server_del(_server_local);
			_server_local = NULL;
		}
	if (_server_remote)
		{
			ecore_ipc_server_del(_server_remote);
			_server_remote = NULL;
		}
}

static int _client_init()
{
	Node *nd2;
	nd2 = calloc(1, sizeof(Node));
	
	ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_ADD, _client_cb_add, NULL);
	ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_DEL,	_client_cb_del, NULL);
	ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_DATA, _client_cb_data, NULL) ;
	
	if (nd2)
		{
			_server_local = ecore_ipc_server_connect(ECORE_IPC_REMOTE_SYSTEM,
																							 "localhost", 
																							 9889,
																							 nd2);
			
			if (!_server_local) 
				{
					free(nd2);
					return 0;
				}
			else
				{
					nd2->server = _server_local ;
					nd2->name = strdup("foo");
					nd2->key = strdup(_key_private);
					_nodes = eina_list_append(_nodes, nd2);
				}
		}

	return 1;
}

static void _client_shutdown()
{
	printf("bye bye!\n");
	_node_del(_nodes->data);
	_server_local = NULL;
}

static Eina_Bool _files_query(void* data)
{
	const char* genre = "anime";
	
	ecore_ipc_server_send(_server_local, OP_MEDIA_QUERY, MQ_TYPE_GENRE,
												0, 0, 0,
												genre, strlen(genre));
	return EINA_FALSE;
}

static Eina_Bool _genre_query(void* data)
{
	ecore_ipc_server_send(_server_local, OP_GENRES_GET, 0,
												0, 0, 0,
												NULL, 0);
	
	{
		Ecore_Timer* t = ecore_timer_add(2.0, _files_query, NULL);
		t = NULL;
	}
	
	return EINA_FALSE;
}

int
main(int argc, char **argv)
{
	char * dbPath;

	if (argc < 2) { printf("need arg: raged <path to database>\n"); exit(1); return 0; }
	
	dbPath = argv[1];

	eet_init();
	ecore_init();
	ecore_file_init();
	ecore_con_init();
	ecore_ipc_init();
   
	ecore_app_args_set(argc, (const char **)argv);
	
#ifdef _SERVER_
	if (!_server_init())
		{
			printf("Raged: ERROR - cannot listen on sockets\n");
			exit(-1);
		}
	printf("server running!\n");
	_store = &DB_Store;
	_store->init(dbPath);
	
#else
	if (!_client_init())
		{
			printf("ragec: error cannot connect to server!\n");
			exit(-1);
		}
	
	ecore_ipc_server_send(_server_local, OP_VERSION, _version,
												_version_magic1, _version_magic2, _version_magic3,
												NULL, 0);
	
	{
		Ecore_Timer* t = ecore_timer_add(2.0, _genre_query, NULL);
	}
#endif
	
	ecore_main_loop_begin();

#if _SERVER_
	_store->shutdown();
	_server_shutdown();
#else
	_client_shutdown();
#endif
   
	ecore_ipc_shutdown();
	ecore_con_shutdown();
	ecore_file_shutdown();
	ecore_shutdown();
	eet_shutdown();
	return 0;
}
