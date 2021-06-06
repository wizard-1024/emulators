/*
 * File:     m20_lp.c
 * Purpose:  M-20 simulator line printer
 *
 * Copyright (c) 2014, Dmitry Stefankov
 *
 * $Id$
 *
 * Revision History.
 *
 *  13-Nov-2014  DVS  Initial Implemementation
 *  14-Nov-2014  DVS  Added text print
 *  17-Nov-2014  DVS  Cleanup and more debugging
 *  18-Nov-2014  DVS  cyclic_sum
 *  20-Nov-2014  DVS  Added print width
 *  21-Nov-2014  DVS  Added very classic print form for M-20
 *  21-Nov-2014  DVS  Added octal help format for print type 3
 *  23-Nov-2014  DVS  Added bcd print (type 4,5) for decimal print
 *  05-Dec-2014  DVS  Minor fixes
 *  27-Dec-2014  DVS  Added +,- bcd-codes according [1973 Lavrov]
 *
 */


#include "m20_defs.h"
#include <math.h>


#define UNIT_V_NEWOUTFMT        (UNIT_V_UF + 0)         
#define UNIT_NEWOUTFMT          (1u << UNIT_V_NEWOUTFMT)
#define UNIT_V_OCTALHELPFMT     (UNIT_V_UF + 1)         
#define UNIT_OCTALHELPFMT       (1u << UNIT_V_OCTALHELPFMT)


/* external memory references */
extern t_value  msu_drum_print_buf[MSU_DRUM_PRINT_BUF_SIZE];
extern int  msu_drum_print_last_pos;

extern const unsigned char m20_to_cyr_win1251[MAX_PRINTER_SYMBOLS_NUM];

/* external references from CPU module */
extern int active_lpt;


/* external functions */
extern char *skip_spaces (char *p);
extern void mosu_store (int addr, t_value val);
extern t_value mosu_load (int addr);
extern t_stat put_code_into_cbuf_reg( t_value ncode);
extern t_value  cyclic_checksum( t_value x, t_value y);
extern double m20_to_ieee (t_value word);

/* functions */
t_stat lpt_reset (DEVICE *dptr);
t_stat lpt_attach (UNIT *uptr, char *cptr);
t_stat lpt_detach (UNIT *uptr);


static t_value  lp_sum = 0;
static int    output_codes_count = 0;
static int    print_width = 7;
static int    decimal_print_type = 4;

static char cr_lf_str[] = "\r\n";

static char lbuf[LPT_WIDTH + 1];                        /* + null */


/* 
   LPT data structures

   lpt_dev      LPT device descriptor
   lpt_unit     LPT unit descriptor
   lpt_reg      LPT register list
*/


UNIT lpt_unit = {
    UDATA (NULL, UNIT_SEQ+UNIT_ATTABLE+UNIT_TEXT, 0)
    };

REG lpt_reg[] = {
    { DRDATA (LPTWIDTH, print_width, 8), PV_LEFT },
    { DRDATA (DPTYPE,   decimal_print_type, 8), PV_LEFT },
    { NULL }
    };


MTAB lpt_mod[] = {
    { UNIT_NEWOUTFMT, 0,              "no new output format ",       "NONEWEXTFMT",    NULL, NULL },
    { UNIT_NEWOUTFMT, UNIT_NEWOUTFMT, "new output extended format ", "NEWEXTFMT",      NULL, NULL },
    { UNIT_OCTALHELPFMT, 0,                 "no octal help format ", "NOOCTHELPFMT",    NULL, NULL },
    { UNIT_OCTALHELPFMT, UNIT_OCTALHELPFMT, "octal help format ",    "OCTHELPFMT",      NULL, NULL },
    { 0 }
    };


DEVICE lpt_dev = {
    "LPT", &lpt_unit, lpt_reg, lpt_mod,
    1, 8, 12, 1, 8, 45,
    NULL, NULL, &lpt_reset,
    NULL, &lpt_attach, &lpt_detach, NULL,
    DEV_DISABLE | DEV_DEBUG
    };



