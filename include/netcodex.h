#ifndef NETCODE_LIBRARY_EXTENSIONS_INCLUDE_GUARD
#define NETCODE_LIBRARY_EXTENSIONS_INCLUDE_GUARD

#ifdef __cplusplus
extern "C" {
#endif

#include <netcode.h>

void ncxPrintClientCodeEfficiency (NCclient * client);

int ncxServerMemoryUsage (NCserver * server);
int ncxClientMemoryUsage (NCclient * client);

#ifdef __cplusplus
}
#endif

#endif