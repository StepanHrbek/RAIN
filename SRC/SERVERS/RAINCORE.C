// rain core, midas and amp functions are called here

#define AMPLIFY_MP3 3 // play mp3 streams louder
#define AMPLIFY_ALL 3 // play all sounds louder
#define MP3PRECALCSEC 3 //how many seconds to precalculate?
#define EXTENDFILE // file.c from midas is extended and we can extend too

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <midasdll.h>

 // including file.h is dirty job
 #define ID_rf 1200
 #define ID_file 1300
 #define ulong unsigned long
 #define WORD unsigned short
 #define CALLING __cdecl
 #include "file.h"

#include "raincore.h"
#include "raindef.h"
#include "..\amp\rain-amp.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

////////////////////////////////////////////////////////////////////////

struct errOne
{
    struct errOne *next;
    char *text;
};

static struct errOne *errNew=NULL;
static struct errOne *errOld=NULL;

void errAdd(char *msg)   //stores copy of msg
{
    struct errOne *err=(struct errOne *)malloc(sizeof(struct errOne));
    if(err)
    {
        err->text=strdup(msg);
        err->next=NULL;
        if(errNew) errNew=errNew->next=err;
          else errOld=errNew=err;
    }
}

char *errGet()          //returns msg to be destroyed
{
    if(!errOld) return NULL;
    {
    char *result=errOld->text;
    struct errOne *tmp=errOld;
    errOld=errOld->next;
    if(!errOld) errNew=NULL;
    free(tmp);
    return result;
    }
}

////////////////////////////////////////////////////////////////////////

#define errBufLen TALK_STRSIZE
char errBuf[errBufLen];
#define hintlen TALK_STRSIZE
char hint[hintlen];

static void reportErr(int err)
{
    _bprintf(errBuf,errBufLen," %s: %s",hint,MIDASgetErrorMessage(err));
    errAdd(errBuf);
}

static void MIDASreportErr()
{
    if(MIDASgetLastError())
    {
        extern int mLastError;
        reportErr(MIDASgetLastError());
        mLastError=0;
    }
}

////////////////////////////////////////////////////////////////////////

#define SOUNDS 1000 // max number of simultaneously playing sounds
enum SoundType { None,Module,Sample,Stream };
struct Sound
{
    enum SoundType type;
    int rainhandle;   // 1..rainhandles
    int datahandle;   // midas
    int playhandle;   // midas
    int loop;         // used by stream feeder and stream/mod end detection
    int bufferbytes;  // used by stream feeder
};
static struct Sound     sound[SOUNDS];
int              sounds=0;
int              rainhandles=0;

////////////////////////////////////////////////////////////////////////

