#include "rain.h"
#include <stdio.h>	// for server name printing (puts())
#include <conio.h>	// for keyboard scanning (kbhit())

I32 ourSoundHandle;
int ourSoundEnded=0;

void OurReportEnd(I32 handle)
{
 if(handle==ourSoundHandle) ourSoundEnded=1;
}

int main(int argc,char **argv)
{
 // initialize RAIN and write name of server
 puts(RainInit());

 // do we have anything to play?
 if(argc<2) {puts("No soundfile to play.");return 1;}

 // hook our function that will be called each time some sound ends
 ReportEnd=OurReportEnd;

 // start playing sound
 ourSoundHandle=RainPlay(argv[1],0,0,0/*no loop*/,64/*volume*/,0/*panning*/);

 // repeat until sound end or user abort
 do
 {
   RainPoll();
 }
 while(!ourSoundEnded && !kbhit());

 // clear keypresses
 while(kbhit()) getch();

 return 0;
}
