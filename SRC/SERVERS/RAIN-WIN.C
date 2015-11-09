#define CHILD         // pusti a ceka na konec childprocesu
#define __WC32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <share.h>
#include <stdlib.h>    // free

#define VER "MIDAS/WIN server 1.5"
#define configname "midas.win"
#include "rain-mds.inc"

///////////////////////////////////////////////////////////////////////

#define MAX_LINE_LENGTH TALK_STRSIZE
static char          input[MAX_LINE_LENGTH];
static int           inputlen=0;
static FILE          *rainin=NULL;
static FILE          *rainout=NULL;
static char          argpart1[10+MAX_PATH];
static char          argpart2[1+MAX_PATH];
static char          argfull[10+MAX_PATH+1+MAX_PATH];
#define raincmdname  (argpart1+10)
#define rainrepname  (argpart2+1)


static void ParseString(char *input)
{
    int  command,param1,param2,param3;
    char filename[TALK_STRSIZE];
    int  fileofs,filesize;
    command=0;
    sscanf(input,"%i %i %i %i %i %i %s",&command,&param1,&param2,&param3,&fileofs,&filesize,filename);
    if(command==CMD_TERMINATE)
    {
        int i;
        if(persist)
        {
            RainReset();
//            fclose(rainin); //shutdown input, let next client create it
//            remove(raincmdname);
//            rainin=NULL;
            return;
        }
        result=param1;
        for(i=0;i<30;i++)
        {
            if(endRain) break;
            Sleep(100);
        }
        endRain=1;
    }
    else
        RainCommand(command,param1,param2,param3,filename,fileofs,filesize);
}

static void TalkPoll()
{
    if(!rainin)
    {
        rainin=_fsopen(raincmdname,"rt",SH_DENYNO);
        if(rainin) setbuf(rainin,NULL);
    }
    if(rainin)
    {
        ParseString("0"); //just 50 or more Hz polling with no command
        a:
        fgets(input+inputlen,MAX_LINE_LENGTH-inputlen,rainin);
        inputlen+=strlen(input+inputlen);
        if ((inputlen && input[inputlen-1]=='\n') || inputlen>=MAX_LINE_LENGTH-1)
        {
            ParseString(input); //parse real command
            inputlen=0;
            input[0]=0;
            goto a;  //parse all commands until end of file
        }
    }
    {
        char *err;
        while (err=errGet())
        {
            fprintf(rainout,"%s\n",err);
            free(err);
        }
    }
}

///////////////////////////////////////////////////////////////////////

static void FillError()
{
    FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM,
      NULL,
      GetLastError(),
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR) &error,
      BUFSIZE,
      NULL
      );
}

#define APICALL(func) if(!(func)) {FillError();return;}
#define APICALLF(func) if(!(func)) {FillError();return FALSE;}
#define APICALLP(func) if(!(func)) {FillError();printf(error);error[0]=0;return;}
#define APICALLPF(func) if(!(func)) {FillError();printf(error);error[0]=0;return FALSE;}

///////////////////////////////////////////////////////////////////////

#ifdef CHILD

PROCESS_INFORMATION ProcessInformation={0,0,0,0};

static void myWaitThread(void *p)
{
    DWORD dw;
    switch( WaitForSingleObject(ProcessInformation.hProcess,INFINITE) )
    {
      case WAIT_FAILED:
        FillError();
        break;
      default:
        if( !GetExitCodeProcess(ProcessInformation.hProcess,&dw) )
          FillError();
        else
          result=dw;
    }
    endRain=1;
}

#endif

static BOOL AppStarted(const char **argv)
{
#ifdef CHILD
    STARTUPINFO StartupInfo={
      sizeof(STARTUPINFO),
      NULL,
      NULL,
      NULL, // title
      0,0,0,0,0,0,0,0,0, // don't override default window size, position etc
      0,
      0,
      NULL, // hStdInput;
      NULL, // hStdOutput;
      NULL // hStdError;
    };
    char commandline[512];
    int arg=0;
    commandline[0]=0;
    do {
      strcat( commandline, argv[arg] );
      strcat( commandline, " " );
    } while(argv[++arg]);
    APICALLF( CreateProcess(
      NULL,
      commandline,
      NULL, // process attributes
      NULL, // thread attributes
      0, // inherit handles
      0, // ABOVE_NORMAL_PRIORITY_CLASS
      NULL, // environment
      NULL, // current directory name
      &StartupInfo,
      &ProcessInformation
      ) );
    if( !_beginthread(myWaitThread, 8192, argv) )
    {
      _bprintf(error,BUFSIZE,"Couldn't create thread.\n");
      return FALSE;
    }
#endif
    APICALLF(SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS));
    return TRUE;
}

static void AppClose()
{
}

///////////////////////////////////////////////////////////////////////

void PlatformSpecificCore(const char **argv)
{
    int l_temppath;
    char temppath[MAX_PATH];
    l_temppath=GetTempPath(MAX_PATH,temppath);
    if(l_temppath>=MAX_PATH) _bprintf(error,BUFSIZE,"Path to temporary files is too long."); else {
     strcpy(argpart1,"RAIN=FILE ");
     strcpy(argpart2,",");
     if(!GetTempFileName(temppath,"raincmd",0,raincmdname)) _bprintf(error,BUFSIZE,"Unable to create temporary filename."); else {
      if(!GetTempFileName(temppath,"rainrep",0,rainrepname)) _bprintf(error,BUFSIZE,"Unable to create temporary filename."); else {
       rainout=fopen(rainrepname,"w+t");
       if (!rainout) _bprintf(error,BUFSIZE,"Unable to open rain reply file."); else {
        setbuf(rainout,NULL);
        fprintf(rainout,FULL "\n");
        if (remove(raincmdname)) _bprintf(error,BUFSIZE,"Unable to remove rain command file."); else {
         input[0]=0;
         if ( putenv(strcat(strcpy(argfull,argpart1),argpart2)) ) _bprintf(error,BUFSIZE,"Too small environment.\n"); else {

          if (!quiet)
          {
              printf("Resident Audio Interface installed.\n");
              installed=1;
          }
          if ( AppStarted(argv) )
          {
            while ( !endRain )
            {
              TalkPoll();
              Sleep(5); //max 15ms, longer sleep interrupts mp3 decoding
            }
            AppClose();
          }
          putenv("RAIN=");
         }
        }
        if(rainin) fclose(rainin);
       }
       fclose(rainout);
      }
      remove(rainrepname);
     }
     remove(raincmdname);
    }
}
