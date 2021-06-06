/*
 * File:     m20_sys.c
 * Purpose:  M-20 simulator interface
 *
 * Copyright (c) 2009, Serge Vakulenko
 * Copyright (c) 2014, Dmitry Stefankov
 *
 * $Id$
 *
 * This file implements three essential functions:
 *
 * sim_load()   - loading and dumping memory and CPU state
 *		  in a way, specific for M20 architecture
 * fprint_sym() - print a machune instruction using
 *  		  opcode mnemonic or in a digital format
 * parse_sym()	- scan a string and build an instruction
 *		  word from it
 *
 * Revision History.
 *
 *  03-Nov-2014  DVS  Initial Implemementation
 *  10-Nov-2014  DVS  Added card reader
 *  12-Nov-2014  DVS  Added punch card
 *  13-Nov-2014  DVS  Added line printer device
 *  14-Nov-2014  DVS  Added drum device
 *  17-Nov-2014  DVS  Cleanup and more debugging
 *  03-Dec-2014  DVS  Changed symbolic decoding
 *  04-Dec-2014  DVS  Added debug dump for regs and mem
 *  07-Dec-2014  DVS  Added decimal numbers input
 *  21-Dec-2014  DVS  Added opcode and modifiers for cpu trace output
 *  13-Mar-2015  DVS  Cleanup code
 *
 */


#include "m20_defs.h"
#include <math.h>


/* external references (CPU module) */

extern DEVICE cpu_dev;
extern DEVICE cdr_dev;
extern DEVICE cdp_dev;
extern DEVICE lpt_dev;
extern DEVICE drum_dev;
extern DEVICE mt_dev;

extern UNIT   cpu_unit;
extern REG    cpu_reg[];

extern uint16   regKRA;

extern int   diag_print;
extern int   debug_dump_regs;
extern int   debug_dump_mem;

extern t_value mosu_load (int addr);
extern void mosu_store (int addr, t_value val);


extern const char *m20_opname [M20_SYM_OPCODE_TABLE_SIZE];
extern const char *m20_short_opname [M20_SYM_OPCODE_TABLE_SIZE];

/*
 * SCP data structures and interface routines
 *
 * sim_name		simulator name string
 * sim_PC		pointer to saved PC register descriptor
 * sim_emax		maximum number of words for examine/deposit
 * sim_devices		array of pointers to simulated devices
 * sim_stop_messages	array of pointers to stop messages
 * sim_load		binary loader
 */

char sim_name[] = "M-20";

REG *sim_PC = &cpu_reg[0];

int32 sim_emax = 1;	/* max.words per machine command */

DEVICE *sim_devices[] = {
    &cpu_dev,
    &cdr_dev,
    &cdp_dev,
    &lpt_dev,
    &drum_dev,
    &mt_dev,
    NULL
    };



/*
 * Transform real number into M-20 format
 *
 * IEEE 754 number presentation (double):
 *	64   63———53  52————–1
 *	sign exponent mantissa
 * High (53th) bit of mantissa is not stored. It is ALWAYS equal to 1.
 *
 * Number presentation in M-20:
 *	44   43—--37  36————–1
 *      sign exponent mantissa
 */
t_value ieee_to_m20 ( double d )
{
    t_value word;
    int exponent;
    int sign;

    sign = d < 0;
    if (sign) d = -d;
    d = frexp (d, &exponent);
    /* 0.5 <= d < 1.0 */
    d = ldexp (d, BITS_36);
    //word = d;
    word = (t_value)d;
    if (d - word >= 0.5)
	word += 1;		/* Rounding. */
    if (exponent < MIN_EXPONENT_VALUE)
	exponent = MIN_EXPONENT_VALUE;	/* Nearest number to zero */
    if (exponent > MAX_EXPONENT_VALUE) {
	word = 0xfffffffffLL;
	exponent = MAX_EXPONENT_VALUE;	/* Max.number */
    }
    word |= ((t_value) (exponent + M20_MANTISSA_SHIFT)) << BITS_36;
    word |= (t_value) sign << BITS_43;	/* Sign. */

    return word;
}



/*
 *  Skip space or tab
 */
char *skip_spaces (char *p)
{
    while (*p == ' ' || *p == '\t') ++p;

    return p;
}



/*
 *  Skip all symbols exceipt space and tab
 */
char *skip_nonspaces (char *p)
{
    while (*p != ' ' && *p != '\t') ++p;

    return p;
}



/*
 *  Read line from input file
 */
