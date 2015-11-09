#include <stdio.h>
#include <process.h>
#include <errno.h>
#include <string.h>

main( int argc, char *argv[] )
{
	char cmdline[128];
        strcpy( argv[0]+strlen(argv[0])-3, "LE" );
	execlp( "DOS4GW.EXE", "DOS4GW.EXE", argv[0], getcmd(cmdline), NULL );
	printf( "Stub failed executing DOS4GW.EXE: %s.", strerror( errno ) );
	exit( 99 );
}
