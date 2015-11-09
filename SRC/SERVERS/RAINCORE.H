// rain core, midas and amp functions are called here

#ifndef __RAINCORE_H
#define __RAINCORE_H

void errAdd(char *msg);
char *errGet();

int RainCommand(int command,int param1,int param2,int param3,
                char *filename,int fileofs,int filesize);
void RainReset();

#endif
