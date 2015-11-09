// amp interface common for all rain servers

#ifndef __AMP_H
#define __AMP_H

#ifdef __cplusplus
extern "C" {
#endif

void *MP3open(char *filename,int fileofs,int filesize,
              int *freq,int *stereo,void errAdd(char *));
int   MP3read(void *mp3,int loop,void *sampbuf, int len);
void  MP3close(void *mp3);

#ifdef __cplusplus
};
#endif

#endif