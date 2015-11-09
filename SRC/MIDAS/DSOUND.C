/*      dsound.c
 *
 * DirectSound Sound Device
 *
 * $Id: dsound.c,v 1.14 1997/07/31 14:30:04 pekangas Exp $
 *
 * Copyright 1996,1997 Housemarque Inc.
 *
 * This file is part of MIDAS Digital Audio System, and may only be
 * used, modified and distributed under the terms of the MIDAS
 * Digital Audio System license, "license.txt". By continuing to use,
 * modify or distribute this file you indicate that you have
 * read the license and understand and accept it fully.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

/* We need to define this, otherwise the dsound.h include won't compile
   with Visual C: (funny) */
#ifndef WIN32
#define WIN32
#endif

#include <dsound.h>

#include "lang.h"
#include "mtypes.h"
#include "errors.h"
#include "mglobals.h"
#include "sdevice.h"
#include "mmem.h"
#include "dsm.h"

#ifdef DEBUGMESSAGES
#  define DEBUG_CODE(x) x
#else
#  define DEBUG_CODE(x)
#endif

RCSID(char const *dsound_rcsid = "$Id: dsound.c,v 1.14 1997/07/31 14:30:04 pekangas Exp $";)

#define DSVERSION 1.10
#define DSVERSTR "1.10"


/* Sound Device information */

    /* Sound Card names: */
static char     *dsCardName = "DirectSound";


/* Critical section object to prevent thread synchronization problems: */
static CRITICAL_SECTION deviceUsed;

/* Sound Device internal static variables */
static unsigned mixRate, outputMode, mixMode;
static unsigned bufferLen;
static unsigned mixElemSize;
static unsigned bufferLeaveSpace;

static unsigned updateMix;              /* number of elements to mix between
                                           two updates */
static unsigned mixLeft;                /* number of elements to mix before
                                           next update */
static unsigned writePos;               /* write position */
static unsigned readPos;                /* read position */

static LPDIRECTSOUND ds;                /* the DirectSound object */
static LPDIRECTSOUNDBUFFER buf;         /* the buffer we will be using */
static HWND         hwnd;               /* the HWND for DirectSound */

static HMODULE dsModule = NULL;         /* dsound.dll module handle */

static HRESULT (WINAPI *dsDirectSoundCreate)(const GUID *lpGUID,
                                             LPDIRECTSOUND *ppDS,
                                             IUnknown FAR *pUnkOuter) = NULL;

static int      emulation;              /* are we running in emulation? */
static int      primary;                /* primary buffer mode? */

static int      suspended;              /* is playback suspended? */


/****************************************************************************\
*       enum dsFunctIDs
*       -----------------
* Description:  ID numbers for DirectSound Sound Device functions
\****************************************************************************/

enum dsFunctIDs
{
    ID_dsDetect = ID_ds,
    ID_dsInit,
    ID_dsClose,
    ID_dsGetMode,
    ID_dsOpenChannels,
    ID_dsSetAmplification,
    ID_dsGetAmplification,
    ID_dsSetUpdRate,
    ID_dsStartPlay,
    ID_dsPlay,
    ID_dsInitDevice,
    ID_dsCloseDevice,
    ID_dsSuspend,
    ID_dsResume
};



/* Local prototypes: */
int CALLING dsSetUpdRate(unsigned updRate);





/****************************************************************************\
*
* Function:     static int dsError(HRESULT error)
*
* Description:  Converts a DirectSound error code to MIDAS error code
*
* Input:        MMRESULT error          Windows multimedia system error code
*
* Returns:      MIDAS error code
*
\****************************************************************************/

