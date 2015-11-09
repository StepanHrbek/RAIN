/*      win32cnf.c
 *
 * MIDAS Digital Audio System Win32 configuration functions
 *
 * $Id: win32cnf.c,v 1.4 1997/07/31 14:28:47 pekangas Exp $
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
#include <stdlib.h>
#include "midas.h"

/* Klugekluge: */
#if defined(M_DLL)
#define EXPORT_IN_MIDASDLL_H
#endif

#include "midasdll.h"
#include "win32dlg.h"

RCSID(const char *win32cnf_rcsid = "$Id: win32cnf.c,v 1.4 1997/07/31 14:28:47 pekangas Exp $";);

extern unsigned char mConfDialog[];
extern unsigned char mConfCustomDialog[];

/* From midasdll.c: */
extern int      mLastError;


static char     *dialogTitle = "MIDAS Digital Audio System v" MVERSTR
                               " Setup";


#define NUMMIXRATES 8
static unsigned mixRates[NUMMIXRATES] =
{
    8000, 11025, 16000, 22050, 27429, 32000, 37800, 44100
};

typedef struct
{
    unsigned button, mixRate, interpolation;
} Quality;

#define NUMQUALITIES 5
Quality qualities[NUMQUALITIES] =
{
    { IDC_VERYLOWQUALITY, 22050, 0 },
    { IDC_LOWQUALITY, 32000, 0 },
    { IDC_MEDIUMQUALITY, 44100, 0 },
    { IDC_HIGHQUALITY, 32000, 1 },
    { IDC_VERYHIGHQUALITY, 44100, 1 }
};


static unsigned mixRate;
static unsigned interpolation;



static BOOL CALLBACK CustomDialogProc(HWND hwnd, UINT msg, WPARAM wparam,
                                      LPARAM lparam)
{
    int i;
    unsigned n;
    char str[32];
    
    lparam = lparam;
    
    switch ( msg )
    {
        case WM_INITDIALOG:
            if ( interpolation )
                SendDlgItemMessage(hwnd, IDC_MIXINTERPOLATING, BM_SETCHECK,
                                   1, 0);
            else
                SendDlgItemMessage(hwnd, IDC_MIXNORMAL, BM_SETCHECK, 1, 0);

            i = -1;
            for ( n = 0; n < NUMMIXRATES; n++ )
            {
                itoa(mixRates[n], str, 10);
                SendDlgItemMessage(hwnd, IDC_MIXRATECOMBO, CB_ADDSTRING, 0,
                                   (LPARAM) str);
                if ( mixRates[n] == mixRate )
                    i = (int) n;
            }
            
            if ( i == -1 )
            {
                itoa(mixRate, str, 10);
                SendDlgItemMessage(hwnd, IDC_MIXRATECOMBO, WM_SETTEXT, 0,
                                   (LPARAM) str);
            }
            else
            {
                SendDlgItemMessage(hwnd, IDC_MIXRATECOMBO, CB_SETCURSEL,
                                   (WPARAM) i, 0);
            }
                      
            return TRUE;

            
        case WM_COMMAND:
            switch ( LOWORD(wparam) )
            {
                case IDOK:
                    interpolation = SendDlgItemMessage(hwnd,
                                                       IDC_MIXINTERPOLATING,
                                                       BM_GETCHECK, 0, 0);
                    SendDlgItemMessage(hwnd, IDC_MIXRATECOMBO, WM_GETTEXT,
                                       31, (LPARAM) str);
                    if ( atoi(str) != 0 )
                        mixRate = atoi(str);
                        
                    EndDialog(hwnd, 1);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hwnd, 0);
                    return TRUE;
            }
    }

    return FALSE;
}




