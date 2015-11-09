// RAIN high-level C/C++ API implementation
//
// tested compilers/targets:
//  - DJGPP egcs-2.91.60 DOS 32bit protected mode
//  - MinGW egcs-2.91.66 Windows 32bit protected mode
//  - Cygwin gcc-2.95.2 Windows 32bit protected mode
//  - Watcom C/C++ 11.0 DOS 16bit real mode
//  - Watcom C/C++ 11.0 DOS 32bit protected mode
//  - Watcom C/C++ 11.0 Windows 32bit protected mode
//
// differences from pascal API (rain.pas):
//  - no support for asynchronous calls (may be added later in case of need)
//  - exitcode is NOT(?) known during 'atexit' sequence, so RAIN always sends 0
//    to server (may be fixed by adding void RainSetExitCode() in case of need)

#include "rain.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define CMD_NONE	0
#define CMD_PLAY	1
#define CMD_STOP	2
#define CMD_VOL		3
#define CMD_PAN		4
#define CMD_AMPLIFY	5
#define CMD_TERMINATE	-1

int protokol=0;

// protokol 1
  #ifndef _WIN32
  #include <dos.h>	// int86()
  #ifdef DJGPP
   #include <sys/nearptr.h>
  #endif
  #endif
  #if INT_MAX==2147483647
   #define i32			"%i"
   #define STRCPY		strcpy
   #define PTR32BIT
  #else
   #define i32			"%li"
   #define STRCPY		_fstrcpy
   #define PTR32BIT		far
  #endif
  I32 volatile PTR32BIT *talk = NULL;
  #define TALK_BUFSIZE		1024 // talk buffer length
  #define TALK_STRSIZE		256  // max talk string length including zero end
  #define TALK_CMD		0
  #define TALK_PARAM1		1
  #define TALK_PARAM2		2
  #define TALK_PARAM3		3
  #define TALK_FILE_OFS		4
  #define TALK_FILE_SIZE	5
  #define TALK_FILE_NAME	((char PTR32BIT *)(talk+6))
  #define TALK_REPORT		((char PTR32BIT *)(talk+128))

// protokol 2
  FILE *raincmd;
  FILE *rainrep;
  char *raincmdname;
  char *rainrepname;
  char *ReadReply()
  {
    static char input[255]; // input stream without any terminating 0
    static int inputlen=0; // chars in input
    static int skipeol=0;
    char *p10;
    static char result[256]; // input line terminated by 0
    *result=0;
    inputlen+=fread(input+inputlen,sizeof(char),255-inputlen,rainrep);
    p10=memchr(input,10,inputlen);
    if(p10) {
      if(p10[-1]==13) {memcpy(p10-1,p10,input+inputlen-p10);p10--;}
      if(!skipeol) {memcpy(result,input,p10-input);result[p10-input]=0;}
      inputlen-=p10+1-input;
      memcpy(input,p10+1,inputlen);
      skipeol=0;
      }
    else if(inputlen==255) {
      if(!skipeol) {memcpy(result,input,255);result[255]=0;}
      inputlen=0;
      skipeol=1;
      }
    return result;
  }

void RainExitProc();

char *RainInit()
{
  static char result[256]="";
  char *env=getenv("RAIN");
  static int inited=0;
  if(inited) return "RainInit may be called just once.";
  inited=1;
  atexit(RainExitProc);
  if(env) {
    // if possible (not in Mingw/Cygwin where getenv() returns ptr into copy of environment)
    // invalidate rain variable so child processes won't think rain is for them
    assert(env[-1]=='=');
    env[-2]='X';
    // is rain talking via memory?
    #ifndef _WIN32
    if(env[0]=='S' && env[1]=='E' && env[2]=='G' && env[3]==' ') {
      I32 segment=atoi(env+4);
      if(segment) {
        #if INT_MAX==2147483647
          // looks like flat pointer 0:32
          assert(sizeof(void *)==4);
          #ifdef DJGPP
            if(!__djgpp_nearptr_enable()) return result;
            talk=(I32 *)(__djgpp_conventional_base+(segment<<4));
          #else
            talk=(I32 *)(segment<<4);
          #endif
        #else
          // looks like real mode pointer 16:16
          assert(sizeof(void *)==2);
          assert(sizeof(void far *)==4);
          talk=(I32 far *)(segment<<16);
        #endif
        STRCPY(result,TALK_REPORT);
        *TALK_REPORT=0;
        protokol=1;
        return result;
        }
      }
    #endif
    // is rain talking via file?
    if(env[0]=='F' && env[1]=='I' && env[2]=='L' && env[3]=='E' && env[4]==' ') {
      static char filenames[256];
      char *carka;
      strcpy(filenames,env+5);
      carka=strchr(filenames,',');
      if(*carka) {
        raincmdname=filenames;
        *carka=0;
        rainrepname=carka+1;
        rainrep=fopen(rainrepname,"rb");
        if(rainrep) {
          raincmd=fopen(raincmdname,"at");
          if(raincmd) {
            char *reply;
            do reply=ReadReply(); while(!*reply); // wait for server version
            strcpy(result,reply);
            do reply=ReadReply(); while(*reply); // skip replies for previous clients
            protokol=2;
            return result;
            }
          fclose(rainrep);
          }
        }
      }
    }
  return result;
}

