// amp interface common for all rain servers

#include <malloc.h>
#include "binfstd.h"
#include "binfarc.h"
#include "ampdec.h"
#include "rain-amp.h"

struct MP3
{
    sbinfile dmfile;
    abinfile mp3file;
    binfile *instream;
    ampegdecoder dec;
};

#define Mp3 ((struct MP3 *)mp3)

void *MP3open(char *filename,int fileofs,int filesize,
              int *freq,int *stereo,void errAdd(char *))
{
    MP3 *mp3=new MP3;
    if ( mp3 )
    {
        if ( mp3->dmfile.open(filename,sbinfile::openro) )
            errAdd(" Unable to open file (with mp3).");
        else
        {
            if ( filesize && mp3->mp3file.open(mp3->dmfile,fileofs,filesize) )
                errAdd(" Unable to open file (mp3).");
            else
            {
                if (filesize>0) mp3->instream=&mp3->mp3file;
                    else mp3->instream=&mp3->dmfile;
                if (!mp3->dec.open(*mp3->instream, *freq, *stereo, 1/*0=float,1=int*/, 0/*no downmix*/, 0/*no force to mono-stereo*/))
                {
                    mp3->dec.ioctl(mp3->dec.ioctlrbufset,NULL);
                    return mp3;
                }
                errAdd(" Error in mp3 file.");
                mp3->mp3file.close();
            }
            mp3->dmfile.close();
        }
        delete mp3;
    }
    return NULL;
}

int MP3read(void *mp3,int loop,void *sampbuf, int len)
{
    if ( !mp3 ) return 0;
    int readen=Mp3->dec.read(sampbuf, len);
    if (!readen && loop)
    {
        Mp3->dec.seek(0);
        readen=Mp3->dec.read(sampbuf, len);
    }
    return readen;
};

void MP3close(void *mp3)
{
    if ( !mp3 ) return;
    Mp3->dec.close();
    Mp3->mp3file.close();
    Mp3->dmfile.close();
    delete Mp3;
};
