// main() for all midas based rain servers

#include <errno.h>
#include <fcntl.h>     // O_CREAT na debuggink
#include <io.h>        // W_OK
#include <process.h>
#include <stdio.h>
#include <midasdll.h>
#include "raindef.h"
#include "raincore.h"

#define             FULL "Resident Audio Interface 1.0 (" VER ")"

#define             BUFSIZE 300
static char         error[BUFSIZE]="";
static int          result=EXITCODE_NOTRUN;
static int          endRain=0;

static int          config=0;
static int          persist=0;
static int          quiet=0;
static int          installed=0;

extern int *midasSD;
extern int MixNoSound;

void PlatformSpecificCore();

///////////////////////////////////////////////////////////////////////
#ifdef WIN32_LEAN_AND_MEAN

static HWND _consoleHwnd = NULL;

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    char txt[256];
    if ( GetWindowText(hwnd, txt, 255) )
        if ( !strcmp(txt, FULL) )
        {
            _consoleHwnd = hwnd;
            return FALSE;
        }
    return TRUE;
}

static int consoleHwnd()
{
    // Hack - try to figure out our console window's window handle:
    char        oldTitle[256];
    GetConsoleTitle(oldTitle, 255);
    SetConsoleTitle(FULL);
    Sleep(250);
    EnumWindows(&EnumWindowsProc, (LPARAM) NULL);
    SetConsoleTitle(oldTitle);
    if ( _consoleHwnd == NULL )
        // Panic! Couldn't find console window window handle.
        MIDASsetOption(MIDAS_OPTION_DSOUND_MODE, MIDAS_DSOUND_DISABLED);
//printf("hwnd=%i\n",(int)_consoleHwnd);
    return (int)_consoleHwnd;
}

#endif

///////////////////////////////////////////////////////////////////////

static char         configpath[128];
static char        *usedconfig;

static int GetConfig(const char *exepath)
{
    // fill configpath
    char *name=configpath;
    char *p;
    strcpy(configpath,exepath);
    p=strrchr(configpath,'\\')+1; if(p>name) name=p;
    p=strrchr(configpath,':')+1; if(p>name) name=p;
    strcpy(name,configname);

    if ( config )
    {
        if ( !MIDASconfig() ) return 0;
    }
    else
    {
        usedconfig=configname;
        //don't read config from readonly file (possibly came from different computer)
        if ( access(usedconfig,F_OK+R_OK+W_OK)==0 )
          if ( MIDASloadConfig(usedconfig) ) return 1;
        usedconfig=configpath;
        if ( access(usedconfig,F_OK+R_OK+W_OK)==0 )
          if ( MIDASloadConfig(usedconfig) ) return 1;
    }
    return 1;
}

static void LeaveConfig()
{
    if ( config )
    {
        usedconfig=configname;
        if ( access(usedconfig,F_OK+R_OK+W_OK)==0 )
          if ( MIDASsaveConfig(usedconfig) ) return;
        usedconfig=configpath;
        if ( access(usedconfig,F_OK+R_OK+W_OK)==0 )
          if ( MIDASsaveConfig(usedconfig) ) return;
        usedconfig=configname;
        if ( MIDASsaveConfig(usedconfig) ) return;
        usedconfig=configpath;
        if ( MIDASsaveConfig(usedconfig) ) return;
    }
}

///////////////////////////////////////////////////////////////////////

int main(int argc, const char **argv)
{
    int spawnarg=1; //which arg is name of application to spawn
    int runprog;
    int help=0;
//close(1);
//open("z_stdout",O_WRONLY|O_CREAT|O_BINARY);
    MIDASstartup();
    nextarg:
     if (spawnarg<argc)
      if (argv[spawnarg])
       if (argv[spawnarg][0]=='-' || argv[spawnarg][0]=='/')
        if (argv[spawnarg][1]!=0)
         if (argv[spawnarg][2]==0)
          switch (argv[spawnarg][1])
          {
            case '?':
            case 'h':help   =1;spawnarg++;goto nextarg;
            case 'c':config =1;spawnarg++;goto nextarg;
            case 'p':persist=1;spawnarg++;goto nextarg;
            case 'q':quiet  =1;spawnarg++;goto nextarg;
          }
    runprog=spawnarg<argc && argv[spawnarg]!=NULL;
    if ( help || ( !config && !runprog ) ) _bprintf(error,BUFSIZE,"\n" FULL ".\n\nUsage: RAIN [-c] [-p] [-q] [program [its parameters]]\n  -c      ...configure audio system and save config\n  -p      ...persistent server, don't end with client\n  -q      ...quiet start end exit without banner\n  program ...run program\n"); else {
      if(getenv("RAIN")) _bprintf(error,BUFSIZE,"RAIN always running or environment variable RAIN used by another program.\n"); else {
        MIDASsetOption(MIDAS_OPTION_MIXING_MODE, MIDAS_MIX_HIGH_QUALITY);
#ifdef WIN32_LEAN_AND_MEAN
        MIDASsetOption(MIDAS_OPTION_DSOUND_HWND, (DWORD) consoleHwnd());
        MIDASsetOption(MIDAS_OPTION_DSOUND_MODE, MIDAS_DSOUND_STREAM);
//        MIDASsetOption(MIDAS_OPTION_DSOUND_MODE, MIDAS_DSOUND_PRIMARY);
//        MIDASsetOption(MIDAS_OPTION_MIXBUFLEN, 500); // for waveOut default 500 is ok
        MIDASsetOption(MIDAS_OPTION_DSOUND_BUFLEN, 200); // with kt266+ac97+winMe default 100 is low
#endif
        if ( GetConfig(argv[0]) ) {
          if ( MIDASinit() ) {
            if ( midasSD == &MixNoSound ) {
              printf("No soundcard found.\n");
              result=EXITCODE_NO_SOUNDCARD;
            } else {
              if ( MIDASopenChannels(32+8) ) {
                if ( MIDASallocAutoEffectChannels(8) ) {
                  RainCommand(CMD_AMPLIFY,100,0,0,NULL,0,0);
                  if ( runprog ) PlatformSpecificCore(argv+spawnarg);
                }
              }
            }
            MIDASclose();
          }
          LeaveConfig();
        }
      }
    }
    if ( *error ) printf(error);
    switch ( MIDASgetLastError() ) {
       case 0:  break;
       case 41://errDeviceBusy
                printf("No soundcard found or soundcard busy.\n");
                result=EXITCODE_NO_SOUNDCARD;
                break;
       default: printf("MIDAS error: %s\n", MIDASgetErrorMessage(MIDASgetLastError()));
       }
    if ( installed ) printf("Resident Audio Interface uninstalled.\n");
    return result;
}