static int dsError(HRESULT error)
{
#define DSERR(err, str, merr) \
    case err: \
        DEBUG_CODE(puts("DirectSound error: " str)); \
        return merr; \

    switch ( error )
    {
        DSERR(DS_OK, "DS_OK", OK)
        DSERR(DSERR_ALLOCATED, "DSERR_ALLOCATED", errDeviceBusy)
        DSERR(DSERR_ALREADYINITIALIZED, "DSERR_ALREADYINITIALIZED", \
              errDeviceBusy)
        DSERR(DSERR_BADFORMAT, "DSERR_BADFORMAT", errBadMode)
        DSERR(DSERR_BUFFERLOST, "DSERR_BUFFERLOST", errSDFailure)
        DSERR(DSERR_CONTROLUNAVAIL, "DSERR_CONTROLUNAVAIL", errUndefined)
        DSERR(DSERR_GENERIC, "DSERR_GENERIC", errUndefined)
        DSERR(DSERR_INVALIDCALL, "DSERR_INVALIDCALL", errUnsupported)
        DSERR(DSERR_INVALIDPARAM, "DSERR_INVALIDPARAM", errInvalidArguments)
        DSERR(DSERR_NOAGGREGATION, "DSERR_NOAGGREGATION", errUndefined)
        DSERR(DSERR_NODRIVER, "DSERR_NODRIVER", errDeviceNotAvailable)
        DSERR(DSERR_OTHERAPPHASPRIO, "DSERR_OTHERAPPHASPRIO", errDeviceBusy)
        DSERR(DSERR_OUTOFMEMORY, "DSERR_OUTOFMEMORY", errOutOfMemory)
        DSERR(DSERR_PRIOLEVELNEEDED, "DSERR_PRIOLEVELNEEDED", errUndefined)
        DSERR(DSERR_UNINITIALIZED, "DSERR_UNINITIALIZED", errUndefined)
        DSERR(DSERR_UNSUPPORTED, "DSERR_UNSUPPORTED", errUnsupported)
    }

    DEBUG_CODE(puts("DirectSound undefined error"));
    return errUndefined;
}

    /* Error code passing macros for multimedia system errors - similar to
       PASSERROR in errors.h: */

#ifdef DEBUG
    #define PASSDSERR(error_, functID) { error = dsError(error_); \
        errAdd(dsError(error_), functID); return error; }
#else
    #define PASSDSERR(error_, functID) return dsError(error_);
#endif




/****************************************************************************\
*
* Function:     int dsDetect(int *result)
*
* Description:  Detects a DirectSound Sound Device
*
* Input:        int *result             pointer to detection result
*
* Returns:      MIDAS error code. Detection result (1 if detected, 0 if not)
*               is written to *result.
*
\****************************************************************************/

int CALLING dsDetect(int *result)
{
    HRESULT     dsResult;
    DSCAPS      caps;

    /* First of all, don't detect DirectSound if DirectSound support is
       disabled: */
    if ( mDSoundMode == M_DSOUND_DISABLED )
    {
        DEBUG_CODE(puts("dsDetect: Not detected: DirectSound disabled"));
        *result = 0;
        return OK;
    }

    /* We should try to use DirectSound - check that we have the HWND: */
    if ( mDSoundHwnd == NULL )
    {
        *result = 0;
        ERROR(errDSoundNoHwnd, ID_dsDetect);
        return errDSoundNoHwnd;
    }

    hwnd = mDSoundHwnd;

    /* If the DirectSound object has been set, or DirectSound mode is forced,
       all is clear - we'll use it no matter what: */
    if ( (mDSoundObject != NULL) || (mDSoundMode == M_DSOUND_FORCE_STREAM) )
    {
        DEBUG_CODE(puts("dsDetect: Detected: DirectSound forced"));
        *result = 1;
        return OK;
    }

    /* Check if DirectSound is disabled from the setup: */
    if ( mDisableDSound )
    {
        *result = 0;
        return OK;
    }

    /* Try to load the DirectSound DLL and get the address for
       DirectSoundCreate: */
    if ( (dsModule = LoadLibrary("dsound.dll")) == NULL )
    {
        DEBUG_CODE(puts("dsDetect: Not detected: Couldn't load dsound.dll"));
        *result = 0;
        return OK;
    }
    if ( (dsDirectSoundCreate = (HRESULT (WINAPI *)(const GUID *,
                                                    LPDIRECTSOUND *,
                                                    IUnknown FAR *))
          GetProcAddress(dsModule, "DirectSoundCreate")) == NULL )
    {
        DEBUG_CODE(puts("dsDetect: Not detected: Couldn't get "
                        "DirectSoundCreate address"));
        *result = 0;
        return OK;
    }

    DEBUG_CODE(puts("dsDetect: dsound.dll loaded OK"));

    /* Try to create the DirectSound object: */
    if ( (dsResult = dsDirectSoundCreate(NULL, &ds, NULL)) != DS_OK )
    {
        DEBUG_CODE(puts("dsDetect: Not detected: DirectSoundCreate failed"));
        *result = 0;
        return OK;
    }

    /* Get DirectSound capabilities: */
    caps.dwSize = sizeof(DSCAPS);
    dsResult = ds->lpVtbl->GetCaps(ds, &caps);

    /* If capability get failed, or DirectSound uses emulation, don't
       detect: */
    if ( (dsResult != DS_OK) || (caps.dwFlags & DSCAPS_EMULDRIVER) )
    {
        DEBUG_CODE(puts("dsDetect: Not detected: GetCaps failed or "
            "emulation mode"));
        *result = 0;
        ds->lpVtbl->Release(ds);
        return OK;
    }

    /* Great, emulation not used, we'll use DirectSound: */
    *result = 1;
    ds->lpVtbl->Release(ds);

    DEBUG_CODE(puts("dsDetect: Detected: DirectSound present, no emulation"));

    return OK;
}