t_stat read_m20_fmt_line ( FILE *input, int *type, t_value *val )
{
    char *p;
    int i;
    int d_sign, d_tag, d_exp, d_exp_sign, d_m, exp_val, d;
    t_value  tcode, m, e;
    char buf [1024];

next_line:
    memset( buf, sizeof(buf), 0 );
    if (fgets (buf, sizeof (buf)-1, input) == NULL) {
	*type = 0;
	return SCPE_OK;
    }

    p = skip_spaces (buf);
    if ((*p == '\n') || (*p == ';'))  goto next_line;
    if ((*p == '*')  || (*p == '\r')) goto next_line;

    if (diag_print) fprintf( stderr, "%s", buf );

    if (*p == ':') {
	/* Address of data place */
	*type = ':';
	*val = strtol (p+1, 0, 8);
	return SCPE_OK;
    }

    if (*p == '@') {
	/* Starting address */
	*type = '@';
	*val = strtol (p+1, 0, 8);
	return SCPE_OK;
    }

    if (*p == '=') {
	/* Real number */
	*type = '=';
	*val = ieee_to_m20 (strtod (p+1, NULL));
	return SCPE_OK;
    }


    if (*p == '+' || *p == '-') {
        /* binary-decimal-coded number */
        *type = '=';
        tcode = 0;
        /* detect tage */
        d_tag = 0;
        if ((*p != '+') && (*p != '-')) goto fmt_err;
        if (*p=='+') d_tag = 0;
        if (*p=='-') d_tag = 1;
        p++;
        /* detect number sign */
        d_sign = 0;
        if ((*p != '+') && (*p != '-')) goto fmt_err;
        if (*p=='+') d_sign = 0;
        if (*p=='-') d_sign = 1;
        p=skip_spaces(p+1);
        /* detect exponent sign and exponent sign */
        d_exp = atoi(p);
        d_exp_sign = 0;
        if (d_exp < 0) d_exp_sign = 1;
        exp_val = abs(d_exp);
        if (exp_val > 19) goto fmt_err;
        e = exp_val % 10;
        if (exp_val / 10) e |= 1 << 4;
        p = skip_nonspaces(p+1);
        p=skip_spaces(p+1);
        /* detect mantissa */
        m = 0;
        for( i=0; i<9; i++ ) {
	    if (*p < '0' || *p > '9') goto fmt_err;
            d_m = *p - '0';
            m |= ((t_value)d_m << (32-i*4));
            p=skip_spaces(p+1);
        }
        /* make a final code */
        tcode = m;
        tcode |= (e << BITS_36);
        if (d_tag) tcode |= TAG;
        if (d_sign) tcode |= SIGN;
        if (d_exp_sign) tcode |= EXPONENT_SIGN;
        *val = tcode;
	return SCPE_OK;
    }

    if (*p == '#') {
        /* binary-decimal-coded number (another form) */
        *type = '=';
        tcode = 0;
        p=skip_spaces(p+1);
        /* detect tag, sign, exponent sign */
        if (*p < '0' || *p > '7') goto fmt_err;
        d_tag = 0; d_sign = 0; d_exp_sign = 0;
        d = *p - '0';
        if (d & 4) d_tag = 1;
        if (d & 2) d_sign = 1;
        if (d & 1) d_exp_sign = 1;
        /* detect exponent */
        p=skip_spaces(p+1);
        d_exp = atoi(p);
        exp_val = abs(d_exp);
        if (exp_val > 19) goto fmt_err;
        e = exp_val % 10;
        if (exp_val / 10) e |= 1 << 4;
        p = skip_nonspaces(p+1);
        p=skip_spaces(p+1);
        /* detect mantissa */
        m = 0;
        for( i=0; i<9; i++ ) {
	    if (*p < '0' || *p > '9') goto fmt_err;
            d_m = *p - '0';
            m |= ((t_value)d_m << (32-i*4));
            p=skip_spaces(p+1);
        }
        /* make a final code */
        tcode = m;
        tcode |= (e << BITS_36);
        if (d_tag) tcode |= TAG;
        if (d_sign) tcode |= SIGN;
        if (d_exp_sign) tcode |= EXPONENT_SIGN;
        *val = tcode;
	return SCPE_OK;
    }


    /* Code (word) */
    if (*p < '0' || *p > '7') {
	/* wrong line of input file */
        goto fmt_err;
    }
    *type = '=';
    *val = *p - '0';
    for (i=0; i<14; ++i) {
	p = skip_spaces (p + 1);
	if (*p < '0' || *p > '7') {
	    /* too short word  */
	    goto fmt_err;
	}
	*val = *val << 3 | (*p - '0');
    }

    return SCPE_OK;

fmt_err:
    fprintf( stderr, "ERROR INPUT!\n" );
    return SCPE_FMT;
}   



