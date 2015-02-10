/* Copyright (c) 2015 Sterling Orsten
     This software is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software. You are granted a perpetual, 
   irrevocable, world-wide license to copy, modify, and redistribute
   this software for any purpose, including commercial applications. */

#ifndef NETCODE_LIBRARY_INTERFACE_INCLUDE_GUARD
#define NETCODE_LIBRARY_INTERFACE_INCLUDE_GUARD

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NCprotocol NCprotocol;
typedef struct NCclass NCclass;
typedef struct NCint NCint;
typedef struct NCserver NCserver;
typedef struct NCpeer NCpeer;
typedef struct NCobject NCobject;
typedef struct NCclient NCclient;
typedef struct NCview NCview;
typedef struct NCblob NCblob;
     
NCprotocol *    ncCreateProtocol  (int maxFrameDelta);
NCclass *       ncCreateClass     (NCprotocol * protocol);
NCint *         ncCreateInt       (NCclass * cl);

NCserver *      ncCreateServer    (const NCprotocol * protocol);
NCclient *      ncCreateClient    (const NCprotocol * protocol);

NCpeer *        ncCreatePeer      (NCserver * server);
NCobject *      ncCreateObject    (NCserver * server, const NCclass * cl);
void            ncPublishFrame    (NCserver * server);
void            ncDestroyServer   (NCserver * server);

void            ncSetVisibility   (NCpeer * peer, const NCobject * object, int isVisible);
NCblob *        ncProduceUpdate   (NCpeer * peer);
void            ncConsumeResponse (NCpeer * peer, const void * data, int size);
void            ncDestroyPeer     (NCpeer * peer);

void            ncSetObjectInt    (NCobject * object, const NCint * field, int value);
void            ncDestroyObject   (NCobject * object);

int             ncGetViewCount    (const NCclient * client);
const NCview *  ncGetView         (const NCclient * client, int index);
void            ncConsumeUpdate   (NCclient * client, const void * data, int size);
NCblob *        ncProduceResponse (NCclient * client);
void            ncDestroyClient   (NCclient * client);

const NCclass * ncGetViewClass    (const NCview * view);
int             ncGetViewInt      (const NCview * view, const NCint * field);

const void *    ncGetBlobData     (const NCblob * blob);
int             ncGetBlobSize     (const NCblob * blob);
void            ncFreeBlob        (NCblob * blob);

#ifdef __cplusplus
}
#endif

#endif