/****************************************************************************\
*
* Function:     int dsInitDevice(void)
*
* Description:  Initializes the DirectSound output device and creates output
*               buffer.
*
* Returns:      MIDAS error code
*
\****************************************************************************/

//Dee: these flags are added to all dsound objects
#define DSBCAPS_RAINFLAGS DSBCAPS_STICKYFOCUS  //DSBCAPS_GLOBALFOCUS

int dsInitDevice(void)
{
    HRESULT     dsResult;
    DSBUFFERDESC bufferDesc;
    WAVEFORMATEX format;
    DSBCAPS     bufferCaps;
    unsigned    targetBufferLen;
    DSCAPS      dscaps;
#ifdef DEBUG
    int         error;
#endif

    /* Check that we have the DirectSound HWND: */
    if ( mDSoundHwnd == NULL )
    {
        ERROR(errDSoundNoHwnd, ID_dsInitDevice);
        return errDSoundNoHwnd;
    }

    hwnd = (HWND) mDSoundHwnd;
    emulation = primary = 0;

    /* If the DirectSound DLL hasn't been loaded yet, load it and get the
       address for DirectSoundCreate: */
    if ( (dsModule == NULL) || (dsDirectSoundCreate == NULL) )
    {
        /* Try to load the DirectSound DLL and get the address for
           DirectSoundCreate: */
        if ( (dsModule = LoadLibrary("dsound.dll")) == NULL )
        {
            DEBUG_CODE(puts("dsInitDevice: Couldn't load dsound.dll"));
            ERROR(errCouldntLoadDsound, ID_dsInitDevice);
            return errCouldntLoadDsound;
        }
        if ( (dsDirectSoundCreate = (HRESULT (WINAPI *)(const GUID *,
                                                        LPDIRECTSOUND *,
                                                        IUnknown FAR *))
              GetProcAddress(dsModule, "DirectSoundCreate")) == NULL )
        {
            DEBUG_CODE(puts("dsInitDevice: Couldn't get "
                            "DirectSoundCreate address"));
            ERROR(errCouldntLoadDsound, ID_dsInitDevice);
            return errCouldntLoadDsound;
        }

        DEBUG_CODE(puts("dsInitDevice: dsound.dll loaded"));
    }
    
    /* If the DirectSound object has been given, use it, in stream mode,
       otherwise create the DirectSound object and try using primary
       buffer mode: */
    if ( mDSoundObject != NULL )
    {
        DEBUG_CODE(puts("Using given DirectSound object"));
        ds = (LPDIRECTSOUND) mDSoundObject;
    }
    else
    {
        /* Create the DirectSound object: */
        DEBUG_CODE(puts("Creating DirectSound object"));
        if ( (dsResult = dsDirectSoundCreate(NULL, &ds, NULL)) != DS_OK )
            PASSDSERR(dsResult, ID_dsInitDevice)

        /* If we are using primary buffer mode, try to set cooperative
           level to write primary: */
        if ( mDSoundMode == M_DSOUND_PRIMARY )
        {
            if ( (dsResult = ds->lpVtbl->SetCooperativeLevel(ds, hwnd,
                DSSCL_WRITEPRIMARY)) == DS_OK )
            {
                /* Worked! We'll use primary buffer mode: */
                primary = 1;
            }
        }

        /* If we failed to get write primary cooperative level, or are
           simply forced to use stream mode, use exclusive cooperative
           level: */
        if ( !primary )
        {
            if ( (dsResult = ds->lpVtbl->SetCooperativeLevel(ds, hwnd,
                DSSCL_EXCLUSIVE)) != DS_OK )
                PASSDSERR(dsResult, ID_dsInitDevice)

            /* We'll need to create a primary buffer ourselves, as the
                default is 22kHz 8-bit. We'll use the same format as for
                the playback buffer. */

            DEBUG_CODE(puts("dsInit: Creating primary buffer in stream "
                "playback mode"));
            memset(&bufferDesc, 0, sizeof(DSBUFFERDESC));
            bufferDesc.dwSize = sizeof(DSBUFFERDESC);
            bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_RAINFLAGS;
            bufferDesc.dwBufferBytes = 0;
            bufferDesc.lpwfxFormat = NULL;

            if ( (dsResult = ds->lpVtbl->CreateSoundBuffer(ds,
                &bufferDesc, &buf, NULL)) != DS_OK )
                PASSDSERR(dsResult, ID_dsInitDevice)

            DEBUG_CODE(puts("dsInit: Setting stream playback primary "
                "buffer format"));

            /* Set the primary buffer format: */
            memset(&format, 0, sizeof(WAVEFORMATEX));
            format.wFormatTag = WAVE_FORMAT_PCM;
            if ( outputMode & sdStereo )
                format.nChannels = 2;
            else
                format.nChannels = 1;
            format.nSamplesPerSec = mixRate;
            format.nAvgBytesPerSec = mixElemSize * mixRate;
            format.nBlockAlign = mixElemSize;
            if ( outputMode & sd16bit )
                format.wBitsPerSample = 16;
            else
                format.wBitsPerSample = 8;
            format.cbSize = 0;

            if ( (dsResult = buf->lpVtbl->SetFormat(buf, &format)) != DS_OK )
                PASSDSERR(dsResult, ID_dsInitDevice)
        }
    }

    /* OK, now we have a valid DirectSound object. Check if we are using
       emulation: */
    dscaps.dwSize = sizeof(DSCAPS);
    if ( (dsResult = ds->lpVtbl->GetCaps(ds, &dscaps)) != DS_OK )
        PASSDSERR(dsResult, ID_dsInitDevice);
    if ( dscaps.dwFlags & DSCAPS_EMULDRIVER )
        emulation = 1;

    /* Figure out our preferred buffer length: (use mDSoundBufferLength
       only if not in emulation mode) */
    if ( emulation )
        targetBufferLen = mixRate * mixElemSize * mBufferLength / 1000;
    else
        targetBufferLen = mixRate * mixElemSize * mDSoundBufferLength / 1000;
    targetBufferLen = (targetBufferLen + 15) & (~15);

    writePos = 0;

    /* Now create the playback buffer - this will be the primary buffer if in
       primary buffer mode, a normal stream buffer otherwise: */
    if ( primary )
    {
        DEBUG_CODE(puts("dsInit: Creating primary buffer"));
        memset(&bufferDesc, 0, sizeof(DSBUFFERDESC));
        bufferDesc.dwSize = sizeof(DSBUFFERDESC);
        bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_RAINFLAGS;
        bufferDesc.dwBufferBytes = 0;
        bufferDesc.lpwfxFormat = NULL;

        if ( (dsResult = ds->lpVtbl->CreateSoundBuffer(ds, &bufferDesc,
            &buf, NULL)) != DS_OK )
            PASSDSERR(dsResult, ID_dsInitDevice)

        DEBUG_CODE(puts("dsInit: Setting primary buffer format"));

        /* Set its format: */
        memset(&format, 0, sizeof(WAVEFORMATEX));
        format.wFormatTag = WAVE_FORMAT_PCM;
        if ( outputMode & sdStereo )
            format.nChannels = 2;
        else
            format.nChannels = 1;
        format.nSamplesPerSec = mixRate;
        format.nAvgBytesPerSec = mixElemSize * mixRate;
        format.nBlockAlign = mixElemSize;
        if ( outputMode & sd16bit )
            format.wBitsPerSample = 16;
        else
            format.wBitsPerSample = 8;
        format.cbSize = 0;

        if ( (dsResult = buf->lpVtbl->SetFormat(buf, &format)) != DS_OK )
            PASSDSERR(dsResult, ID_dsInitDevice)

        DEBUG_CODE(puts("dsInit: Getting primary buffer capabilities"));
        /* Get buffer capabilities: */
        bufferCaps.dwSize = sizeof(DSBCAPS);
        if ( (dsResult = buf->lpVtbl->GetCaps(buf, &bufferCaps)) != DS_OK )
            PASSDSERR(dsResult, ID_dsInitDevice)
        bufferLen = bufferCaps.dwBufferBytes;

        DEBUG_CODE(printf("dsInit: Primary buffer size %u bytes\n",
            bufferLen));

        DEBUG_CODE(puts("dsInit: Playing primary buffer"));
        /* Play the primary buffer: */
        if ( (dsResult = buf->lpVtbl->Play(buf, 0, 0, DSBPLAY_LOOPING))
            != DS_OK )
            PASSDSERR(dsResult, ID_dsInitDevice)

        /* If the buffer is larger than our preferred buffer length, we'll
           need to leave a portion of it unused: */
        if ( bufferLen > (targetBufferLen+16) )
            bufferLeaveSpace = bufferLen - targetBufferLen;
        else
            bufferLeaveSpace = 16;
    }
    else
    {
        /* We'll use just a normal stream buffer: */

        bufferLen = targetBufferLen;
        bufferLeaveSpace = 16;

        DEBUG_CODE(puts("dsInit: Creating stream buffer"));

        /* Set up wave output format structure: */
        memset(&format, 0, sizeof(WAVEFORMATEX));
        format.wFormatTag = WAVE_FORMAT_PCM;
        if ( outputMode & sdStereo )
            format.nChannels = 2;
        else
            format.nChannels = 1;
        format.nSamplesPerSec = mixRate;
        format.nAvgBytesPerSec = mixElemSize * mixRate;
        format.nBlockAlign = mixElemSize;
        if ( outputMode & sd16bit )
            format.wBitsPerSample = 16;
        else
            format.wBitsPerSample = 8;
        format.cbSize = 0;

        /* Set up directsound buffer description structure: */
        memset(&bufferDesc, 0, sizeof(DSBUFFERDESC));
        bufferDesc.dwSize = sizeof(DSBUFFERDESC);
        bufferDesc.dwFlags = 0 | DSBCAPS_RAINFLAGS;
        bufferDesc.dwBufferBytes = bufferLen;
        bufferDesc.lpwfxFormat = &format;

        /* Create the buffer: */
        if ( (dsResult = ds->lpVtbl->CreateSoundBuffer(ds, &bufferDesc, &buf,
            NULL)) != DS_OK )
            PASSDSERR(dsResult, ID_dsInitDevice)

        DEBUG_CODE(puts("dsInit: Playing stream buffer"));

        /* Play it: */
        if ( (dsResult = buf->lpVtbl->Play(buf, 0, 0, DSBPLAY_LOOPING))
            != DS_OK )
            PASSDSERR(dsResult, ID_dsInitDevice)
    }

    return OK;
}