typedef struct decimal_print_values {
    int      num_sign;
    int      exp_sign;
    int      exponent_value;
    t_value  mcode;
    char     mantissa_array[32];
} DECIMAL_PRINT_VALUES, * PDECIMAL_PRINT_VALUES;



/* Reset routine */

t_stat lpt_reset (DEVICE *dptr)
{
    if (sim_deb && lpt_dev.dctrl) fprintf (sim_deb, "lpt: lpt_reset(..)\n");

    /* reset output buffer */
    output_codes_count = 0;
    msu_drum_print_last_pos = 0;
    memset( msu_drum_print_buf, 0, sizeof(msu_drum_print_buf) );

    lp_sum = 0;
    memset( lbuf, 0, sizeof(lbuf) );

    return SCPE_OK;
}


/* Attach routine */

t_stat lpt_attach (UNIT *uptr, char *cptr)
{
    if (sim_deb && lpt_dev.dctrl) fprintf (sim_deb, "lpt: lpt_attach(..)\n");

    lp_sum = 0;
    memset( lbuf, 0, sizeof(lbuf) );

    active_lpt++;

    return attach_unit (uptr, cptr);
}


t_stat lpt_detach (UNIT *uptr)
{
    if (sim_deb && lpt_dev.dctrl) fprintf (sim_deb, "lpt: lpt_detach(..)\n");

    lp_sum = 0;
    memset( lbuf, 0, sizeof(lbuf) );

    active_lpt--;

    return detach_unit (uptr);
}



t_stat  output_lp_char( char ch )
{
    fputc(ch, lpt_unit.fileref);                         /* write line */
    lpt_unit.pos = ftell (lpt_unit.fileref);               /* update position */

    if (ferror (lpt_unit.fileref)) {                       /* error? */
        perror ("Line printer I/O error");
        clearerr (lpt_unit.fileref);
        return SCPE_IOERR;
    }

    return SCPE_OK;
}



t_stat  output_lp_line( char * out_line )
{
    fputs (out_line, lpt_unit.fileref);                         /* write line */
    lpt_unit.pos = ftell (lpt_unit.fileref);               /* update position */

    if (ferror (lpt_unit.fileref)) {                       /* error? */
        perror ("Line printer I/O error");
        clearerr (lpt_unit.fileref);
        return SCPE_IOERR;
    }

    return SCPE_OK;
}




t_stat  make_decimal_print_values( t_value mcode, PDECIMAL_PRINT_VALUES  p_d_values )
{
    int sign, exp_d, i;
    char * p, *s, *p1,*p2;
    double d;
    char  temp_buf[32];

    if (p_d_values == NULL) return SCPE_IOERR;

    d = m20_to_ieee (mcode);
    _snprintf (lbuf,sizeof(lbuf),"%13.9e", d);
    s = lbuf;
    p = s;
    sign = 1;
    if (*p == '-') { sign = 0; p++; }
    p1 = strchr(p,'.');
    p2 = strchr(p,'e');
    if ((p1 == NULL) || (p2 == NULL)) return SCPE_IOERR;

    p_d_values->mcode = mcode;
    p_d_values->num_sign = sign;

    exp_d = atoi(p2+1);

    s = p2; s++;
    memset( temp_buf, 0, sizeof(temp_buf) );
    if (*p == '0') {
        p1++; s = temp_buf; i = 0;
        while( p1 != p2) {
            *s++ = *p1++;
            if (i > (sizeof(temp_buf)-1)) break;
        }
    }
    else {
        s = temp_buf;
        *s++ = *p; exp_d++;
        p1++;
        i=1;
        while( p1 != p2) {
            *s++ = *p1++; i++;
            if (i > (sizeof(temp_buf)-1)) break;
        }
    }
    temp_buf[9] = '\0';

    p_d_values->exponent_value = exp_d;
    sign = 0;
    if (exp_d >= 0) sign = 1; 
    p_d_values->exp_sign = sign;

    strncpy( p_d_values->mantissa_array, temp_buf, 32-1 );

    return SCPE_OK;
}



