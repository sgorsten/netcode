#ifndef VIEW_H
#define VIEW_H

typedef struct VClass_  * VClass;
typedef struct VServer_ * VServer;
typedef struct VPeer_   * VPeer;
typedef struct VObject_ * VObject;
typedef struct VClient_ * VClient;
typedef struct VView_   * VView;

VClass  vCreateClass   (int numIntFields);

VServer vCreateServer  (const VClass * classes, int numClasses);
VClient vCreateClient  (const VClass * classes, int numClasses);

VPeer   vCreatePeer    (VServer server);
VObject vCreateObject  (VServer server, VClass objectClass);

void    vSetVisibility (VPeer peer, VObject object, int isVisible);
int     vPublishUpdate (VPeer peer, void * buffer, int bufferSize);

void    vSetObjectInt  (VObject object, int index, int value);
void    vDestroyObject (VObject object);

void    vConsumeUpdate (VClient client, const void * buffer, int bufferSize);
int     vGetViewCount  (VClient client);
VView   vGetView       (VClient client, int index);

VClass  vGetViewClass  (VView view);
int     vGetViewInt    (VView view, int index);

#endif