/****************************************************************************\
*
* Function:     int dsCloseDevice(void)
*
* Description:  Closes the DirectSound output device and deallocates output
*               buffer
*
* Returns:      MIDAS error code
*
\****************************************************************************/

int dsCloseDevice(void)
{
    HRESULT     dsResult;
#ifdef DEBUG
    int         error;
#endif

    /* Stop playing the buffer: */
    if ( (dsResult = buf->lpVtbl->Stop(buf)) != DS_OK )
        PASSDSERR(dsResult, ID_dsCloseDevice);

    /* Release it: */
    buf->lpVtbl->Release(buf);

    /* If the DirectSound object was allocated by us, release it: */
    if ( mDSoundObject == NULL )
        ds->lpVtbl->Release(ds);

    return OK;
}




/****************************************************************************\
*
* Function:     int dsInit(unsigned mixRate, unsigned mode)
*
* Description:  Initializes DirectSound Sound Device
*
* Input:        unsigned mixRate        mixing rate in Hz
*               unsigned mode           output mode
*
* Returns:      MIDAS error code
*
\****************************************************************************/

int CALLING dsInit(unsigned _mixRate, unsigned mode)
{
    int         error;

    mixRate = _mixRate;
    suspended = 0;

    /* Initialize our Device Used -critical section and enter it: */
    InitializeCriticalSection(&deviceUsed);
    EnterCriticalSection(&deviceUsed);

    DEBUG_CODE(printf("Dee: dsinit mode=%i\n",(int)mode));

    /* Determine the actual output and mixing mode and mixing element size: */
    if ( mode & sdMono )
    {
        outputMode = sdMono;
        mixMode = dsmMixMono;
        mixElemSize = 1;
    }
    else
    {
        outputMode = sdStereo;
        mixMode = dsmMixStereo;
        mixElemSize = 2;
    }
    if ( mode & sd8bit )
    {
        outputMode |= sd8bit;
        mixMode |= dsmMix8bit;
    }
    else
    {
        outputMode |= sd16bit;
        mixMode |= dsmMix16bit;
        mixElemSize = mixElemSize * 2;
    }

    if ( mOversampling )
        mixMode |= dsmMixInterpolation;

    /* Initialize output device: */
    if ( (error = dsInitDevice()) != OK )
    {
        LeaveCriticalSection(&deviceUsed);
        PASSERROR(ID_dsInit);
    }

    /* Initialize Digital Sound Mixer: */
    if ( (error = dsmInit(mixRate, mixMode)) != OK )
    {
        LeaveCriticalSection(&deviceUsed);
        PASSERROR(ID_dsInit);
    }

    /* Set update rate to 50Hz: */
    if ( (error = dsSetUpdRate(5000)) != OK )
    {
        LeaveCriticalSection(&deviceUsed);
        PASSERROR(ID_dsInit)
    }

    LeaveCriticalSection(&deviceUsed);

    DEBUG_CODE(printf("Dee: dsinit outputMode=%i\n",(int)outputMode));

    return OK;
}