I32 RainCommand(I32 command,I32 param1,I32 param2,I32 param3,char *filename,I32 fileofs,I32 filesize)
{
  static int raincommands=0;
  I32 result=0;

  // just one command at one time
  raincommands++;
  if(raincommands==1)
  {

    char reply[TALK_STRSIZE];

    // send command, read reply
    switch(protokol) {
      #ifndef _WIN32
      case 1:{
        union REGS regs;
        talk[TALK_PARAM1]=param1;
        talk[TALK_PARAM2]=param2;
        talk[TALK_PARAM3]=param3;
        if(!filename) *TALK_FILE_NAME=0; else STRCPY(TALK_FILE_NAME,filename);
        talk[TALK_FILE_OFS]=fileofs;
        talk[TALK_FILE_SIZE]=filesize;
        talk[TALK_CMD]=command;
        #if INT_MAX==2147483647
         int386(0x12,&regs,&regs);
        #else
         int86(0x12,&regs,&regs);
        #endif
        STRCPY(reply,TALK_REPORT);
        if(*reply) *TALK_REPORT=0;
        break;
        }
      #endif
      case 2:{
        switch(command) {
          case CMD_PLAY     :fprintf(raincmd,i32" "i32" "i32" "i32" "i32" "i32" %s\n",command,param1,param2,param3,fileofs,filesize,filename);break;
          case CMD_VOL      :
          case CMD_PAN      :fprintf(raincmd,i32" "i32" "i32"\n",command,param1,param2);break;
          case CMD_STOP     :
          case CMD_AMPLIFY  :
          case CMD_TERMINATE:fprintf(raincmd,i32" "i32"\n",command,param1);break;
          }
        fflush(raincmd);
        strcpy(reply,ReadReply());
        break;
        }
      default:
        *reply=0;
      }
    // return handle if needed
    if(command==CMD_PLAY) {
      static I32 lasthandle=0;
      result=++lasthandle;
      }
    // call reporters
    if(*reply)
      if(*reply==' ') ReportError(reply+1); else
       if(reply[0]=='e' && reply[1]=='n' && reply[2]=='d' && reply[3]==' ' && reply[4]=='o' && reply[5]=='f' && reply[6]==' ') {
         I32 handle=atoi(reply+7);
         if(!handle) goto unknown;
         ReportEnd(handle);
         }
       else {
         char unk[TALK_STRSIZE+20];
         unknown:
         sprintf(unk,"Unknown report: %s",reply);
         ReportError(unk);
         }
    }
  raincommands--;

  return result;
}

void RainPoll()
{
  RainCommand(CMD_NONE,0,0,0,NULL,0,0);
}

I32 RainPlay(char *filename,I32 fileofs,I32 filesize,I32 loop,I32 volume,I32 panning)
{
  return RainCommand(CMD_PLAY,loop,volume,panning,filename,fileofs,filesize);
}

void RainSetVolume(I32 handle,I32 volume)
{
  RainCommand(CMD_VOL,handle,volume,0,NULL,0,0);
}

void RainSetPanning(I32 handle,I32 panning)
{
  RainCommand(CMD_PAN,handle,panning,0,NULL,0,0);
}

void RainStop(I32 handle)
{
  RainCommand(CMD_STOP,handle,0,0,NULL,0,0);
}

void RainAmplification(I32 amp)
{
  RainCommand(CMD_AMPLIFY,amp,0,0,NULL,0,0);
}

void IgnoreEnd(I32 handle)
{
  //printf("RAIN: end of %i\n",(int)handle);
}

void EchoError(char *err)
{
  fprintf(stderr,"RAIN: %s\n",err);
}

void(*ReportError)(char *)=EchoError;
void(*ReportEnd)(I32)=IgnoreEnd;

void RainExitProc()
{
  char *env;
  RainCommand(CMD_TERMINATE,0/*exitcode*/,0,0,NULL,0,0);
  // validate rain variable to get rain visible for possible next clients
  env=getenv("RAIX");
  if(env) env[-2]='N';
}

