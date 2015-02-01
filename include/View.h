#ifndef VIEW_H
#define VIEW_H

typedef struct VClass_ * VClass;
typedef struct VServer_ * VServer;
typedef struct VClient_ * VClient;
typedef struct VObject_ * VObject;

VClass vCreateClass(int numIntFields);

VServer vCreateServer(const VClass * classes, int numClasses);
VObject vCreateServerObject(VServer server, VClass objectClass);
int vPublishUpdate(VServer server, void * buffer, int bufferSize);

VClient vCreateClient(const VClass * classes, int numClasses);
VObject vCreateClientObject(VClient client, VClass objectClass);
void vConsumeUpdate(VClient client, const void * buffer, int bufferSize);

void vSetObjectState(VObject object, const int * intFields);
void vGetObjectState(VObject object, int * intFields);

#endif