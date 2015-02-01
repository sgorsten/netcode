#ifndef VIEW_H
#define VIEW_H

typedef struct VClass_ * VClass;

typedef struct VServer_ * VServer;
typedef struct VObject_ * VObject;

typedef struct VClient_ * VClient;
typedef struct VView_ * VView;

VClass vCreateClass(int numIntFields);

VServer vCreateServer(const VClass * classes, int numClasses);
VObject vCreateObject(VServer server, VClass objectClass);
void vSetObjectInt(VObject object, int index, int value);
int vPublishUpdate(VServer server, void * buffer, int bufferSize);

VClient vCreateClient(const VClass * classes, int numClasses);
VView vCreateView(VClient client, VClass viewClass);
void vConsumeUpdate(VClient client, const void * buffer, int bufferSize);

int vGetViewInt(VView object, int index);

#endif