static BOOL CALLBACK ConfDialogProc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam)
{
    char str[32];
    unsigned n;
    unsigned stereo;

    lparam = lparam;
    
    switch ( msg )
    {
        case WM_INITDIALOG:
            SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM) dialogTitle);
            SendDlgItemMessage(hwnd, IDC_CUSTOMQUALITY, BM_SETCHECK, 1, 0);

            SendDlgItemMessage(hwnd, IDC_SOUNDCARD, CB_ADDSTRING, 0,
                               (LPARAM) "No Sound");            
            SendDlgItemMessage(hwnd, IDC_SOUNDCARD, CB_ADDSTRING, 0,
                               (LPARAM) "Default");
            SendDlgItemMessage(hwnd, IDC_SOUNDCARD, CB_SETCURSEL, 1, 0);
            
            if ( (midasOutputMode == 0) ||
                 (midasOutputMode & MIDAS_MODE_STEREO) )
                SendDlgItemMessage(hwnd, IDC_STEREO, BM_SETCHECK, 1, 0);
            mixRate = midasMixRate;
            if ( mOversampling )
                interpolation = 1;
            else
                interpolation = 0;
            for ( n = 0; n < NUMQUALITIES; n++ )
            {
                if ( (qualities[n].mixRate == mixRate) &&
                     (qualities[n].interpolation == interpolation) )
                {
                    SendDlgItemMessage(hwnd, qualities[n].button,
                                       BM_SETCHECK, 1, 0);
                    SendDlgItemMessage(hwnd, IDC_CUSTOMQUALITY,
                                       BM_SETCHECK, 0, 0);
                    break;
                }
            }
            
            itoa(mBufferLength, str, 10);
            SendDlgItemMessage(hwnd, IDC_BUFFEREDIT, WM_SETTEXT, 0,
                               (LPARAM) str);
            itoa(mDSoundBufferLength, str, 10);
            SendDlgItemMessage(hwnd, IDC_DSBUFFEREDIT, WM_SETTEXT, 0,
                               (LPARAM) str);

            SendDlgItemMessage(hwnd, IDC_NEVERDSOUND, BM_SETCHECK,
                               (WPARAM) mDisableDSound, 0);
            
            return TRUE;

        case WM_USER+0x1742:
            if ( DialogBoxIndirect(GetModuleHandle(NULL), (LPDLGTEMPLATE)
                                   mConfCustomDialog, hwnd,
                                   CustomDialogProc) == -1 )
            {
                mLastError = errUndefined;
                EndDialog(hwnd, 0);
            }
            return TRUE;

        case WM_COMMAND:
            switch ( LOWORD(wparam) )
            {
                case IDOK:
                    midasMixRate = mixRate;
                    mOversampling = interpolation;
                    stereo = SendDlgItemMessage(hwnd, IDC_STEREO,
                                                BM_GETCHECK, 0, 0);
                    if ( stereo )
                        midasOutputMode = MIDAS_MODE_STEREO;//Dee:was 16BIT_STEREO
                    else
                        midasOutputMode = MIDAS_MODE_MONO;//Dee:was 16BIT_MONO
                    
                    SendDlgItemMessage(hwnd, IDC_BUFFEREDIT, WM_GETTEXT,
                                       31, (LPARAM) str);
                    if ( atoi(str) != 0 )
                        mBufferLength = atoi(str);
                    
                    SendDlgItemMessage(hwnd, IDC_DSBUFFEREDIT, WM_GETTEXT,
                                       31, (LPARAM) str);
                    if ( atoi(str) != 0 )
                        mDSoundBufferLength = atoi(str);

                    if ( SendDlgItemMessage(hwnd, IDC_SOUNDCARD,
                                            CB_GETCURSEL, 0, 0) == 0 )
                    {
#ifdef NODSOUND
                        midasSDNumber = 1;
#else
                        midasSDNumber = 2;
#endif
                    }
                    else
                    {
                        midasSDNumber = -1;
                    }

                    mDisableDSound = (int) SendDlgItemMessage(hwnd,
                        IDC_NEVERDSOUND, BM_GETCHECK, 0, 0);                    
                    
                    EndDialog(hwnd, 1);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hwnd, 0);
                    return TRUE;

                case IDC_VERYLOWQUALITY:
                case IDC_LOWQUALITY:
                case IDC_MEDIUMQUALITY:
                case IDC_HIGHQUALITY:
                case IDC_VERYHIGHQUALITY:
                    if ( HIWORD(wparam) == BN_CLICKED )
                    {
                        for ( n = 0; n < NUMQUALITIES; n++ )
                        {
                            if ( qualities[n].button == LOWORD(wparam) )
                            {
                                mixRate = qualities[n].mixRate;
                                interpolation = qualities[n].interpolation;
                                break;
                            }
                        }
                    }
                    return TRUE;

                case IDC_CUSTOMQUALITY:                    
                    if ( HIWORD(wparam) == BN_CLICKED )
                    {
                        if ( SendDlgItemMessage(hwnd, IDC_CUSTOMQUALITY,
                                                BM_GETCHECK, 0, 0) )
                        {
                            if ( !PostMessage(hwnd, WM_USER+0x1742, 17, 42) )
                            {
                                mLastError = errUndefined;
                                EndDialog(hwnd, 0);
                            }
                        }
                    }
                    return TRUE;
            }
    }

    return FALSE;
}



