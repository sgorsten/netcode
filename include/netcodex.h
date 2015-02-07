#ifndef NETCODE_LIBRARY_EXTENSIONS_INCLUDE_GUARD
#define NETCODE_LIBRARY_EXTENSIONS_INCLUDE_GUARD

#ifdef __cplusplus
extern "C" {
#endif

int               ncxServerMemoryUsage (struct NCserver * server);
int               ncxClientMemoryUsage (struct NCclient * client);

#ifdef __cplusplus
}
#endif

#endif