static void soundPlay(struct Sound *s,char *filename,int fileofs,int filesize,int loop)
{
    // read sound file header
    int err;
    fileHandle fh=0;
    long fsize;
    #define HEADSIZE 78
    unsigned char buf[HEADSIZE];
    memset(buf,0,HEADSIZE);
#ifdef EXTENDFILE
    rfSuperStart=fileofs;
    rfSuperSize=filesize;
#endif
    s->loop=loop;
    #define MCALL1(f) err=f;if(err) {reportErr(err); return;}
    #define MCALL2(f) err=f;if(err) {reportErr(err); err=fileClose(fh); if(err) reportErr(err); return;}
    MCALL1(fileOpen(filename,fileOpenRead,&fh));
    MCALL2(fileGetSize(fh,&fsize));
    MCALL2(fileRead(fh,buf,fsize<HEADSIZE?fsize:HEADSIZE));
    MCALL1(fileClose(fh));
    // analyze sound file header
    // waw sample? starts with "RIFF" with fmt!=55h
    if (buf[0]=='R' && buf[1]=='I' && buf[2]=='F' && buf[3]=='F' /*&& buf[8]=='W' && buf[9]=='A' && buf[10]=='V' && buf[11]=='E'*/  && buf[20]!=0x55)
    {
        extern int lastWAVfreq;
        s->type=Sample;
        s->datahandle=(int)MIDASloadWaveSample(filename,loop?MIDAS_LOOP_YES:MIDAS_LOOP_NO);
        if(s->datahandle) s->playhandle=(int)MIDASplaySample((MIDASsample)s->datahandle,MIDAS_CHANNEL_AUTO,0/*priority*/,lastWAVfreq,64,MIDAS_PAN_MIDDLE);
    }
#ifdef EXTENDFILE
    // iff sample with 0x16 characters long name? (FT2 makes them)
    // it isn't ment as IFF support, just quick 99% working hack
    else if (buf[0]=='F' && buf[1]=='O' && buf[2]=='R' && buf[3]=='M' && buf[8]=='8' && buf[9]=='S' && buf[10]=='V' && buf[11]=='X' && buf[12]=='N' && buf[13]=='A' && buf[14]=='M' && buf[15]=='E' && buf[16]==0 && buf[17]==0 && buf[18]==0 && buf[19]==0x16)
    {
        int rate=(buf[62]<<8)+buf[63];
        rfSuperStart+=78;
        rfSuperSize=MIN(fsize-78,(buf[74]<<24)+(buf[75]<<16)+(buf[76]<<8)+buf[77]);
        rfSuperXor=128;
        s->type=Sample;
        s->datahandle=(int)MIDASloadRawSample(filename,MIDAS_SAMPLE_8BIT_MONO,loop?MIDAS_LOOP_YES:MIDAS_LOOP_NO);
        if(s->datahandle) s->playhandle=(int)MIDASplaySample((MIDASsample)s->datahandle,MIDAS_CHANNEL_AUTO,0/*priority*/,rate,64,MIDAS_PAN_MIDDLE);
    }
#endif
    // mp3 stream? starts with FF Fx or FF Ex or "ID3" or "RIFF" with fmt=55h
    else if ( (buf[0]==0xff && (buf[1] & 0xe0)==0xe0)
           || (buf[0]=='I' && buf[1]=='D' && buf[2]=='3')
           || (buf[0]=='R' && buf[1]=='I' && buf[2]=='F' && buf[3]=='F' && buf[20]==0x55) )
    {
        int rate,stereo;
        s->type=Stream;
        s->datahandle=(int)MP3open(filename,fileofs,filesize,&rate,&stereo,errAdd);
        if(s->datahandle) s->playhandle=(int)MIDASplayStreamPolling(stereo?MIDAS_SAMPLE_16BIT_STEREO:MIDAS_SAMPLE_16BIT_MONO,rate,1000*MP3PRECALCSEC);
        s->bufferbytes=rate*(stereo?4:2)*MP3PRECALCSEC;
    }
    // maybe module?
    else
    {
        s->type=Module;
        s->datahandle=(int)MIDASloadModule(filename);
        if(s->datahandle) s->playhandle=(int)MIDASplayModule((MIDASmodule)s->datahandle,loop);
    }
#ifdef EXTENDFILE
    rfSuperStart=0;
    rfSuperSize=0;
    rfSuperXor=0;
#endif
    MIDASreportErr();
}

static void soundSetVolume(struct Sound *s,int volume)
{
    if (volume<0) volume=0; else if (volume>64) volume=64;
    if (s->playhandle)
    switch (s->type)
    {
        case Module:
            MIDASsetMusicVolume((MIDASmodulePlayHandle)s->playhandle,volume/AMPLIFY_MP3);
            break;
        case Sample:
            MIDASsetSampleVolume((MIDASsamplePlayHandle)s->playhandle,volume/AMPLIFY_MP3);
            break;
        case Stream:
            MIDASsetStreamVolume((MIDASstreamHandle)s->playhandle,volume);
            break;
#ifdef DEBUG
        default:
            errAdd(" Set volume: Unknown type.");
#endif
    }
    MIDASreportErr();
}

static void soundSetPanning(struct Sound *s,int panning)
{
    if (panning<-64) panning=-64; else if (panning>64) panning=64;
    if (s->playhandle)
    switch (s->type)
    {
        case Module:
            break;
        case Sample:
            MIDASsetSamplePanning((MIDASsamplePlayHandle)s->playhandle,panning);
            break;
        case Stream:
            MIDASsetStreamPanning((MIDASstreamHandle)s->playhandle,panning);
            break;
#ifdef DEBUG
        default:
            errAdd(" Set panning: Unknown type.");
#endif
    }
    MIDASreportErr();
}