/*
 *  Load memory contents from file
 */
t_stat m20_abs_load ( FILE *input )
{
   int addr, type;
   t_value word;
   t_stat err;

   addr = 1;
   regKRA = 1;

   for (;;) {
      err = read_m20_fmt_line( input, &type, &word );
      if (err) return err;


      switch (type) {
	case 0:			/* EOF */
		return SCPE_OK;
	case ':':		/* address */
		addr = (int)word;
		break;
	case '=':		/* word */
		mosu_store(addr,word);
		++addr;
		break;
	case '@':		/* start address */
		regKRA = (int)word;
		break;
	}

        if (addr >= MAX_MEM_SIZE) return SCPE_FMT;
    }

    return SCPE_OK;
}



/*
 *  Dump abs. memory to file.
 */
t_stat m20_abs_mem_dump ( FILE *of, char *fname )
{
   int i, last_addr = -1;
   t_value cmd;
   t_value mcode;

   fprintf (of, "; %s\n", fname);

   for (i=1; i<MAX_MEM_SIZE; ++i) {
      mcode = mosu_load(i);
      if (mcode == 0) continue; // wrong, must return 0!
      if (i != last_addr+1) fprintf (of, "\n:%04o\n", i);
      last_addr = i;
      cmd = mcode;
      fprintf (of, "%o %02o %04o %04o %04o\n",
                    (int) (cmd >> BITS_42) & 7,
                    (int) (cmd >> BITS_36) & MAX_OPCODE_VALUE,
                    (int) (cmd >> BITS_24) & MAX_ADDR_VALUE,
		    (int) (cmd >> BITS_12) & MAX_ADDR_VALUE,
		    (int) (cmd >> BITS_0)  & MAX_ADDR_VALUE);
   }

   return SCPE_OK;
}





/*
 *  Loader/dumper
 */
t_stat sim_load (FILE *fi, char *cptr, char *fnam, int dump_flag)
{
    t_stat err;

    if (dump_flag) err = m20_abs_mem_dump( fi, fnam );
    else err = m20_abs_load(fi);

    return err;
}



/*
 *  Search opcode by symbolic instruction name
 */
int m20_instr_to_opcode (char *instr)
{
    int i;

    for (i=0; i<M20_SYM_OPCODE_TABLE_SIZE; ++i)
	if (strcmp (m20_opname[i], instr) == 0) return i;

    return -1;
}



/*
 *  Print 12-bit address of machine instruction
 */
void m20_fprint_addr ( FILE *of, int a, int flag )
{
    if (flag) putc ('@', of);

    if (flag && a >= 07700) {
	fprintf (of, "-%o", (a ^ MAX_ADDR_VALUE) + 1);
    } else {
	if (flag) putc ('+', of);
	fprintf (of, "%04o", a);
    }
}



/*
 *  Print machine instruction.
 */
void m20_cmd_fprint( FILE *of, t_value cmd )
{
    const char *m;
    int flags, op, a1, a2, a3;

    flags = cmd >> BITS_42 & MAX_ADDR_TAG_VALUE;
    op =    cmd >> BITS_36 & MAX_OPCODE_VALUE;
    a1 =    cmd >> BITS_24 & MAX_ADDR_VALUE;
    a2 =    cmd >> BITS_12 & MAX_ADDR_VALUE;
    a3 =    cmd >> BITS_0  & MAX_ADDR_VALUE;

    m = m20_opname [op];
    if (cpu_unit.flags & SHORT_SYM_OP) m = m20_short_opname [op];

    fprintf (of, "[op=%02o mod=%0o] %-30s ", op, flags, m );
    m20_fprint_addr (of, a1, flags & 4);

    fprintf (of, ", ");
    m20_fprint_addr (of, a2, flags & 2);

    fprintf (of, ", ");
    m20_fprint_addr (of, a3, flags & 1);
}
                                        



/*
 * Symbolic decode
 *
 * Inputs:
 *	*of	= output stream
 *	addr	= current PC
 *	*val	= pointer to data
 *	*uptr	= pointer to unit
 *	sw	= switches
 * Outputs:
 *	return	= status code
 */