/****************************************************************************\
*
* Function:     dsClose(void)
*
* Description:  Uninitializes DirectSound Sound Device
*
* Returns:      MIDAS error code
*
\****************************************************************************/

int CALLING dsClose(void)
{
    int         error;

    EnterCriticalSection(&deviceUsed);

    /* Uninitialize Digital Sound Mixer: */
    if ( (error = dsmClose()) != OK )
    {
        LeaveCriticalSection(&deviceUsed);
        PASSERROR(ID_dsClose)
    }

    /* Close the output device: */
    if ( !suspended )
    {
        if ( (error = dsCloseDevice()) != OK )
        {
            LeaveCriticalSection(&deviceUsed);
            PASSERROR(ID_dsClose);
        }
    }

    LeaveCriticalSection(&deviceUsed);
    DeleteCriticalSection(&deviceUsed);

    return OK;
}




/****************************************************************************\
*
* Function:     int dsSuspend(void)
*
* Description:  Suspends all sound playback and releases the output device
*
* Returns:      MIDAS error code
*
\****************************************************************************/

int CALLING dsSuspend(void)
{
    int         error;

    EnterCriticalSection(&deviceUsed);

    suspended = 1;

    if ( (error = dsCloseDevice()) != OK )
    {
        LeaveCriticalSection(&deviceUsed);
        PASSERROR(ID_dsSuspend)
    }

    LeaveCriticalSection(&deviceUsed);

    return OK;
}