static int soundGetStatus(struct Sound *s)
{
    int result=0;
    if (s->playhandle)
    switch (s->type)
    {
        case Module:
            {
            MIDASplayStatus status;
            MIDASgetPlayStatus((MIDASmodulePlayHandle)s->playhandle,&status);
            result=s->loop?1:1-status.songLoopCount;
            }
            break;
        case Sample:
            result=MIDASgetSamplePlayStatus((MIDASsamplePlayHandle)s->playhandle);
            break;
        case Stream:
            {
            int buffered=MIDASgetStreamBytesBuffered((MIDASstreamHandle)s->playhandle);
            result=1;
            #define BUFLEN 4608
            if ( buffered+2*BUFLEN<=s->bufferbytes )
            {
                unsigned char buf[BUFLEN];
                int decoded=MP3read((void *)s->datahandle,s->loop,buf,BUFLEN);
                if ( !decoded ) result=buffered/8;
                MIDASfeedStreamData((MIDASstreamHandle)s->playhandle,buf,decoded,1/*feedAll*/);
            }
            }
            break;
#ifdef DEBUG
        default:
            errAdd(" Get status: Unknown type.");
#endif
    }
    MIDASreportErr();
    return result;
}

static void soundStop(struct Sound *s)
{
    if (s->datahandle)
    switch (s->type)
    {
        case Module:
            if (s->playhandle)
            {
                MIDASstopModule((MIDASmodulePlayHandle)s->playhandle);
                MIDASreportErr();
            }
            MIDASfreeModule((MIDASmodule)s->datahandle);
            break;
        case Sample:
            if (s->playhandle)
            {
                MIDASstopSample((MIDASsamplePlayHandle)s->playhandle);
                MIDASreportErr();
            }
            MIDASfreeSample((MIDASsample)s->datahandle);
            break;
        case Stream:
            if (s->playhandle)
            {
                MIDASstopStream((MIDASstreamHandle)s->playhandle);
                MIDASreportErr();
            }
            MP3close((void *)s->datahandle);
            break;
#ifdef DEBUG
        default:
            errAdd(" Stop: Unknown type.");
#endif
    }
    MIDASreportErr();
}

////////////////////////////////////////////////////////////////////////

static void RainFreeStopped()
{
    int i;
    for(i=0;i<sounds;i++)
        if(!soundGetStatus(&sound[i]))
        {
            _bprintf(errBuf,errBufLen,"end of %i",sound[i].rainhandle);
            errAdd(errBuf);
            soundStop(&sound[i]);
            sound[i--]=sound[--sounds];
        }
}

////////////////////////////////////////////////////////////////////////

int RainCommand(int command,int param1,int param2,int param3,
                 char *filename,int fileofs,int filesize)
{
    int result=0;
    int i;
    if (command==CMD_PLAY)
        _bprintf(hint,hintlen,"%s,%i,%i",filename,fileofs,filesize);
    else
        _bprintf(hint,hintlen,"%i(%i,%i)",command,param1,param2);
    switch (command)
    {
        case CMD_NONE:
            RainFreeStopped();
            MIDASpoll();
            break;
        case CMD_PLAY:
            rainhandles++;
            if (sounds<SOUNDS)
            {
                sound[sounds].type=None;
                sound[sounds].rainhandle=rainhandles;
                sound[sounds].datahandle=0;
                sound[sounds].playhandle=0;
                soundPlay(&sound[sounds],filename,fileofs,filesize,param1);
                soundSetVolume(&sound[sounds],param2);
                soundSetPanning(&sound[sounds],param3);
                sounds++;
            }
            result=rainhandles;
            break;
        case CMD_VOL:
            for(i=0;i<sounds;i++)
              if(sound[i].rainhandle==param1)
              {
                  soundSetVolume(&sound[i],param2);
              }
            break;
        case CMD_PAN:
            for(i=0;i<sounds;i++)
              if(sound[i].rainhandle==param1)
              {
                  soundSetPanning(&sound[i],param2);
              }
            break;
        case CMD_STOP:
            for(i=0;i<sounds;i++)
              if(sound[i].rainhandle==param1)
              {
                  soundStop(&sound[i]);
                  sound[i--]=sound[--sounds];
              }
            break;
        case CMD_AMPLIFY:
            MIDASsetAmplification(AMPLIFY_ALL*AMPLIFY_MP3*param1);
            break;
        default:
            _bprintf(errBuf,errBufLen," Unknown command %i.",command);
            errAdd(errBuf);
    }
    MIDASreportErr();
    return result;
}

////////////////////////////////////////////////////////////////////////

void RainReset()
{
    // stop all sounds
    while(sounds) soundStop(&sound[--sounds]);
    // reset amplification
    MIDASsetAmplification(AMPLIFY_ALL*AMPLIFY_MP3*100);
    // reset handle number (clients always start from 1)
    rainhandles=0;
    // delete errors
    while(errGet());
}