t_stat  output_m20_type_0( t_value  mcode )
{
    t_stat   err;
    DECIMAL_PRINT_VALUES  dp_values;

    memset( &dp_values, 0, sizeof(dp_values) );
    err = make_decimal_print_values( mcode, &dp_values);
    if (err) return err;

    err = output_lp_char (mcode & TAG ? '1' : '0');
    if (err) return err;
    err = output_lp_char (dp_values.num_sign ? '0' : '1');
    if (err) return err;
    err = output_lp_char (dp_values.exp_sign & 1 ? '0' : '1');
    if (err) return err;
    _snprintf (lbuf,sizeof(lbuf),"%02u", abs(dp_values.exponent_value));
    err = output_lp_line(lbuf);
    if (err) return err;
    _snprintf (lbuf,sizeof(lbuf),"%s", dp_values.mantissa_array);
    err = output_lp_line(lbuf);
    if (err) return err;

    err = output_lp_line(cr_lf_str);
    if (err) return err;

    return SCPE_OK;
}



t_stat  output_m20_type_1( t_value  mcode )
{
    t_stat   err;
    DECIMAL_PRINT_VALUES  dp_values;

    memset( &dp_values, 0, sizeof(dp_values) );
    err = make_decimal_print_values( mcode, &dp_values);
    if (err) return err;

    err = output_lp_char (mcode & TAG ? '+' : '-');
    if (err) return err;
    err = output_lp_char (dp_values.num_sign ? '+' : '-');
    if (err) return err;
    err = output_lp_char (dp_values.exp_sign & 1 ? '+' : '-');
    if (err) return err;
    _snprintf (lbuf,sizeof(lbuf),"%02u", abs(dp_values.exponent_value));
    err = output_lp_line(lbuf);
    if (err) return err;
    _snprintf (lbuf,sizeof(lbuf),"%s", dp_values.mantissa_array);
    err = output_lp_line(lbuf);
    if (err) return err;

    err = output_lp_line(cr_lf_str);
    if (err) return err;

    return SCPE_OK;
}




t_stat  output_m20_type_2( t_value  mcode )
{
    t_stat   err;
    DECIMAL_PRINT_VALUES  dp_values;
    double d;

    memset( &dp_values, 0, sizeof(dp_values) );
    err = make_decimal_print_values( mcode, &dp_values);
    if (err) return err;

    //err = output_lp_char (mcode & TAG ? '+' : '-');
    err = output_lp_char (mcode & TAG ? '-' : '+');
    if (err) return err;
    err = output_lp_char (dp_values.num_sign ? '+' : '-');
    if (err) return err;
    _snprintf (lbuf,sizeof(lbuf),"%s", dp_values.mantissa_array);
    err = output_lp_line(lbuf);
    if (err) return err;
    err = output_lp_char (' ');
    if (err) return err;
    err = output_lp_char (dp_values.exp_sign & 1 ? '+' : '-');
    if (err) return err;
    _snprintf (lbuf,sizeof(lbuf),"%02u", abs(dp_values.exponent_value));
    err = output_lp_line(lbuf);
    if (err) return err;

    /* user-friendly M-20 print style */
    if (lpt_unit.flags & UNIT_OCTALHELPFMT) {
        d = m20_to_ieee (mcode);
        _snprintf (lbuf,sizeof(lbuf),"   [%015llo,", mcode);
        err = output_lp_line(lbuf);
        if (err) return err;
	d = m20_to_ieee (mcode);
	err = output_lp_char (mcode & TAG ? '#' : ' ');
        if (err) return err;
        //_snprintf (lbuf,sizeof(lbuf),"%13e]", d);
        _snprintf (lbuf,sizeof(lbuf),"%.36f]", d);
        err = output_lp_line(lbuf);
        if (err) return err;
    }

    err = output_lp_line(cr_lf_str);
    if (err) return err;

    return SCPE_OK;
}




