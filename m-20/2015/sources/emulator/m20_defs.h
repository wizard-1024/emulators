/*
 * File:     m20_defs.h
 * Purpose:  M-20 simulator definitions
 *
 * Copyright (c) 2009, Serge Vakulenko
 * Copyright (c) 2014, Dmitry Stefankov
 *
 * $Id$
 *
 * Revision History.
 *
 *  03-Nov-2014  DVS  Initial Implemementation
 *  04-Nov-2014  DVS  Added S.Vakulenko definitions
 *  20-Nov-2014  DVS  Updated
 *  12-Jan-2015  DVS  Minor update
 *  16-Jan-2015  DVS  Updated tape and drum definitions
 *
 */

#ifndef _M20_DEFS_H_
#define _M20_DEFS_H_    0

#include "sim_defs.h"				/* simulator definitions */

#if !defined(USE_INT64) 
#error "M-20 wanted 64b values because M-20 have 45b values!"
#endif



/* Memory and devices */

#define MAX_MEM_SIZE      4096        /* max memory size per M-20 (12-bit address) */
#define DRUM_SIZE	  4096	      /* magnetic drum size (040000) */
#define MAX_TAPE_SIZE     75000       /* magnetic tape size = N codes */

#define MIN_TAPE_ZONE_SIZE         1
#define MAX_TAPE_ZONE_SIZE      4095  /* NOT 4096 */

#define MIN_TAPE_ZONE_NUM          0  /* 1 */
#define MAX_TAPE_ZONE_NUM        255  /* 256 */

#define TAPE_READ_MODE         1
#define TAPE_WRITE_MODE        2
#define TAPE_FORMAT_MODE       4

#define MIN_TAPE_NUM           0
#define MAX_TAPE_NUM           3           /* physical numbers = 00, 01,10,11 */

#define MAX_TAPES_COUNT        4


#define MIN_PHYS_DRUM_NUM      1           
#define MAX_PHYS_DRUM_NUM      3           /* physical numbers = 01,10,11, logical=0,1,2,3 */
#define MAX_PHYS_DRUM_COUNT    3
#define MAX_LOG_DRUM_COUNT     4

#define DRUM_READ_MODE         1
#define DRUM_WRITE_MODE        2

#define MAX_ADDR_VALUE    07777       /* length = 12 bits */
#define MAX_OPCODE_VALUE    077       /* bits 42-37 = length 6 bits) */
#define MAX_ADDR_TAG_VALUE   07       /* bits 45,44,43 = length 3 bits */

#define M20_MANTISSA_SHIFT   64       /* decimal */


#define MSU_DRUM_PRINT_BUF_SIZE    512                  /* print ring buffer size */

#define CDR_BUF_SIZE    512                             /* card rdr buffer */
#define CDR_WIDTH       80                              /* card rdr width */
#define CDP_BUF_SIZE    101                             /* card punch buffer */
#define CDP_WIDTH       80                              /* card punch width */

#define LPT_BUF         201                             /* line print buffer */
#define LPT_WIDTH       132                             /* line print width */

#define BIT63		0x8000000000000000ULL	        /* 63-й бит */
#define BIT62		0x4000000000000000ULL	        /* 62-й бит */
#define BIT61		0x2000000000000000ULL	        /* 61-й бит */
#define BIT60		0x1000000000000000ULL	        /* 60-й бит */

#define ADDRESS_CODE_MARKER_SIGN  BIT63
#define COMMON_CODE_MARKER_SIGN   BIT62
#define CHECKSUM_MARKER_SIGN      BIT61
#define END_MARKER_SIGN           BIT60

#define PRINT_TYPE_DECIMAL    1
#define PRINT_TYPE_OCTAL      2
#define PRINT_TYPE_TEXT       3

#define OP_ROUND_BIT          4
#define OP_NORM_BIT           5

#define M20_AUTO_MODE         0
#define M20_CYCLIC_MODE       1
#define M20_ONE_IMPULSE_MODE  2

#define MOSU_MODE_I           1
#define MOSU_MODE_II          2


/*
 * Разряды машинного слова.
 */
#define BIT46		01000000000000000LL	/* 46-й разряды */
#define TAG		00400000000000000LL	/* 45-й разряды-признак */
#define SIGN		00200000000000000LL	/* 44-й разряды-знак */
#define BIT38		00002000000000000LL	/* 38-й разряд */
#define BIT37		00001000000000000LL	/* 37-й разряд */
#define BIT36		00000400000000000LL	/* 36-й разряд */
#define BIT19		00000000001000000LL	/* 19-й разряд */
#define BIT01		00000000000000001LL	/* 1-й разряд */
#define WORD44		00377777777777777LL	/* разряды 44..1 */
#define WORD45		00777777777777777LL	/* разряды 45..1 */
#define MANTISSA	00000777777777777LL	/* разряды 36..1 */
#define EXPONENT        00177000000000000LL     /* разряды 43..37 */
#define EXPONENT_SIGN   00100000000000000LL     /* разряды 43 */
#define EXP_SIGN_TAG    00777000000000000LL     /* разряды 45..37 */


