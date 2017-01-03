#ifndef OSUTIL_H
#define OSUTIL_H

#define OS LINUX

#if (OS == LINUX)

#elif (OS == RIOT)

#else
#error "OS must be #define'd to LINUX or RIOT"
#endif

#endif