t_stat  output_m20_type_3( t_value  mcode )
{
    t_stat   err;
    DECIMAL_PRINT_VALUES  dp_values;
    double d;

    memset( &dp_values, 0, sizeof(dp_values) );
    err = make_decimal_print_values( mcode, &dp_values);
    if (err) return err;

    //err = output_lp_char (mcode & TAG ? '+' : '-');
    err = output_lp_char (mcode & TAG ? '-' : '+');
    if (err) return err;
    err = output_lp_char (dp_values.num_sign ? '+' : '-');
    if (err) return err;
    _snprintf (lbuf,sizeof(lbuf),"%s", dp_values.mantissa_array);
    err = output_lp_line(lbuf);
    if (err) return err;
    err = output_lp_char (' ');
    if (err) return err;
    err = output_lp_char (dp_values.exp_sign & 1 ? '+' : '-');
    if (err) return err;
    _snprintf (lbuf,sizeof(lbuf),"%02u", abs(dp_values.exponent_value));
    err = output_lp_line(lbuf);
    if (err) return err;

    /* user-friendly M-20 print style */
    if (lpt_unit.flags & UNIT_OCTALHELPFMT) {
        d = m20_to_ieee (mcode);
        _snprintf (lbuf,sizeof(lbuf),"   [%015llo,", mcode);
        err = output_lp_line(lbuf);
        if (err) return err;
	d = m20_to_ieee (mcode);
	err = output_lp_char (mcode & TAG ? '#' : ' ');
        if (err) return err;
        //_snprintf (lbuf,sizeof(lbuf),"%13e]", d);
        _snprintf (lbuf,sizeof(lbuf),"%.18f]", d);
        err = output_lp_line(lbuf);
        if (err) return err;
    }

    err = output_lp_line(cr_lf_str);
    if (err) return err;

    return SCPE_OK;
}



char bcd2char( int bcd_value )
{
    char ch = ' ';

    if (bcd_value >= 0 && bcd_value <= 9) ch = bcd_value+'0';
#if 1
    if (bcd_value == 10) ch = '+';
    if (bcd_value == 11) ch = '-';
#endif

    return ch;
}




/* binary-coded decimal values */
t_stat  output_m20_type_4( t_value  mcode )
{
    t_stat   err;
    double d;
    int exponent;
    t_value mantissa, t;
    char d_ch;
    int i;

    /* signature,signs */
    err = output_lp_char (mcode & TAG ? '-' : '+');
    if (err) return err;
    err = output_lp_char (mcode & SIGN ? '-' : '+');
    if (err) return err;
    err = output_lp_char (mcode & EXPONENT_SIGN ? '-' : '+');
    if (err) return err;
    /* exponent (2 digits) */
    exponent = (mcode & EXPONENT) >> BITS_36;
    //fprintf( stderr, "exponent=%02oo (0x%02X)\n", exponent, exponent );
    d_ch = (exponent >> 4) & 03;
    //fprintf( stderr, "d_ch=%d\n", d_ch );
    err = output_lp_char ( bcd2char(d_ch) );
    if (err) return err;
    d_ch = (exponent & 017);
    //fprintf( stderr, "d_ch=%d\n", d_ch );
    err = output_lp_char ( bcd2char(d_ch) );
    if (err) return err;
    /* delimiter */
    err = output_lp_char (' ');
    if (err) return err;
    /* mantissa (9 digits) */
    mantissa = mcode & MANTISSA;
    for( i=0; i<9; i++ ) {
        t = (mantissa >> (32-i*4)) & 017;
        d_ch = (char)(t);
        err = output_lp_char ( bcd2char(d_ch) );
        if (err) return err;
    }

    /* user-friendly M-20 print style */
    if (lpt_unit.flags & UNIT_OCTALHELPFMT) {
        d = m20_to_ieee (mcode);
        _snprintf (lbuf,sizeof(lbuf),"   [%015llo,", mcode);
        err = output_lp_line(lbuf);
        if (err) return err;
	d = m20_to_ieee (mcode);
	err = output_lp_char (mcode & TAG ? '#' : ' ');
        if (err) return err;
        _snprintf (lbuf,sizeof(lbuf),"%13e]", d);
        err = output_lp_line(lbuf);
        if (err) return err;
    }

    err = output_lp_line(cr_lf_str);
    if (err) return err;

    return SCPE_OK;
}