/****************************************************************************\
*
* Function:     int dsResume(void)
*
* Description:  Resumes sound playback after winwSuspend()
*
* Returns:      MIDAS error code
*
\****************************************************************************/

int CALLING dsResume(void)
{
    int         error;

    EnterCriticalSection(&deviceUsed);

    if ( (error = dsInitDevice()) != OK )
    {
        LeaveCriticalSection(&deviceUsed);
        PASSERROR(ID_dsResume)
    }

    suspended = 0;

    LeaveCriticalSection(&deviceUsed);

    return OK;
}




/****************************************************************************\
*
* Function:     int dsGetMode(unsigned *mode)
*
* Description:  Reads the current output mode
*
* Input:        unsigned *mode          pointer to output mode
*
* Returns:      MIDAS error code. Output mode is written to *mode.
*
\****************************************************************************/

int CALLING dsGetMode(unsigned *mode)
{
    *mode = outputMode;

    return OK;
}




/****************************************************************************\
*
* Function:     int dsSetUpdRate(unsigned updRate);
*
* Description:  Sets the channel value update rate (depends on song tempo)
*
* Input:        unsigned updRate        update rate in 100*Hz (eg. 50Hz
*                                       becomes 5000).
*
* Returns:      MIDAS error code
*
\****************************************************************************/

int CALLING dsSetUpdRate(unsigned updRate)
{
    /* Calculate number of elements to mix between two updates: (even) */
    mixLeft = updateMix = ((unsigned) ((100L * (ulong) mixRate) /
        ((ulong) updRate)) + 1) & 0xFFFFFFFE;

    return OK;
}




/****************************************************************************\
*
* Function:     int dsStartPlay(void)
*
* Description:  Prepares for playing - we'll read the playing position here
*               to keep CPU usage about constant
*
* Returns:      MIDAS error code
*
\****************************************************************************/

int CALLING dsStartPlay(void)
{
    HRESULT     dsResult;
    DWORD       playCursor, writeCursor;
#ifdef DEBUG
    int         error;
#endif

    if ( suspended )
        return OK;

    EnterCriticalSection(&deviceUsed);

    /* Read the position and remember the result: */
    if ( (dsResult = buf->lpVtbl->GetCurrentPosition(buf, &playCursor,
        &writeCursor)) != DS_OK )
        PASSDSERR(dsResult, ID_dsStartPlay)

    DEBUG_CODE(printf("Dee: dsStartPlay: playCursor=%i, writeCursor=%i, result=%i\n",playCursor,writeCursor,(int)dsResult));

    readPos = playCursor;

    LeaveCriticalSection(&deviceUsed);

    return OK;
}