t_stat fprint_sym (FILE *of, t_addr addr, t_value *val,
	UNIT *uptr, int32 sw)
{
    t_value cmd;

    if (uptr && (uptr != &cpu_unit))		/* must be CPU */
	return SCPE_ARG;

    cmd = val[0];
    if (sw & SWMASK ('M')) {			/* symbolic decode? */
	m20_cmd_fprint( of, cmd );
	return SCPE_OK;
    }

    fprintf (of, "%o %02o %04o %04o %04o",
		(int) (cmd >> BITS_42) & MAX_ADDR_TAG_VALUE,
		(int) (cmd >> BITS_36) & MAX_OPCODE_VALUE,
		(int) (cmd >> BITS_24) & MAX_ADDR_VALUE,
		(int) (cmd >> BITS_12) & MAX_ADDR_VALUE,
		(int) (cmd >> BITS_0)  & MAX_ADDR_VALUE);

    return SCPE_OK;
}



char *m20_parse_offset (char *cptr, int *offset)
{
    char *tptr, gbuf[CBUFSIZE];

    cptr = get_glyph (cptr, gbuf, 0);	/* get address */
    *offset = (int)strtotv (gbuf, &tptr, 8);
    if ((tptr == gbuf) || (*tptr != '\0') || (*offset > MAX_ADDR_VALUE)) return 0;

    return cptr;
}




char *m20_parse_address (char *cptr, int *address, int *relative)
{
    cptr = skip_spaces (cptr);			/* absorb spaces */

    if (*cptr >= '0' && *cptr <= '7')
	return m20_parse_offset (cptr, address); /* get address */

    if (*cptr != '@') return 0;

    *relative |= 1;
    cptr = skip_spaces (cptr+1);		/* next char */

    if (*cptr == '+') {
	cptr = skip_spaces (cptr+1);		/* next char */
	cptr = m20_parse_offset (cptr, address);
	if (! cptr) return 0;
    } else if (*cptr == '-') {
 	cptr = skip_spaces (cptr+1);		/* next char */
	cptr = m20_parse_offset (cptr, address);
	if (! cptr) return 0;
	*address = (- *address) & MAX_ADDR_VALUE;
    } else
	return 0;

    return cptr;
}



/*
 * Instruction parse
 */
t_stat parse_instruction (char *cptr, t_value *val, int32 sw)
{
    int opcode, ra, a1, a2, a3;
    char gbuf[CBUFSIZE];

    cptr = get_glyph (cptr, gbuf, 0);		/* get opcode */
    opcode = m20_instr_to_opcode (gbuf);

    if (opcode < 0) return SCPE_ARG;

    ra = 0;
    cptr = m20_parse_address (cptr, &a1, &ra);	/* get address 1 */

    if (! cptr) return SCPE_ARG;

    ra <<= 1;
    cptr = m20_parse_address (cptr, &a2, &ra);	/* get address 2 */
    if (! cptr) return SCPE_ARG;

    ra <<= 1;
    cptr = m20_parse_address (cptr, &a3, &ra);	/* get address 3 */
    if (! cptr) return SCPE_ARG;

    val[0] = (t_value) opcode << BITS_36 | (t_value) ra << BITS_42 | (t_value) a1 << BITS_24 | a2 << BITS_12 | a3;
    if (*cptr != 0) return SCPE_2MARG;

    return SCPE_OK;
}




/*
 * Symbolic input
 *
 * Inputs:
 *	*cptr   = pointer to input string
 *	addr    = current PC
 *	*uptr   = pointer to unit
 *	*val    = pointer to output values
 *	sw      = switches
 * Outputs:
 *	status  = error status
 */
t_stat parse_sym (char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 sw)
{
    int32 i;

    if (uptr && (uptr != & cpu_unit))		/* must be CPU */
	return SCPE_ARG;

    cptr = skip_spaces (cptr);			/* absorb spaces */
    if (! parse_instruction (cptr, val, sw))	/* symbolic parse? */
	return SCPE_OK;

    val[0] = 0;
    for (i=0; i<14; i++) {
	if (*cptr == 0) return SCPE_OK;
	if (*cptr < '0' || *cptr > '7') return SCPE_ARG;
	val[0] = (val[0] << 3) | (*cptr - '0');
	cptr = skip_spaces (cptr+1);		/* next char */
    }
    if (*cptr != 0) return SCPE_ARG;

    return SCPE_OK;
}