/****************************************************************************\
*
* Function:     BOOL  MIDASconfig(void)
*
* Description:  MIDAS Digital Audio System configuration. Prompts the user for all
*               configuration information and sets the MIDAS variables
*               accordingly. Call before midasInit() but after
*               midasSetDefaults().
*
* Returns:      TRUE if configuration was successful, FALSE if not
*               (an error occured or Escape was pressed)
*
\****************************************************************************/

_FUNC(BOOL) MIDASconfig(void)
{
    int result;

    mLastError = OK;
    
    if ( (result = DialogBoxIndirect(GetModuleHandle(NULL),
                                     (LPDLGTEMPLATE) mConfDialog,
                                     NULL, ConfDialogProc)) == -1 )
    {
/*        printf("DialogBoxIndirect failed: GetLastError(): %u\n",
               GetLastError());*/
        mLastError = errUndefined;
        return FALSE;
    }

    if ( result )
        return TRUE;
    return FALSE;
}




#define CALLMIDAS(x) \
    if ( (error = x) != OK ) \
    { \
        mLastError = error; \
        return FALSE; \
    } 

/****************************************************************************\
*
* Function:     BOOL MIDASloadConfig(char *fileName);
*
* Description:  Loads configuration from file saved using midasSaveConfig().
*
* Input:        char *fileName          configuration file name, ASCIIZ
*
* Returns:      TRUE if successful, FALSE if not (file not found or some other
*               error)
*
\****************************************************************************/

_FUNC(BOOL) MIDASloadConfig(char *fileName)
{
    static fileHandle  f;
    int         error;

    /* open configuration file: */
    CALLMIDAS(fileOpen(fileName, fileOpenRead, &f));

    /* read mixing rate: */
    CALLMIDAS(fileRead(f, &midasMixRate, sizeof(unsigned)));

    /* read output mode: */
    CALLMIDAS(fileRead(f, &midasOutputMode, sizeof(unsigned)));

    /* read interpolation info: */
    CALLMIDAS(fileRead(f, &mOversampling, sizeof(int)));

    /* read buffer length: */
    CALLMIDAS(fileRead(f, &mBufferLength, sizeof(int)));

    /* read DirectSound buffer length: */
    CALLMIDAS(fileRead(f, &mDSoundBufferLength, sizeof(int)));

    /* read if DirectSound is disabled */
    /* and ignore possible end of file: */
    fileRead(f, &mDisableDSound, sizeof(int));

    /* close configuration file: */
    CALLMIDAS(fileClose(f));

    return TRUE;
}



/****************************************************************************\
*
* Function:     BOOL midasSaveConfig(char *fileName);
*
* Description:  Saves configuration to a file
*
* Input:        char *fileName          configuration file name, ASCIIZ
*
* Returns:      TRUE if successful, FALSE if not
*
\****************************************************************************/

_FUNC(BOOL) MIDASsaveConfig(char *fileName)
{
    static fileHandle  f;
    int         error;

    /* open configuration file: */
    CALLMIDAS(fileOpen(fileName, fileOpenWrite, &f));

    /* write mixing rate: */
    CALLMIDAS(fileWrite(f, &midasMixRate, sizeof(unsigned)));

    /* write output mode: */
    CALLMIDAS(fileWrite(f, &midasOutputMode, sizeof(unsigned)));

    /* write interpolation info: */
    CALLMIDAS(fileWrite(f, &mOversampling, sizeof(int)));

    /* write buffer length: */
    CALLMIDAS(fileWrite(f, &mBufferLength, sizeof(int)));

    /* write DirectSound buffer length: */
    CALLMIDAS(fileWrite(f, &mDSoundBufferLength, sizeof(int)));

    /* write if DirectSound is disabled: */
    CALLMIDAS(fileWrite(f, &mDisableDSound, sizeof(int)));

    /* close configuration file: */
    CALLMIDAS(fileClose(f));

    return TRUE;
}



static HKEY mKey;


