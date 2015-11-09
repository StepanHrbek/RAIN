#include <string.h>
#include <dos.h>
#include <malloc.h>

#define VER "MIDAS/DOS server 1.5"
#define configname "midas.dos"
#include "rain-mds.inc"

////////////////////////////////////////////////////////////////////////

static char           *talk     =NULL;
static void           *pool     =NULL;

////////////////////////////////////////////////////////////////////////

static void __far *myIntStack;
static void __far *callerIntStack;

void stackInit();
#pragma aux stackInit = \
    "mov    word ptr myIntStack+4,ss" \
    "mov    dword ptr myIntStack,esp" \
    "sub    dword ptr myIntStack,5000";

void stackInstall();
#pragma aux stackInstall = \
    "mov    word ptr callerIntStack+4,ss" \
    "mov    dword ptr callerIntStack,esp" \
    "mov    ss,word ptr myIntStack+4" \
    "mov    esp,dword ptr myIntStack";

void stackUninstall();
#pragma aux stackUninstall = \
    "mov    ss,word ptr callerIntStack+4" \
    "mov    esp,dword ptr callerIntStack";

void DStoES();
#pragma aux DStoES = \
    "push   ds" \
    "pop    es";

static void (__interrupt __far *prev_int_12)()=NULL;

static void __interrupt __far new_int_12()
{
    static int inRain=0;
    DStoES();
    inRain++;
    if ( inRain==1 )
    {
        stackInstall();
        _enable();
        if(pool) {free(pool);pool=NULL;}
        if(TALK_CMD==CMD_TERMINATE) RainReset();
        else RainCommand(TALK_CMD,TALK_PARAM1,TALK_PARAM2,TALK_PARAM3,TALK_FILE_NAME,TALK_FILE_OFS,TALK_FILE_SIZE);
        {
            char *err=errGet();
            if(err) strncpy(TALK_REPORT,err,TALK_STRSIZE);
        }
        _disable();
        stackUninstall();
    }
    inRain--;
    _chain_intr( prev_int_12 );
}

///////////////////////////////////////////////////////////////////////

static void Spawn(const char **argv)
{
    result=spawnvp(P_WAIT,argv[0],argv);
    if(result==-1)
    {
        result=EXITCODE_NOTRUN;
        switch (errno)
        {
            case E2BIG :errno=0;_bprintf(error,BUFSIZE,"Too big spawn arglist.\n");break;
            case EINVAL:errno=0;_bprintf(error,BUFSIZE,"Invalid spawn mode.\n");break;
            case ENOENT:errno=0;_bprintf(error,BUFSIZE,"%s not found.\n",argv[0]);break;
            case ENOMEM:errno=0;_bprintf(error,BUFSIZE,"Not enough memory to spawn.\n");break;
            default    :errno=0;_bprintf(error,BUFSIZE,"Spawn failed.\n");break;
        }
    }
    endRain=1;
}

////////////////////////////////////////////////////////////////////////

static unsigned short selector  =0;
static union REGS     r;

void PlatformSpecificCore(const char **argv)
{
    int i;
    int segment;
    char *envseg="RAIN=SEG 00000";
    stackInit();
    memset(&r,0,sizeof(r));
    r.x.eax = 0x0100;
    r.x.ebx = (TALK_BUFSIZE + 15) >> 4;
    int386 (0x31, &r, &r);
    if (r.x.cflag) _bprintf(error,BUFSIZE,"Allocation DOS memory failed.\n"); else {
      segment = r.w.ax;
      selector = r.w.dx;
      talk=(char *) (segment << 4);
      memset(talk,0,TALK_BUFSIZE);
      strncpy(TALK_REPORT,FULL,TALK_STRSIZE);
      for(i=0;i<5;i++)
      {
          envseg[13-i]=48+segment%10;
          segment/=10;
      }
      if ( putenv(envseg) ) _bprintf(error,BUFSIZE,"Too small environment.\n"); else {
        pool=malloc(2200000);
        prev_int_12 = _dos_getvect( 0x12 );
        _dos_setvect( 0x12, new_int_12 );

        MIDASstartBackgroundPlay(18);
        if (!quiet)
        {
            printf("Resident Audio Interface installed.\n");
            installed=1;
        }
        Spawn(argv);

        _dos_setvect( 0x12, prev_int_12 );
        if ( pool ) free(pool);
        putenv("RAIN=");
      }
      memset(&r,0,sizeof(r));
      r.x.eax = 0x0101;
      r.x.edx = selector;
      int386 (0x31, &r, &r);
      selector = 0;
    }
}