/* bits */
#define BITS_0           0
#define BITS_12         12
#define BITS_18         18
#define BITS_24         24
#define BITS_32         32
#define BITS_36         36
#define BITS_42         42
#define BITS_42         42
#define BITS_44         44


/* printer */
#define   MAX_PRINTER_SYMBOLS_NUM   128

/* instructions */
#define  M20_SYM_OPCODE_TABLE_SIZE  64


/* CPU options, stored in cpu_unit.flags */

#define SHORT_SYM_OP          (1 << (UNIT_V_UF + 0))          /* short symbolic instruction name */


/*
 * Разряды условного числа для обращения к внешнему устройству.
 */
#define EXT_DIS_RAM	04000	/* 36 - БМ - блокировка памяти */
#define EXT_DIS_CHECK	02000   /* 35 - БК - блокировка контроля */
#define EXT_TAPE_REV	01000   /* 34 - ОН - обратное движение ленты */
#define EXT_DIS_STOP	00400   /* 33 - БО - блокировка останова */
#define EXT_PUNCH	00200   /* 32 - Пф - перфорация */
#define EXT_PRINT	00100   /* 31 - Пч - печать */
#define EXT_TAPE_FORMAT	00040   /* 30 - РЛ - разметка ленты */
#define EXT_TAPE	00020   /* 29 - Л - лента */
#define EXT_DRUM	00010   /* 28 - Б - барабан */
#define EXT_WRITE	00004   /* 27 - Зп - запись */
#define EXT_UNIT	00003   /* 26..25 - номер барабана или ленты */


/* Operation codes (opcodes */
#define     OPCODE_TRANSFER_MEM2MEM                           000
#define     OPCODE_ADD_ROUND_NORM                             001
#define     OPCODE_SUB_ROUND_NORM                             002
#define     OPCODE_SUB_MOD_ROUND_NORM                         003
#define     OPCODE_DIV_ROUND_NORM                             004
#define     OPCODE_MULT_ROUND_NORM                            005
#define     OPCODE_ADD_ADDR_TO_EXP                            006
#define     OPCODE_ADD_CYCLIC                                 007

#define     OPCODE_INPUT_CODES_FROM_PUNCH_CARDS_WITH_STOP     010
#define     OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_1_011         011
#define     OPCODE_GOTO_AFTER_CYCLE_BY_PA_012                 012
#define     OPCODE_ADD_CMDS                                   013
#define     OPCODE_SHIFT_MANTISSA_BY_ADDR                     014
#define     OPCODE_COMPARE                                    015
#define     OPCODE_JUMP_WITH_RETURN                           016
#define     OPCODE_STOP_017                                   017

#define     OPCODE_LOAD_FROM_KEY_REGISTER                     020
#define     OPCODE_ADD_NORM                                   021
#define     OPCODE_SUB_NORM                                   022
#define     OPCODE_SUB_MOD_NORM                               023
#define     OPCODE_DIV_NORM                                   024
#define     OPCODE_MULT_NORM                                  025
#define     OPCODE_ADD_EXP_TO_EXP                             026
#define     OPCODE_SUB_CYCLIC                                 027

#define     OPCODE_INPUT_CODES_FROM_PUNCH_CARDS               030
#define     OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_1_031         031
#define     OPCODE_GOTO_AFTER_CYCLE_BY_PA_032                 032
#define     OPCODE_SUB_CMDS                                   033
#define     OPCODE_SHIFT_MANTISSA_BY_EXP                      034
#define     OPCODE_COMPARE_WITH_STOP                          035
#define     OPCODE_COND_JUMP_BY_SIG_W_1                       036
#define     OPCODE_STOP_037                                   037

#define     OPCODE_BLANKING_040                               040
#define     OPCODE_ADD_ROUND                                  041
#define     OPCODE_SUB_ROUND                                  042
#define     OPCODE_SUB_MOD_ROUND                              043
#define     OPCODE_SQRT_ROUND_NORM                            044
#define     OPCODE_MULT_ROUND                                 045
#define     OPCODE_SUB_ADDR_FROM_EXP                          046
#define     OPCODE_OUT_LOWER_BITS_OF_MULT                     047

#define     OPCODE_IO_EXT_DEV_TO_MEM_050                      050
#define     OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_0_051         051
#define     OPCODE_CHANGE_RA_BY_ADDR                          052
#define     OPCODE_ADD_OPCS                                   053
#define     OPCODE_SHIFT_CODE_BY_ADDR                         054
#define     OPCODE_LOGICAL_MULT                               055
#define     OPCODE_JUMP_BY_ADDR                               056
#define     OPCODE_STOP_057                                   057

#define     OPCODE_BLANKING_060                               060
#define     OPCODE_ADD                                        061
#define     OPCODE_SUB                                        062
#define     OPCODE_SUB_MOD                                    063
#define     OPCODE_SQRT_NORM                                  064
#define     OPCODE_MULT                                       065
#define     OPCODE_SUB_EXP_FROM_EXP                           066
#define     OPCODE_SHIFT_CYCLIC                               067