static int OpenKey(HKEY hKey, char *name)
{
    if ( RegOpenKeyEx(hKey, name, 0, KEY_ALL_ACCESS, &mKey) !=
         ERROR_SUCCESS )
        return 0;
    return 1;
}


static int CreateKey(HKEY hKey, char *name)
{
    DWORD temp;
    
    if ( RegCreateKeyEx(hKey, name, 0, NULL, REG_OPTION_NON_VOLATILE,
                        KEY_ALL_ACCESS, NULL, &mKey, &temp) != ERROR_SUCCESS )
        return 0;
    return 1;
}



static int ReadValue(char *name, DWORD *value)
{
    DWORD len;
    DWORD type;

    len = sizeof(DWORD);

    if ( RegQueryValueEx(mKey, name, NULL, &type, (LPBYTE) value, &len)
         != ERROR_SUCCESS )
        return 0;
    if ( (type != REG_DWORD) || (len != sizeof(DWORD)) )
        return 0;

    return 1;
}


static int WriteValue(char *name, DWORD value)
{
    if ( RegSetValueEx(mKey, name, 0, REG_DWORD, (CONST BYTE*) &value,
                       sizeof(DWORD)) != ERROR_SUCCESS )
        return 0;
    return 1;
}


static int CloseKey(void)
{
    if ( RegCloseKey(mKey) != ERROR_SUCCESS )
        return 0;
    return 1;
}



/****************************************************************************\
*
* Function:     BOOL MIDASreadConfigRegistry(DWORD key, char *subKey)
*
* Description:  Reads MIDAS configuration from a registry key
*
* Input:        DWORD key               A currently open registry key
*                                       (eg. HKEY_CURRENT_USER)
*               char *subKey            Subkey name for the MIDAS values
*
* Returns:      TRUE if successful, FALSE if not. If this function fails,
*               configuration was probably not saved to the key given
*
\****************************************************************************/

_FUNC(BOOL) MIDASreadConfigRegistry(DWORD key, char *subKey)
{
    mLastError = errUndefined;

    if ( !OpenKey((HKEY) key, subKey) )
        return FALSE;

#define READREG(name, value) \
    if ( !ReadValue(name, (DWORD*) &value) )  return FALSE;

    READREG("MixingRate", midasMixRate);
    READREG("OutputMode", midasOutputMode);
    READREG("Interpolation", mOversampling);
    READREG("BufferLength", mBufferLength);
    READREG("DSoundBufferLength", mDSoundBufferLength);

    if ( !CloseKey() )
        return FALSE;

    mLastError = OK;
    return TRUE;
}



/****************************************************************************\
*
* Function:     BOOL MIDASwriteConfigRegistry(DWORD key, char *subKey)
*
* Description:  Writes MIDAS configuration to a registry key
*
* Input:        DWORD key               A currently open registry key
*                                       (eg. HKEY_CURRENT_USER)
*               char *subKey            Subkey name for the MIDAS values
*
* Returns:      TRUE if successful, FALSE if not.
*
\****************************************************************************/

_FUNC(BOOL) MIDASwriteConfigRegistry(DWORD key, char *subKey)
{
    mLastError = errUndefined;

    if ( !CreateKey((HKEY) key, subKey) )
        return FALSE;

#define WRITEREG(name, value) \
    if ( !WriteValue(name, (DWORD) value) )  return FALSE;

    WRITEREG("MixingRate", midasMixRate);
    WRITEREG("OutputMode", midasOutputMode);
    WRITEREG("Interpolation", mOversampling);
    WRITEREG("BufferLength", mBufferLength);
    WRITEREG("DSoundBufferLength", mDSoundBufferLength);

    if ( !CloseKey() )
        return FALSE;

    mLastError = OK;
    return TRUE;
}






/*
 * $Log: win32cnf.c,v $
 * Revision 1.4  1997/07/31 14:28:47  pekangas
 * Added no sound option to sound cards
 * Changed dialog layout and added DirectSound disable button
 *
 * Revision 1.3  1997/07/31 10:56:57  pekangas
 * Renamed from MIDAS Sound System to MIDAS Digital Audio System
 *
 * Revision 1.2  1997/07/15 12:18:55  pekangas
 * Fixed to compile and link with Visual C.
 * All API functions are again exported properly
 *
 * Revision 1.1  1997/07/08 19:15:53  pekangas
 * Initial revision
 *
*/
