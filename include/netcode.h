#ifndef LIB_NETCODE_INCLUDE_GUARD
#define LIB_NETCODE_INCLUDE_GUARD

#ifdef __cplusplus
extern "C" {
#endif

void const *      ncGetBlobData     (struct NCblob * blob);
int               ncGetBlobSize     (struct NCblob * blob);
void              ncFreeBlob        (struct NCblob * blob);
      
struct NCclass *  ncCreateClass     (int numIntFields);
struct NCserver * ncCreateServer    (struct NCclass * const * classes, int numClasses, int maxFrameDelta);
struct NCclient * ncCreateClient    (struct NCclass * const * classes, int numClasses, int maxFrameDelta);

struct NCpeer *   ncCreatePeer      (struct NCserver * server);
struct NCobject * ncCreateObject    (struct NCserver * server, struct NCclass * objectClass);
void              ncPublishFrame    (struct NCserver * server);

void              ncSetVisibility   (struct NCpeer * peer, struct NCobject * object, int isVisible);
struct NCblob *   ncProduceUpdate   (struct NCpeer * peer);
void              ncConsumeResponse (struct NCpeer * peer, void const * data, int size);

void              ncSetObjectInt    (struct NCobject * object, int index, int value);
void              ncDestroyObject   (struct NCobject * object);

void              ncConsumeUpdate   (struct NCclient * client, void const * data, int size);
struct NCblob *   ncProduceResponse (struct NCclient * client);
int               ncGetViewCount    (struct NCclient * client);
struct NCview *   ncGetView         (struct NCclient * client, int index);

struct NCclass *  ncGetViewClass    (struct NCview * view);
int               ncGetViewInt      (struct NCview * view, int index);

int               ncDebugServerMemoryUsage (struct NCserver * server);
int               ncDebugClientMemoryUsage (struct NCclient * client);

#ifdef __cplusplus
}
#endif

#endif