#define     OPCODE_IO_EXT_DEV_TO_MEM_070                      070
#define     OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_0_071         071
#define     OPCODE_CHANGE_RA_BY_CODE                          072
#define     OPCODE_SUB_OPCS                                   073
#define     OPCODE_SHIFT_CODE_BY_EXP                          074
#define     OPCODE_LOGICAL_ADD                                075
#define     OPCODE_COND_JUMP_BY_SIG_W_0                       076
#define     OPCODE_STOP_077                                   077


/* Simulator stop codes */

enum {
	STOP_STOP = 1,				/* STOP */
	STOP_IBKPT,				/* breakpoint */
	STOP_RUNOUT,				/* run out end of memory limits */
	STOP_BADCMD,				/* invalid instruction */
	STOP_ADDOVF,				/* addition overflow */
	STOP_EXPOVF,				/* exponent overflow */
	STOP_MULOVF,				/* multiplication overflow */
	STOP_DIVOVF,				/* division overflow */
	STOP_DIVMOVF,				/* division mantissa overflow */
	STOP_DIVZERO,				/* division by zero */
	STOP_NEGSQRT,				/* negative number arg for sqrt operation */
	STOP_SQRTERR,				/* sqrt error */
	STOP_READERR,				/* drum read error */
	STOP_BADRLEN,				/* invalid drum read length */
	STOP_BADWLEN,				/* invalid drum write length */
	STOP_WRERR,				/* drum write error error */
	STOP_DRUMINVAL,				/* invalid drum control word */
	STOP_DRUMINVDATA,			/* reading uninialized drum data */
	STOP_TAPEINVAL,				/* invalid tape control word */
	STOP_TAPEFMTINVAL,			/* invalid tape format word */
	STOP_TAPEUNSUPP,			/* tape not implemented */
	STOP_TAPEFMTUNSUPP,			/* tape formatting not implemented */
	STOP_PUNCHUNSUPP,			/* punch not implemented */
	STOP_RPUNCHUNSUPP,			/* punch reader not implemented */
	STOP_EXTINVAL,				/* invalid control word */
	STOP_INVARG,				/* invalid argument of instruction */
	STOP_ASSERT,				/* assertion failed */
	STOP_MBINVAL,				/* MB command without MA */
	STOP_EXTDEVIOUNSUPP,			/* external devices i/o not implemented */
        STOP_NOCD,                              /* no cards left */
        STOP_CRINVAL,                           /* CR instruction without CA */
	STOP_CRFMTINVAL,	                /* invalid card reader format word */
        STOP_CRBADSUM,                          /* card reader mismatch cyclic sum */
        STOP_IO_MISSING_SETUP,                  /* i/o setup was missing */
	STOP_DRUMUNSUPP,			/* drum not implemented */
	STOP_PRINTUNSUPP,			/* print not implemented */
        STOP_PUNCHBADBITS,                      /* output to punch has wrong control word */
        STOP_PRINTBADBITS,                      /* output to printer has wrong control word */
        STOP_TAPEBADBITS,                       /* tape i/o has wrong control word */
        STOP_DRUMBADBITS,                       /* drum i/o has wrong control word */
        STOP_TAPEFMTBADBITS,                    /* tape format has wrong control word */
	STOP_TAPEBADRLEN,			/* invalid tepe read length */
	STOP_TAPEBADWLEN,			/* invalid tape write length */
	STOP_TAPEBADFLEN,			/* invalid tape format length */
	STOP_TAPEINVZONE,                       /* invalid tape zone number */
        STOP_NOTAPE,                            /* end of tape detected */
        STOP_TAPEINVDATA,                       /* reading uninialized tape data */
        STOP_NOTAPEZONE,                        /* matching tape zone not found */
        STOP_TAPELARGEDATA,                     /* too large data for tape zone */
        STOP_TAPEUSERSMALLBUF,                  /* too small user buffer for data from tape zone */
	STOP_TAPEREADERR,		        /* tape read error */
	STOP_WRITE_TO_RO_MEM_LOC,               /* writing attempt to read-only memory location */
	STOP_NOT_READY_PUNCH,                   /* not ready punch */
	STOP_NOT_READY_PRINT,                   /* not ready print */
	STOP_CROUTMEMORY,			/* card read out of memory */
	STOP_MEMORY_GARBAGE_DETECTED,           /* detected memory garbase per memory location */
	STOP_DRUM_NOT_IN_WRITE_MODE,		/* drum not in write mode */
	STOP_DRUM_NOT_IN_READ_MODE,		/* drum not in read mode */
	STOP_DRUM_MAP_ERROR,		        /* one more logical drums mapped to one physical drum */
	STOP_TAPE_NOT_IN_FORMAT_MODE,		/* tape not in format mode */
	STOP_TAPE_NOT_IN_WRITE_MODE,		/* tape not in write mode */
	STOP_TAPE_NOT_IN_READ_MODE,		/* tape not in read mode */
	STOP_TAPE_MAP_ERROR,		        /* one more logical tapes mapped to one physical tape */
};


#if !defined(WIN32)
#define  _snprintf  snprintf
#endif

#endif