/****************************************************************************\
*
* Function:     int dsPlay(int *callMP);
*
* Description:  Plays the sound - mixes the correct amount of data with DSM
*               and copies it to the playback buffer.
*
* Input:        int *callMP             pointer to music player calling flag
*
* Returns:      MIDAS error code. If enough data was mixed for one updating
*               round and music player should be called, 1 is written to
*               *callMP, otherwise 0 is written there. Note that if music
*               player can be called, dsPlay() should be called again
*               with a new check for music playing to ensure the mixing buffer
*               gets filled with new data.
*
\****************************************************************************/

int CALLING dsPlay(int *callMP)
{
    int         error;
    HRESULT     dsResult;
    unsigned    numElems;
    unsigned    dsmBufSize;
    unsigned    spaceLeft;
    uchar       *p1, *p2;
    DWORD       l1, l2;
    static int  foreground;
    static DWORD noPlayStart;

    if ( suspended )
    {
        *callMP = 0;
        return OK;
    }

    EnterCriticalSection(&deviceUsed);

    /* Calculate DSM mixing buffer size in elements: (FIXME) */
    dsmBufSize = dsmMixBufferSize;
#ifdef __32__
    dsmBufSize >>= 2;
#else
    dsmBufSize >>= 1;
#endif
    if ( outputMode & sdStereo )
        dsmBufSize >>= 1;

    /* Calculate the amount of space left in the buffer: */
    if ( writePos <= readPos )
        spaceLeft = readPos - writePos;
    else
        spaceLeft = bufferLen - writePos + readPos;

    /* We might need to leave space to make sure we won't buffer too much
       sound: */
    if ( spaceLeft > bufferLeaveSpace )
    {
        DEBUG_CODE(printf("Dee: dsPlay: vetev-ok\n"));
        spaceLeft -= bufferLeaveSpace;
        noPlayStart = 0;
    }
    else
    {
        DEBUG_CODE(printf("Dee: dsPlay: vetev-hack\n"));
        spaceLeft = 0;

        /* Hackhack: */
        if ( GetForegroundWindow() == hwnd )
        {
            DEBUG_CODE(printf("Dee: dsPlay: vetev-hack-match\n"));
            if ( foreground )
            {
                DEBUG_CODE(printf("Dee: dsPlay: vetev-hack-match-fore\n"));
                if ( (noPlayStart == 0) || (GetTickCount() < noPlayStart) )
                {
                    noPlayStart = GetTickCount();
                }
                else
                {
                    if ( (GetTickCount() - noPlayStart) > 1000 )
                    {
                        DEBUG_CODE(puts("dsPlay: Window is on foreground, "
                            "and no data has been played for 1000ms."));
                        DEBUG_CODE(puts("dsPlay: Restore buffer and restart "
                            "playback"));

                        buf->lpVtbl->Restore(buf);
                        buf->lpVtbl->Play(buf, 0, 0, DSBPLAY_LOOPING);

                        noPlayStart = 0;
                    }
                }
            }
            else
            {
                DEBUG_CODE(printf("Dee: dsPlay: vetev-hack-match-back\n"));
                foreground = 1;
            }
        }
        else
        {
            DEBUG_CODE(printf("Dee: dsPlay: vetev-hack-diffwind\n"));
            foreground = 0;
        }
    }

    /* Convert to number of elements: */
    spaceLeft /= mixElemSize;

    /* Loop until we run out of space or face the next update: */
    while ( spaceLeft && mixLeft )
    {
        /* Check that we won't mix more data than there is to the next
           update: */
        if ( spaceLeft > mixLeft )
            numElems = mixLeft;
        else
            numElems = spaceLeft;

        mixLeft -= numElems;
        spaceLeft -= numElems;

        DEBUG_CODE(printf("Dee: dsPlay: locking buffer\n"));

        /* Lock the portion of the buffer we'll write to: */
        while ( (dsResult = buf->lpVtbl->Lock(buf, writePos,
            mixElemSize * numElems, (LPVOID) &p1, &l1, (LPVOID) &p2, &l2, 0))
            != DS_OK )
        {
            DEBUG_CODE(puts("dsPlay: Lock failed!!!!!!!!!!"));
        }

        /* Mix the data to the output buffer: */
        if ( l1 )
        {
            if ( (error = dsmMixData(l1 / mixElemSize, p1)) != OK )
                PASSERROR(ID_dsPlay);
        }
        if ( l2 )
        {
            if ( (error = dsmMixData(l2 / mixElemSize, p2)) != OK )
                PASSERROR(ID_dsPlay);
        }

        DEBUG_CODE(printf("Dee: dsPlay: mixed %i+%i bytes\n",l1,l2));

        writePos += l1 + l2;
        if ( writePos >= bufferLen )
            writePos -= bufferLen;

        /* Unlock the buffer block: */
        if ( (dsResult = buf->lpVtbl->Unlock(buf, (LPVOID) p1, l1,
            (LPVOID) p2, l2)) != DS_OK )
        {
            LeaveCriticalSection(&deviceUsed);
            PASSDSERR(dsResult, ID_dsPlay)
        }
    }

    /* Check if the music player should be called and exit: */
    if ( mixLeft == 0 )
    {
        mixLeft = updateMix;
        *callMP = 1;
    }
    else
    {
        *callMP = 0;
    }

    LeaveCriticalSection(&deviceUsed);

    DEBUG_CODE(printf("Dee: dsPlay ended ok\n"));

    return OK;
}






    /* WinWave Sound Device structure: */

