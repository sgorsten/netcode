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

#define NC_EVENT_CLASS_FLAG 0x00000001
#define NC_CONST_FIELD_FLAG 0x00000001

typedef struct NCprotocol NCprotocol;
typedef struct NCclass NCclass;
typedef struct NCint NCint;
typedef struct NCauthority NCauthority;
typedef struct NCpeer NCpeer;
typedef struct NCobject NCobject;
typedef struct NCview NCview;
typedef struct NCblob NCblob;
     
NCprotocol *    ncCreateProtocol  (int maxFrameDelta);
NCclass *       ncCreateClass     (NCprotocol * protocol, int flags);
NCint *         ncCreateInt       (NCclass * cl, int flags);

NCauthority *   ncCreateAuthority (const NCprotocol * protocol);

NCpeer *        ncCreatePeer      (NCauthority * authority);
NCobject *      ncCreateObject    (NCauthority * authority, const NCclass * cl);
void            ncPublishFrame    (NCauthority * authority);
void            ncDestroyAuthority(NCauthority * authority);

int             ncGetViewCount    (const NCpeer * peer);
const NCview *  ncGetView         (const NCpeer * peer, int index);
void            ncSetVisibility   (NCpeer * peer, const NCobject * object, int isVisible);
NCblob *        ncProduceMessage  (NCpeer * peer);
void            ncConsumeMessage  (NCpeer * peer, const void * data, int size);
void            ncDestroyPeer     (NCpeer * peer);

void            ncSetObjectInt    (NCobject * object, const NCint * field, int value);
void            ncDestroyObject   (NCobject * object);

const NCclass * ncGetViewClass    (const NCview * view);
int             ncGetViewInt      (const NCview * view, const NCint * field);

const void *    ncGetBlobData     (const NCblob * blob);
int             ncGetBlobSize     (const NCblob * blob);
void            ncFreeBlob        (NCblob * blob);

#ifdef __cplusplus
}
#endif

#endif