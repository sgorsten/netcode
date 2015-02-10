/* Copyright (c) 2015 Sterling Orsten
     This software is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software. You are granted a perpetual, 
   irrevocable, world-wide license to copy, modify, and redistribute
   this software for any purpose, including commercial applications. */

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