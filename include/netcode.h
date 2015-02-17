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
typedef struct NCref NCref;
typedef struct NCauthority NCauthority;
typedef struct NCpeer NCpeer;
typedef struct NCobject NCobject;
typedef struct NCblob NCblob;
     
NCprotocol *     ncCreateProtocol       (int maxFrameDelta);
NCclass *        ncCreateClass          (NCprotocol * protocol, int flags);
NCint *          ncCreateInt            (NCclass * cl, int flags);
NCref *          ncCreateRef            (NCclass * cl);
NCauthority *    ncCreateAuthority      (const NCprotocol * protocol);
                                        
NCpeer *         ncCreatePeer           (NCauthority * authority);
NCobject *       ncCreateLocalObject    (NCauthority * authority, const NCclass * cl);
void             ncPublishFrame         (NCauthority * authority);
void             ncDestroyAuthority     (NCauthority * authority);
                                        
const NCclass *  ncGetObjectClass       (const NCobject * object);
int              ncGetObjectInt         (const NCobject * object, const NCint * field);
const NCobject * ncGetObjectRef         (const NCobject * object, const NCref * field);
void             ncSetObjectInt         (NCobject * object, const NCint * field, int value);
void             ncSetObjectRef         (NCobject * object, const NCref * field, const NCobject * value);
void             ncDestroyObject        (NCobject * object);

int              ncGetRemoteObjectCount (const NCpeer * peer);
const NCobject * ncGetRemoteObject      (const NCpeer * peer, int index);
void             ncSetVisibility        (NCpeer * peer, const NCobject * object, int isVisible);
NCblob *         ncProduceMessage       (NCpeer * peer);
void             ncConsumeMessage       (NCpeer * peer, const void * data, int size);
void             ncDestroyPeer          (NCpeer * peer);

const void *     ncGetBlobData          (const NCblob * blob);
int              ncGetBlobSize          (const NCblob * blob);
void             ncFreeBlob             (NCblob * blob);

#ifdef __cplusplus
}
#endif

#endif