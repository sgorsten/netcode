#ifndef NETCODE_LIBRARY_API_INCLUDE_GUARD
#define NETCODE_LIBRARY_API_INCLUDE_GUARD

#ifdef __cplusplus
extern "C" {
#endif

void const *        ncGetBlobData     (struct NCblob * blob);
int                 ncGetBlobSize     (struct NCblob * blob);
void                ncFreeBlob        (struct NCblob * blob);
      
struct NCprotocol * ncCreateProtocol  (int maxFrameDelta);
struct NCclass *    ncCreateClass     (struct NCprotocol * protocol);
struct NCint *      ncCreateInt       (struct NCclass * cl);

struct NCserver *   ncCreateServer    (struct NCprotocol * protocol);
struct NCclient *   ncCreateClient    (struct NCprotocol * protocol);

struct NCpeer *     ncCreatePeer      (struct NCserver * server);
struct NCobject *   ncCreateObject    (struct NCserver * server, struct NCclass * cl);
void                ncPublishFrame    (struct NCserver * server);

void                ncSetVisibility   (struct NCpeer * peer, struct NCobject * object, int isVisible);
struct NCblob *     ncProduceUpdate   (struct NCpeer * peer);
void                ncConsumeResponse (struct NCpeer * peer, void const * data, int size);

void                ncSetObjectInt    (struct NCobject * object, struct NCint * field, int value);
void                ncDestroyObject   (struct NCobject * object);

void                ncConsumeUpdate   (struct NCclient * client, void const * data, int size);
struct NCblob *     ncProduceResponse (struct NCclient * client);
int                 ncGetViewCount    (struct NCclient * client);
struct NCview *     ncGetView         (struct NCclient * client, int index);

struct NCclass *    ncGetViewClass    (struct NCview * view);
int                 ncGetViewInt      (struct NCview * view, struct NCint * field);

#ifdef __cplusplus
}
#endif

#endif