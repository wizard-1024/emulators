/*
 * File:     m20_eng.c
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
 *  05-Dec-2014  DVS  Updated
 *
 */


#include "m20_defs.h"


const char *sim_stop_messages[] = {
	"Unknown error",
	"STOP",
	"Breakpoint",
	"breakpoint (memory access)",
	"breakpoint (instruction access)",
	"Invalid instruction",
	"Run out end of memory",
	"Addition overflow",
	"Exponent overflow",
	"Multiplication overflow",
	"Division overflow",
	"Division mantissa overflow",
        "Division by zero",
	"SQRT from negative number",
	"SQRT error",
	"Drum read error",
	"Invalid drum read length",
	"Invalid drum write length",
	"Drum write error",
	"Invalid drum control word",
	"Reading uninialized drum data",
	"Invalid tape control word",
	"Invalid tape format word",
	"Tape not implemented",
	"Tape formatting not implemented",
	"Punch not implemented",
	"Punch reader not implemented",
	"Invalid control word",
	"Invalid argument of instruction",
	"Assertion failed",
	"MB instruction without MA",
	"External devices i/o not implemented",
        "Card reader empty",
	"CR instruction without CA",
	"invalid card reader format word",
	"card reader mismatch cyclic sum",
        "i/o setup was missing",
        "drum not implemented",
        "print not implemented",
        "output to punch has wrong control word",
        "output to printer has wrong control word",
        "tape i/o has wrong control word",
        "drum i/o has wrong control word",
        "tape format has wrong control word",
	"Invalid tape read length",
	"Invalid tape write length",
	"Invalid tape format length",
        "Invalid tape zone number",
        "end of tape detected",
        "reading uninialized tape data",
        "matching tape zone not found",
        "too large data for tape zone",
        "too small user buffer for data from tape zone",
        "tape read error",
        "writing attempt to read-only memory location",
        "not ready punch",
        "not ready print",
        "card read out of memory",
        "detected memory garbase per memory location",
        "drum not in write mode",
        "drum not in read mode",
        "one more logical drums mapped to one physical drum",
	"tape not in format mode",
	"tape not in write mode",
	"tape not in read mode",
	"one more logical tapes mapped to one physical tape",
    };



char  reg_rk_name[]    = "RK";      /* register of command */
char  reg_rop_name[]   = "ROP";     /* register of operations */
char  reg_ra_name[]    = "RA";      /* register of address */
char  reg_kra_name[]   = "KRA";     /* command register of address */
char  reg_sma_name[]   = "SMA";     /* summator of address */
char  trg_sw_name[]    = "TSW";     /* trigger of signal w */
char  reg_rr_name[]    = "RR";      /* register of result */
char  reg_rpu1_name[]  = "RPU1";    /* register-1 of control panel */
char  reg_rpu2_name[]  = "RPU2";    /* register-1 of control panel */
char  reg_rpu3_name[]  = "RPU3";    /* register-1 of control panel */
char  reg_rpu4_name[]  = "RPU4";    /* register-1 of control panel */


char  reg_rk_desc[]    = "register of command";
char  reg_rop_desc[]   = "register of operations";
char  reg_ra_desc[]    = "register of address";
char  reg_kra_desc[]   = "command register of address";
char  reg_sma_desc[]   = "summator of address";
char  trg_sw_desc[]    = "trigger of signal w";
char  reg_rr_desc[]    = "register of result";
char  reg_rpu1_desc[]  = "register 1 of control panel";
char  reg_rpu2_desc[]  = "register 2 of control panel";
char  reg_rpu3_desc[]  = "register 3 of control panel";
char  reg_rpu4_desc[]  = "register 4 of control panel";