/* binary-coded decimal values */
t_stat  output_m20_type_5( t_value  mcode )
{
    t_stat   err;
    int exponent;
    t_value mantissa, t;
    char d_ch;
    int i;

    /* signature,signs */
    err = output_lp_char (mcode & TAG ? '1' : '0');
    if (err) return err;
    err = output_lp_char (mcode & SIGN ? '1' : '0');
    if (err) return err;
    err = output_lp_char (mcode & EXPONENT_SIGN ? '1' : '0');
    if (err) return err;
    /* exponent (2 digits) */
    exponent = (mcode & EXPONENT) >> BITS_36;
    d_ch = (exponent >> 4) & 03;
    err = output_lp_char ( bcd2char(d_ch) );
    if (err) return err;
    d_ch = (exponent & 017);
    err = output_lp_char ( bcd2char(d_ch) );
    if (err) return err;
    /* mantissa (9 digits) */
    mantissa = mcode & MANTISSA;
    for( i=0; i<9; i++ ) {
        t = (mantissa >> (32-i*4)) & 017;
        d_ch = (char)(t);
        err = output_lp_char ( bcd2char(d_ch) );
        if (err) return err;
    }

    err = output_lp_line(cr_lf_str);
    if (err) return err;

    return SCPE_OK;
}




/* 
   Print routine
*/

