#ifndef NETCODE_LIBRARY_EXTENSIONS_INCLUDE_GUARD
#define NETCODE_LIBRARY_EXTENSIONS_INCLUDE_GUARD

#ifdef __cplusplus
extern "C" {
#endif

int               ncDebugServerMemoryUsage (struct NCserver * server);
int               ncDebugClientMemoryUsage (struct NCclient * client);

#ifdef __cplusplus
}
#endif

#endif