const char *m20_opname [M20_SYM_OPCODE_TABLE_SIZE] = {
	"transfer_m2m","add_rn","sub_rn","sub_mod_rn","div_r","mult_rn","add_adr2exp","add_cyc",
	"in_codes_stop","goto_cyc_pa_w1_011","goto_cyc_pa_012","add_cmds","shift_mant_by_adr","compare","jump_with_return","stop_017",
	"ld_key_reg","add_n","sub_n","sub_mod_n","div","mult_n","add_exp2exp","sub_cyc",
	"in_codes","goto_cyc_pa_w1_031","goto_cyc_pa_032","sub_cmds","shift_mant_by_exp","comp_stop","cond_jump_w1","stop_037",
	"blank_040","add_r","sub_r","sub_mod_r","sqrt_r","mult_r","sub_adr_from_exp","out_low_bits_of_mult",
	"io_ext_dev_050","goto_cyc_pa_w0_051","chg_ra_by_adr","add_ops","shift_code_by_adr","log_mult",	"jump_by_addr",	"stop_057",
	"blank_060","add","sub","sub_mod","sqrt","mult","sub_exp_from_exp","shift_cyc",
	"io_ext_dev_070","goto_cyc_pa_w0_071","chg_ra_by_code",	"sub_ops","shift_code_by_exp","log_add","cond_jump_w0",	"stop_077"
};


const char *m20_short_opname [M20_SYM_OPCODE_TABLE_SIZE] = {
	"xfr_m2m","add_rn","sub_rn","sub_mod_rn","div_r","mult_rn","add_adr2exp","add_cyc",
	"in_codes_stop","goto_cyc_pa_w1_011","goto_cyc_pa_012","add_cmds","shift_mant_by_adr","compare","jump_with_return","stop_017",
	"ld_key_reg","add_n","sub_n","sub_mod_n","div","mult_n","add_exp2exp","sub_cyc",
	"in_codes","goto_cyc_pa_w1_031","goto_cyc_pa_032","sub_cmds","shift_mant_by_exp","comp_stop","cond_jump_w1","stop_037",
	"blank_040","add_r","sub_r","sub_mod_r","sqrt_r","mult_r","sub_adr_from_exp","out_low_bits_of_mult",
	"io_ext_dev_050","goto_cyc_pa_w0_051","chg_ra_by_adr","add_ops","shift_code_by_adr","log_mult",	"jump_by_addr",	"stop_057",
	"blank_060","add","sub","sub_mod","sqrt","mult","sub_exp_from_exp","shift_cyc",
	"io_ext_dev_070","goto_cyc_pa_w0_071","chg_ra_by_code",	"sub_ops","shift_code_by_exp","log_add","cond_jump_w0",	"stop_077"
};


const unsigned char m20_to_cyr_win1251[MAX_PRINTER_SYMBOLS_NUM] = {
/* 000-007 */	'0',   '1',   '2',   '3',   '4',   '5',   '6',   '7',
/* 010-017 */	'8',   '9',   '+',   '-',   '/',   ',',   '.',   ' ',
/* 020-027 */	't',   '^',  '(',   ')',    'x',   '=',   ';',   '[',
/* 030-037 */	']',   '*',   '\'', '\'',   'n',  '<',   '>',   ':',
/* 040-047 */	'À',   'Á',   'Â',   'Ã',   'Ä',   'Å',   'Æ',   'Ç',
/* 050-057 */	'È',   'É',   'Ê',   'Ë',   'Ì',   'Í',   'Î',   'Ï',
/* 060-067 */	'Ð',   'Ñ',   'Ò',   'Ó',   'Ô',   'Õ',   'Ö',   '×',
/* 070-077 */	'Ø',   'Ù',   'Û',   'Ú',   'Ý',   'Þ',   'ß',   'D',
/* 100-107 */	'F',   'G',   'I',   'J',   'L',   'N',   'Q',   'R',
/* 110-117 */	'S',   'U',   'V',   'W',   'Z',     0,     0,   ' ',
/* 120-127 */	  0,     0,     0,     0,     0,     0,     0,     0,
/* 140-147 */	  0,     0,     0,     0,     0,     0,     0,     0,
/* 150-157 */	  0,     0,     0,     0,     0,     0,     0,     0,
/* 160-167 */	  0,     0,     0,     0,     0,     0,     0,     0,
/* 170-177 */	  0,     0,     0,     0,     0,     0,     0,     0,
    };