t_stat write_line_printer (int start_addr, int end_addr, int zone_buf_addr, 
                           int pr_type, int add_only_flag, int dis_mem_acc, int dis_chksum, 
                           int * ocodes )
{
    int out_codes, addr, count;
    t_value  mcode;
    t_stat   err;
    double   d;
    int  values_count;

    if (sim_deb && lpt_dev.dctrl) fprintf (sim_deb, "lpt: write_line(..)\n");

    if ((lpt_unit.flags & UNIT_ATT) == 0)                  /* attached? */
        return SCPE_UNATT;

    /* change zone of buffer register */
    if (zone_buf_addr > 0) {
      if (zone_buf_addr >= MSU_DRUM_PRINT_BUF_SIZE) {
        return STOP_INVARG;
      }
      msu_drum_print_last_pos = zone_buf_addr;
    }

    /* wrong address */
    if (start_addr > end_addr) {
        return STOP_INVARG;
    }

    /* write all codes */
    addr = start_addr;
    count = end_addr - start_addr + 1;

    if (sim_deb && lpt_dev.dctrl) {
      fprintf (sim_deb, "lpt: write_line: start_addr=%04o, end_addr=%04o, count=%d, pr_type=%d\n",
               start_addr,end_addr,count,pr_type);
    }

    while( count--) {
        if (dis_mem_acc) mcode = 0;
        else mcode = mosu_load(addr);
        lp_sum = cyclic_checksum( lp_sum, mcode );
        mcode |= COMMON_CODE_MARKER_SIGN;
        err = put_code_into_cbuf_reg(mcode);
        addr++;
    }

    /* no print, only buffer register update */
    if (add_only_flag) {
      if (sim_deb && lpt_dev.dctrl) fprintf (sim_deb, "lpt: write_line: update buffer only.\n");
      return SCPE_OK;
    }

    /* output checksum */
    if (!dis_chksum) {
        mcode = lp_sum;
        mcode |= CHECKSUM_MARKER_SIGN;
        err = put_code_into_cbuf_reg(mcode);
    }

    /* output end marker */
    mcode = END_MARKER_SIGN;
    err = put_code_into_cbuf_reg(mcode);


    /* print codes from buffer */
    count = 0;
    out_codes = 0;
    values_count = 0;

    while( count < MSU_DRUM_PRINT_BUF_SIZE) {
        mcode = msu_drum_print_buf[count];
        if (mcode & END_MARKER_SIGN) break;

        if ((mcode & COMMON_CODE_MARKER_SIGN) || (mcode & CHECKSUM_MARKER_SIGN)) {
            if (mcode & CHECKSUM_MARKER_SIGN) {
              if (dis_chksum) continue;
              mcode &= ~CHECKSUM_MARKER_SIGN;
            }
            mcode &= ~COMMON_CODE_MARKER_SIGN;

            if (pr_type == PRINT_TYPE_OCTAL) {
                _snprintf (lbuf,sizeof(lbuf),"%015llo", mcode);
                err = output_lp_line(lbuf);
                if (err) return err;
                values_count++;
                if (values_count >= print_width) {
                    err = output_lp_line(cr_lf_str);
                    if (err) return err;
                    values_count = 0;
                }
                else {
	            err = output_lp_char (' ');
                    if (err) return err;
                }
            }

            if (pr_type == PRINT_TYPE_DECIMAL) {
                if (lpt_unit.flags & UNIT_NEWOUTFMT) {
	            d = m20_to_ieee (mcode);
	            err = output_lp_char (mcode & TAG ? '#' : ' ');
                    if (err) return err;
                    _snprintf (lbuf,sizeof(lbuf),"%13e", d);
                    err = output_lp_line(lbuf);
                    if (err) return err;
                    values_count++;
                    if (values_count >= print_width) {
                        err = output_lp_line(cr_lf_str);
                        if (err) return err;
                        values_count = 0;
                    }
                    else {
	                err = output_lp_char (' ');
                        if (err) return err;
                    }
                }
                else {
                    switch( decimal_print_type ) {
                        case 5:
                            err = output_m20_type_5( mcode );
                            break;
                        case 4:
                            err = output_m20_type_4( mcode );
                            break;
                        case 3:
                            err = output_m20_type_3( mcode );
                            break;
                        case 2:
                            err = output_m20_type_2( mcode );
                            break;
                        case 1:
                            err = output_m20_type_1( mcode );
                            break;
                        case 0:
                        default:
                            err = output_m20_type_0( mcode );
                            break;
                    }
                }
            }

            if (pr_type == PRINT_TYPE_TEXT) {
              int i,c;
	      for (i=0; i<6; ++i) {
	          c = mcode >> (35 - 7*i) & 0177;
	          if (c == 127) continue;
	          c = m20_to_cyr_win1251[c];;
	          if (! c) c = ' ';
	          /* Assume we have windows-151 locale. */
	          err = output_lp_char (c);
                  if (err) return err;
                  values_count++;
                  if (values_count > 127) {
                    err = output_lp_line(cr_lf_str);
                    if (err) return err;
                    values_count = 0;
                  }
	      } /*for*/
            }

            out_codes++;
            if (ocodes != NULL) *ocodes = out_codes;
            goto print_next_code;
        }
      print_next_code:
        count++;
    }

#if 0
    if (values_count != 0) {
        err = output_lp_line(cr_lf_str);
        if (err) return err;
    }
#endif

    if (ocodes != NULL) *ocodes = out_codes;

    /* reset output buffer */
    output_codes_count = 0;
    lp_sum = 0;
    msu_drum_print_last_pos = 0;
    memset( msu_drum_print_buf, 0, sizeof(msu_drum_print_buf) );

    return SCPE_OK;
}

