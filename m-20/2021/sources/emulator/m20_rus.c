/*
 * File:     m20_rus.c
 * Purpose:  M-20 simulator interface
 *
 * Copyright (c) 2014, Dmitry Stefankov
 *
 * $Id$
 *
 * Revision History.
 *
 *  03-Nov-2014  DVS  Initial Implemementation
 *  12-Nov-2014  DVS  Updated
 *  14-Nov-2014  DVS  Updated
 *  17-Nov-2014  DVS  Updated
 *  21-Nov-2014  DVS  Added short form of symbolic instructions
 *                    Updated long form of symbolic instructions
 *  03-Dec-2014  DVS  Updated long form of symbolic instructions
 *  05-Dec-2014  DVS  Updated
 *  19-Jan-2015  DVS  Added KOI8-R encoding
 *  24-Feb-2015  DVS  Added UTF8 encoding
 *
 */


#include "m20_defs.h"

#if   defined(RUS_WIN_CP1251)
#include "m20_rus_win_cp1251.h"
#elif defined(RUS_DOS_CP866)
#include "m20_rus_dos_cp866.h"
#elif defined(RUS_UNIX_KOI8R)
#include "m20_rus_unix_koi8_r.h"
#elif defined(RUS_UTF8)
#include "m20_rus_utf8.h"
#else
#error Undefined russian encoding. Please select one.
#endif