SoundDevice     DSound = {
    0,                                  /* tempoPoll = 0 */
    sdUseMixRate | sdUseOutputMode | sdUseDSM,  /* configBits */
    0,                                  /* port */
    0,                                  /* IRQ */
    0,                                  /* DMA */
    1,                                  /* cardType */
    1,                                  /* numCardTypes */
    sdMono | sdStereo | sd8bit | sd16bit,       /* modes */

    "DirectSound Sound Device " DSVERSTR,       /* name */
    &dsCardName,                                /* cardNames */
    0,                                          /* numPortAddresses */
    NULL,                                       /* portAddresses */

    &dsDetect,
    &dsInit,
    &dsClose,
#ifdef M_SD_HAVE_SUSPEND
    &dsSuspend,
    &dsResume,
#endif
    &dsmGetMixRate,
    &dsGetMode,
    &dsmOpenChannels,
    &dsmCloseChannels,
    &dsmClearChannels,
    &dsmMute,
    &dsmPause,
    &dsmSetMasterVolume,
    &dsmGetMasterVolume,
    &dsmSetAmplification,
    &dsmGetAmplification,
    &dsmPlaySound,
    &dsmReleaseSound,
    &dsmStopSound,
    &dsmSetRate,
    &dsmGetRate,
    &dsmSetVolume,
    &dsmGetVolume,
    &dsmSetSample,
    &dsmGetSample,
    &dsmSetPosition,
    &dsmGetPosition,
    &dsmGetDirection,
    &dsmSetPanning,
    &dsmGetPanning,
    &dsmMuteChannel,
    &dsmAddSample,
    &dsmRemoveSample,
    &dsSetUpdRate,
    &dsStartPlay,
    &dsPlay
#ifdef SUPPORTSTREAMS
    ,
    &dsmStartStream,
    &dsmStopStream,
    &dsmSetLoopCallback,
    &dsmSetStreamWritePosition,
    &dsmPauseStream
#endif
};


/*
 * $Log: dsound.c,v $
 * Revision 1.14  1997/07/31 14:30:04  pekangas
 * Added support for disabling DirectSound from the setup
 *
 * Revision 1.13  1997/07/31 10:56:40  pekangas
 * Renamed from MIDAS Sound System to MIDAS Digital Audio System
 *
 * Revision 1.12  1997/06/11 14:37:36  pekangas
 * Major cleanup for new mixing routines
 *
 * Revision 1.11  1997/05/30 18:26:45  pekangas
 * Fixed to work with new mixing routines
 *
 * Revision 1.10  1997/05/03 17:58:42  pekangas
 * Removed debug messages
 *
 * Revision 1.9  1997/05/03 17:54:46  pekangas
 * Now loads dsound.dll at run time if necessary - the DirectSound import
 * library is no longer needed
 *
 * Revision 1.8  1997/04/07 21:07:50  pekangas
 * Added the ability to pause/resume streams.
 * Added functions to query the number of stream bytes buffered
 *
 * Revision 1.7  1997/02/19 21:43:45  pekangas
 * Fixed retail build warnings
 *
 * Revision 1.6  1997/02/19 20:43:14  pekangas
 * Added Suspend() and Resume() -functions to Win32 sound devices
 *
 * Revision 1.5  1997/02/10 19:55:05  pekangas
 * Fixed to compile in debug mode
 *
 * Revision 1.4  1997/02/10 19:51:19  pekangas
 * Fixed removing debug messages (oops)
 *
 * Revision 1.3  1997/02/10 19:42:56  pekangas
 * Removed debug messages
 *
 * Revision 1.2  1997/02/08 18:25:53  pekangas
 * Fixed a Visual C warning
 *
 * Revision 1.1  1997/02/06 21:08:58  pekangas
 * Initial revision
 *
*/
