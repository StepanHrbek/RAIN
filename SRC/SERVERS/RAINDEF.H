#ifndef __RAINDEF_H
#define __RAINDEF_H

// rain talk buffer

#define TALK_BUFSIZE 1024 //talk buffer length
#define TALK_STRSIZE 256  //max talk string length including zero end

#define TALK_CMD       (*(volatile int*)(talk+ 0))       //parameters for rain
#define TALK_PARAM1    (*(volatile int*)(talk+ 4))       //
#define TALK_PARAM2    (*(volatile int*)(talk+ 8))       //
#define TALK_PARAM3    (*(volatile int*)(talk+12))
#define TALK_FILE_OFS  (*(volatile int*)(talk+16))
#define TALK_FILE_SIZE (*(volatile int*)(talk+20))
#define TALK_FILE_NAME         (( char*)(talk+24))

#define TALK_REPORT            (( char*)(talk+512))      //reports from rain

// rain commands

#define CMD_NONE       0
#define CMD_PLAY       1 // param1=loop,param2=type,filename,ofs,size
#define CMD_STOP       2 // param1=handle
#define CMD_VOL        3 // param1=handle,param2=vol
#define CMD_PAN        4 // param1=handle,param2=pan
#define CMD_AMPLIFY    5 // param1=amplification
#define CMD_TERMINATE -1

// rain exit codes

#define EXITCODE_NO_SOUNDCARD 98 // no soundcard detected, RAIN ends immediately
                                 // loader can recognize exitcode and run your 
                                 // application without RAIN (running with 
                                 // silent RAIN would waste memory)
#define EXITCODE_NOTRUN 99       // application was not run due to some error

#endif