// RAIN high-level C/C++ API

#ifndef __RAIN_H
#define __RAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>
#if INT_MAX==2147483647
 #define I32 int
#else
#if LONG_MAX==2147483647
 #define I32 long
#else
 #error Compiler/platform not supported by RAIN.
#endif
#endif

char   *RainInit(); // returns static string, don't free
void	RainPoll();
I32	RainPlay(char *filename,I32 fileofs,I32 filesize,I32 loop,I32 volume,I32 panning);
void	RainSetVolume(I32 handle,I32 volume);
void	RainSetPanning(I32 handle,I32 volume);
void	RainStop(I32 handle);
void	RainAmplification(I32 amp);

extern void(*ReportError)(char *);
extern void(*ReportEnd)(I32);

#ifdef __cplusplus
};
#endif

#endif
