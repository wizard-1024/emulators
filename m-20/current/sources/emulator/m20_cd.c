/*
 * File:     m20_cd.c
 * Purpose:  M-20 simulator card reader (punch)
 *
 * Copyright (c) 2014, Dmitry Stefankov
 *
 * $Id$
 *
 * Revision History.
 *
 *  10-Nov-2014  DVS  Initial Implemementation
 *  11-Nov-2014  DVS  Updated
 *  12-Nov-2014  DVS  Added punch card (writer)
 *  13-Nov-2014  DVS  Added circular buffer logic
 *  17-Nov-2014  DVS  Cleanup and more debugging
 *  18-Nov-2014  DVS  cyclic_sum
 *  20-Nov-2014  DVS  Added modifiers, various formats 
 *                    of input and output
 *  23-Nov-2014  DVS  Added binary-decimal print
 *  03-Dec-2014  DVS  Changed cdr boot logic
 *  05-Dec-2014  DVS  Minor fixes
 *  07-Dec-2014  DVS  Changed bcd input
 *  27-Dec-2014  DVS  Added check for card read beyond memory
 *  17-Jan-2015  DVS  Added another binary-decimal input form
 *  25-Jan-2015  DVS  Added more debugging to see input errors
 *  13-Mar-2015  DVS  Cleanup code
 *
 */


#include "m20_defs.h"
#include <math.h>


#define UNIT_V_OUTEXTFMT        (UNIT_V_UF + 0)         
#define UNIT_OUTEXTFMT          (1u << UNIT_V_OUTEXTFMT)
#define UNIT_V_INTEXTFMT        (UNIT_V_UF + 1)         
#define UNIT_INEXTFMT           (1u << UNIT_V_INTEXTFMT)


/* external references (CPU module) */

extern uint16   regKRA;
extern uint16   regRA;
extern uint16   regSMA;


extern t_value cr_io_addr_1;
extern t_value cr_io_addr_2;
extern t_value cr_io_addr_3;

extern void mosu_store (int addr, t_value val);
extern t_value mosu_load (int addr);
extern t_stat put_code_into_cbuf_reg( t_value ncode);
extern t_value  cyclic_checksum( t_value x, t_value y);

extern int   boot_device_req_cdr;
extern int   active_cdp;


/* external references (SYS module) */
extern t_value  ieee_to_m20 (double d);
extern char *   skip_spaces (char *p);
extern char *   skip_nonspaces (char *p);

/* external references (LP module) */
extern  char bcd2char( int bcd_value );

/* external references (DRM module) */
extern t_value  msu_drum_print_buf[MSU_DRUM_PRINT_BUF_SIZE];
extern int  msu_drum_print_last_pos;

char cdr_buf[CDR_BUF_SIZE];                      /* > CDR_WIDTH */
char cdp_line_buf[(CDP_BUF_SIZE) + 1];           /* + null */

char debug_cdr_buf[CDR_BUF_SIZE];                /* > CDR_WIDTH */


static int   output_codes_count = 0;
static int32 cdp_buf_full = 0;                   /* punch buf full? */
static t_value  cdp_sum = 0;
int   cdr_input_codes_count;

static int bcd_print = 0;

t_stat cdr_set_mode (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat cdp_set_mode (UNIT *uptr, int32 val, char *cptr, void *desc);

t_stat cdr_svc (UNIT *uptr);
t_stat cdr_boot (int32 unitno, DEVICE *dptr);
t_stat cdr_attach (UNIT *uptr, char *cptr);
t_stat cdr_detach (UNIT *uptr);
t_stat cdr_reset (DEVICE *dptr);

t_stat cdp_reset (DEVICE *dptr);
t_stat cdp_attach (UNIT *uptr, char *cptr);
t_stat cdp_detach (UNIT *uptr);


/* 
   Card reader data structures

   cdr_dev      CDR descriptor
   cdr_unit     CDR unit descriptor
   cdr_reg      CDR register list
*/


UNIT cdr_unit = {
    UDATA (&cdr_svc, UNIT_SEQ+UNIT_ATTABLE+UNIT_ROABLE+UNIT_TEXT, 0), 100
    };

REG cdr_reg[] = {
    { 0 },
    { NULL }
};

MTAB cdr_mod[] = {                                                   
    { UNIT_INEXTFMT,  0,              "no input extended format ",  "NOEXT",    &cdr_set_mode, NULL },
    { UNIT_INEXTFMT,  UNIT_INEXTFMT,  "input extended format ",     "EXTFMT",   &cdr_set_mode, NULL },
    { 0 }
};

DEVICE cdr_dev = {
    "CDR", &cdr_unit, cdr_reg, cdr_mod,
    1, 8, 12, 1, 8, 45,
    NULL, NULL, &cdr_reset,
    &cdr_boot, &cdr_attach, &cdr_detach, NULL,
    DEV_DISABLE | DEV_DEBUG
};


/* 
   CDP data structures

   cdp_dev      CDP device descriptor
   cdp_unit     CDP unit descriptor
   cdp_reg      CDP register list
*/


UNIT cdp_unit = {
    UDATA (NULL, UNIT_SEQ+UNIT_ATTABLE+UNIT_TEXT, 0)
    };

REG cdp_reg[] = {
    { DRDATA (BCDPRINT,  bcd_print, 8), PV_LEFT },
    { NULL }
    };

MTAB cdp_mod[] = {                                                   
    { UNIT_OUTEXTFMT, 0,              "no output extended format ", "NOEXT",    &cdp_set_mode, NULL },
    { UNIT_OUTEXTFMT, UNIT_OUTEXTFMT, "output extended format ",    "EXTFMT",   &cdp_set_mode, NULL },
    { 0 }
};

DEVICE cdp_dev = {
    "CDP", &cdp_unit, cdp_reg, cdp_mod,
    1, 8, 12, 1, 8, 45,
    NULL, NULL, &cdp_reset,
    NULL, &cdp_attach, &cdp_detach, NULL,
    DEV_DISABLE | DEV_DEBUG
    };


/* Reader/punch set mode - valid only if not attached */

t_stat cdr_set_mode (UNIT *uptr, int32 val, char *cptr, void *desc)
{
    if (sim_deb && cdr_dev.dctrl) fprintf (sim_deb, "cdr: cdr_set_mode(..)\n");

    return (uptr->flags & UNIT_ATT) ? SCPE_NOFNC: SCPE_OK;
}



/* Card reader reset */

t_stat cdr_reset (DEVICE *dptr)
{
    if (sim_deb && cdr_dev.dctrl) fprintf (sim_deb, "cdr: cdr_reset(..)\n");

    sim_cancel (&cdr_unit);                                /* clear reader event */

    cr_io_addr_1 = 0;
    cr_io_addr_2 = 0;
    cr_io_addr_3 = 0;

    cdr_input_codes_count = 0;

    return SCPE_OK;
}



/* Card reader attach */

t_stat cdr_attach (UNIT *uptr, char *cptr)
{
    if (sim_deb && cdr_dev.dctrl) fprintf (sim_deb, "cdr: cdr_attach(..)\n");

    return attach_unit (uptr, cptr);
}



/* Card reader detach */

t_stat cdr_detach (UNIT *uptr)
{
    if (sim_deb && cdr_dev.dctrl) fprintf (sim_deb, "cdr: cdr_detach(..)\n");

    return detach_unit (uptr);
}



/* Bootstrap routine */

t_stat cdr_boot (int32 unitno, DEVICE *dptr)
{
    if (sim_deb && cdr_dev.dctrl) fprintf (sim_deb, "cdr: cdr_boot(..)\n");

    regKRA = 1;
    regRA = 0;
    regSMA = 0;
    mosu_store( regKRA, 010000100010000LL);
    boot_device_req_cdr = 1;

    return SCPE_OK;
}



/*  Card reader service. */
t_stat cdr_svc (UNIT *uptr)
{
    if (sim_deb && cdr_dev.dctrl) fprintf (sim_deb, "cdr: cdr_svc(..)\n");

    return SCPE_OK;
}



int get_marker_value( char ch)
{
   int res = -1;

   if (ch == '0') res = 0;
   if (ch == '1') res = 1;

   return res;
}





/* 
   Card read routine
   Read until end marker encountered.
*/
t_stat read_card (t_value * csum, t_value * rsum, int * rcodes,
                   int * stop_blocking, int * control_blocking)
{
    int i;
    t_stat r;
    int cr_input_done = 0;
    char *p, *s;
    int  main_marker, aux_marker, store_addr, d_sign, d_tag, d_exp_sign, exp_val, d_m;
    int  a1, a2, a3, d_exp, d;
    t_value  sum, rcode, r_sum, tcode, e, m;
    //double f;
    int  do_write = 0;

    if (sim_deb && cdr_dev.dctrl) fprintf (sim_deb, "cdr: read_card(..)\n");

    if (sim_is_active (&cdr_unit)) {                       /* busy? */
      sim_cancel (&cdr_unit);                              /* cancel */
      if ((r = cdr_svc (&cdr_unit)))                       /* process */
          return r;
    }

    if ((cdr_unit.flags & UNIT_ATT) == 0)                  /* attached? */
        return SCPE_UNATT;

    //if (cr_io_addr_2 == 0)  return STOP_CRINVAL;
    //if (cr_io_addr_1 == 0)  return STOP_CRINVAL;

    sum = 0;
    main_marker = aux_marker = -1;
    store_addr = (int)cr_io_addr_1;
    if (store_addr) do_write = 1;
    cdr_input_codes_count = 0;

    while( !cr_input_done ) {
        memset( cdr_buf, 0, sizeof(cdr_buf) );             /* clear extended buf */

        fgets (cdr_buf, CDR_BUF_SIZE, cdr_unit.fileref);   /* rd char card */

        if (store_addr >= MAX_MEM_SIZE) {                  /* cannot read card out of memory! */
            return STOP_CROUTMEMORY;
        }

        main_marker = aux_marker = -1;

        if (cdr_buf[0] == ';') goto next_card;             /* comment */
        if (strlen(cdr_buf) < 9) goto next_card;           /* too small line */

        /* get left marker */
        p = skip_spaces(cdr_buf);
        main_marker = get_marker_value(*p);

        if (sim_deb && cdr_dev.dctrl) {
           memset( debug_cdr_buf, 0, sizeof(debug_cdr_buf) );
           strncpy( debug_cdr_buf, cdr_buf, sizeof(debug_cdr_buf)-1 );
           s = strchr(debug_cdr_buf,'\n'); if (s != NULL) *s = '\0';
           s = strchr(debug_cdr_buf,'\r'); if (s != NULL) *s = '\0';
           fprintf (sim_deb, "cdr: read_card(): cdr_buf='%s'\n", debug_cdr_buf );
        }

        /* process number */
        p = skip_spaces(p+1);
        rcode = 0;
        if (cdr_unit.flags & UNIT_INEXTFMT) {
           if (*p == '=') {
               rcode = ieee_to_m20 (strtod (p+1, NULL));
               p = skip_nonspaces(p+1);
               p--;
               goto take_right_marker;
           }
        }
        if (*p == '+' || *p == '-') {
           /* binary-decimal-coded number */
           d_tag = 0;
           /* detect tage */
           d_tag = 0;
           if ((*p != '+') && (*p != '-')) return STOP_CRFMTINVAL;
           if (*p=='+') d_tag = 1;
           if (*p=='-') d_tag = 0;
           p++;
           /* detect number sign */
           d_sign = 0;
           if ((*p != '+') && (*p != '-')) return STOP_CRFMTINVAL;
           if (*p=='+') d_sign = 0;
           if (*p=='-') d_sign = 1;
           p=skip_spaces(p+1);
           /* detect exponent sign and exponent sign */
           d_exp = atoi(p);
           d_exp_sign = 0;
           if (d_exp < 0) d_exp_sign = 1;
           exp_val = abs(d_exp);
           if (exp_val > 19) return SCPE_FMT;
           e = exp_val % 10;
           if (exp_val / 10) e |= 1 << 4;
           p = skip_nonspaces(p+1);
           p=skip_spaces(p+1);
           /* detect mantissa */
           m = 0;
           for( i=0; i<9; i++ ) {
	       if (*p < '0' || *p > '9') return SCPE_FMT;
               d_m = *p - '0';
               m |= ((t_value)d_m << (32-i*4));
               p=skip_spaces(p+1);
           }
           tcode = m;
           tcode |= (e << BITS_36);
           if (d_tag) tcode |= TAG;
           if (d_sign) tcode |= SIGN;
           if (d_exp_sign) tcode |= EXPONENT_SIGN;
           rcode = tcode;
           p--;
           goto take_right_marker;
        }
        if (*p == '#') {
            /* binary-decimal-coded number (another form) */
            tcode = 0;
            p=skip_spaces(p+1);
            /* detect tag, sign, exponent sign */
            if (*p < '0' || *p > '7') return STOP_CRFMTINVAL;
            d_tag = 0; d_sign = 0; d_exp_sign = 0;
            d = *p - '0';
            if (d & 4) d_tag = 1;
            if (d & 2) d_sign = 1;
            if (d & 1) d_exp_sign = 1;
            /* detect exponent */
            p=skip_spaces(p+1);
            d_exp = atoi(p);
            exp_val = abs(d_exp);
            if (exp_val > 19) return SCPE_FMT;
            e = exp_val % 10;
            if (exp_val / 10) e |= 1 << 4;
            p = skip_nonspaces(p+1);
            p=skip_spaces(p+1);
            /* detect mantissa */
            m = 0;
            for( i=0; i<9; i++ ) {
	        if (*p < '0' || *p > '9') return SCPE_FMT;
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
            rcode = tcode;
            p--;
            goto take_right_marker;
        }

#if 0
        if (*p == '+' || *p == '-') {
           /* as decimal number, not bcd number! */
           d_tag = 0;
           if (*p=='+') d_tag = 1;
           if (*p=='-') d_tag = 0;
           p++;
           d_sign = 1;
           if (*p=='+') d_sign = 1;
           if (*p=='-') d_sign = -1;
           p=skip_spaces(p+1);
           d_exp = atoi(p);
           p = skip_nonspaces(p+1);
           p=skip_spaces(p+1);
           if (*p < '0' || *p > '9') return STOP_CRFMTINVAL;
           tcode = *p - '0';
           for (i=0; i<9; i++) {
	      p = skip_spaces(p+1);
	      if (*p < '0' || *p > '9') return STOP_CRFMTINVAL;
	      tcode = (tcode * 10) + (*p - '0');
           }
           f = (double)tcode * 1E-10;
           f *= d_sign;
           f *= pow(10,d_exp );
           rcode = ieee_to_m20 (f);
           if (d_tag) rcode |= TAG;
           p--;
           goto take_right_marker;
        }
#endif


        if (*p < '0' || *p > '7') return STOP_CRFMTINVAL;
        rcode = *p - '0';
        for (i=0; i<14; i++) {
	   p = skip_spaces(p+1);
	   if (*p < '0' || *p > '7') return STOP_CRFMTINVAL;
	   rcode = (rcode << 3) | (*p - '0');
        }

     take_right_marker:
        /* get right marker */
        p = skip_spaces(p+1);
        aux_marker = get_marker_value(*p);

        if ((main_marker < 0) || (aux_marker < 0)) return STOP_CRFMTINVAL;

        cdr_input_codes_count++;

      next_card:
        if (feof (cdr_unit.fileref))                       /* eof? */
            return STOP_NOCD;

         cdr_unit.pos = ftell (cdr_unit.fileref);          /* update position */

	 a1 = rcode >> BITS_24 & MAX_ADDR_VALUE;
	 a2 = rcode >> BITS_12 & MAX_ADDR_VALUE;
	 a3 = rcode >> BITS_0  & MAX_ADDR_VALUE;

         /* Code marker */
         if ((main_marker == 1) && (aux_marker == 0)) {
            sum = cyclic_checksum( sum, rcode );
            if (do_write) mosu_store(store_addr,rcode);
            store_addr++;
         }

         /* Address marker */
         if ((main_marker == 0) && (aux_marker == 1)) {
            sum = cyclic_checksum( sum, rcode );
            if (a1 > 0) store_addr = a1;
            do_write = 1;
            if (a1 == 0) do_write = 0;
            if (a2 == 1) do_write = 0;
            if (a3 & 1) *stop_blocking = 1;
            if (a3 >> 11 & 1) *control_blocking = 1;
         }

         /* End marker */
         if ((main_marker == 1) && (aux_marker == 1)) {
           cr_input_done = 1;
           r_sum = rcode;
           if (rcodes != NULL) *rcodes = cdr_input_codes_count;
           if (csum != NULL) *csum = sum;
           if (rsum != NULL) *rsum = r_sum;
           if (sim_deb && cdr_dev.dctrl) 
             fprintf (sim_deb, "cdr: read_card(..) r_sum=%015llo, c_sum=%015llo\n", r_sum, sum );
         }
    }

    if (sim_deb && cdr_dev.dctrl) 
      fprintf (sim_deb, "cdr: read_card(..) read_codes_count=%d\n", cdr_input_codes_count);

    sim_activate (&cdr_unit, cdr_unit.wait);              /* activate */

    return SCPE_OK;
}




/* Reader/punch set mode - valid only if not attached */

t_stat cdp_set_mode (UNIT *uptr, int32 val, char *cptr, void *desc)
{
    if (sim_deb && cdp_dev.dctrl) fprintf (sim_deb, "cdp: cdp_set_mode(..)\n");

    return (uptr->flags & UNIT_ATT) ? SCPE_NOFNC: SCPE_OK;
}



/* Card punch reset */

t_stat cdp_reset (DEVICE *dptr)
{
    if (sim_deb && cdp_dev.dctrl) fprintf (sim_deb, "cdp: cdp_reset(..)\n");

    sim_cancel (&cdp_unit);                                /* clear punch event */

    return SCPE_OK;
}




/* Card punch attach */

t_stat cdp_attach (UNIT *uptr, char *cptr)
{
    if (sim_deb && cdp_dev.dctrl) fprintf (sim_deb, "cdp: cdp_attach(..)\n");

    cdp_buf_full = 0;
    output_codes_count = 0;
    cdp_sum = 0;
    msu_drum_print_last_pos = 0;

    memset( msu_drum_print_buf, 0, sizeof(msu_drum_print_buf) );

    active_cdp++;

    return attach_unit (uptr, cptr);
}



/* Card punch detach */

t_stat cdp_detach (UNIT *uptr)
{
    if (sim_deb && cdp_dev.dctrl) fprintf (sim_deb, "cdp: cdp_detach(..)\n");

    active_cdp--;

    return detach_unit (uptr);
}





t_stat  output_cdp_line( char * out_line )
{
    fputs (out_line, cdp_unit.fileref);                    /* output card */
    //fputc ('\n', cdp_unit.fileref);                      /* plus new line */
    cdp_unit.pos = ftell (cdp_unit.fileref);               /* update position */

    if (ferror (cdp_unit.fileref)) {                       /* error? */
        perror ("Card punch I/O error");
        clearerr (cdp_unit.fileref);
        return SCPE_IOERR;
    }

    return SCPE_OK;
}



t_stat  output_cdp_char( char ch )
{
    fputc (ch, cdp_unit.fileref);                          /* output card */
    cdp_unit.pos = ftell (cdp_unit.fileref);               /* update position */

    if (ferror (cdp_unit.fileref)) {                       /* error? */
        perror ("Card punch I/O error");
        clearerr (cdp_unit.fileref);
        return SCPE_IOERR;
    }

    return SCPE_OK;
}



t_stat  output_bcd_cdp_line( t_value mcode )
{
    t_stat   err;
    int exponent;
    t_value mantissa, t;
    int i;
    char d_ch;

    /* signature,signs */
    err = output_cdp_char(mcode & TAG ? '1' : '0');
    if (err) return err;
    err = output_cdp_char(mcode & SIGN ? '1' : '0');
    if (err) return err;
    err = output_cdp_char (mcode & EXPONENT_SIGN ? '1' : '0');
    if (err) return err;

    /* exponent (2 digits) */
    exponent = (mcode & EXPONENT) >> BITS_36;
    d_ch = (exponent >> 4) & 03;
    err = output_cdp_char ( bcd2char(d_ch) );
    if (err) return err;
    d_ch = (exponent & 017);
    err = output_cdp_char ( bcd2char(d_ch) );
    if (err) return err;

    /* delimiter */
    err = output_cdp_char (' ');
    if (err) return err;

    /* mantissa (9 digits) */
    mantissa = mcode & MANTISSA;
    for( i=0; i<9; i++ ) {
        t = (mantissa >> (32-i*4)) & 017;
        d_ch = (char)(t);
        err = output_cdp_char ( bcd2char(d_ch) );
        if (err) return err;
    }

    return SCPE_OK;
} 




/* 
   Card punch routine
   - Run out any previously buffered card
   - Copy card from memory buffer to punch buffer
*/
t_stat punch_card (int start_addr, int end_addr, int zone_buf_addr, int add_only_flag, 
                   int dis_mem_acc, int dis_chksum, int * ocodes, t_value *sum )
{
    int out_codes, addr, count;
    t_value  mcode;
    t_stat   err;
    int      code_section = 0;

    if (sim_deb && cdp_dev.dctrl) fprintf (sim_deb, "cdp: punch_card(..)\n");

    if ((cdp_unit.flags & UNIT_ATT) == 0)                  /* attached? */
      return SCPE_UNATT;

    /* Change update position */
    if (sim_deb && cdp_dev.dctrl) {
      fprintf (sim_deb, "cdp: punch_card: zone_buf_addr=%d\n",zone_buf_addr);
    }
#if 0
    if (zone_buf_addr > 0) {
      if (zone_buf_addr >= MSU_DRUM_PRINT_BUF_SIZE) return STOP_INVARG;
      //msu_drum_print_last_pos = zone_buf_addr;
    }
#endif
    if (zone_buf_addr > 0) {
      if (zone_buf_addr >= MSU_DRUM_PRINT_BUF_SIZE) {
        /* reset output buffer */
        cdp_buf_full = 0;
        output_codes_count = 0;
        cdp_sum = 0;
        msu_drum_print_last_pos = 0;
        memset( msu_drum_print_buf, 0, sizeof(msu_drum_print_buf) );
      }
    }


    /* add codes to buffer */
    addr = start_addr;
    if (cdp_unit.flags & UNIT_OUTEXTFMT) {
      mcode = start_addr << BITS_24;
      cdp_sum = cyclic_checksum( cdp_sum, mcode );
      mcode |= ADDRESS_CODE_MARKER_SIGN;
      err = put_code_into_cbuf_reg(mcode);
    }

    count = end_addr - start_addr + 1;
    if (sim_deb && cdp_dev.dctrl) {
      fprintf (sim_deb, "cdp: punch_card: start_addr=%04o, end_addr=%04o, count=%d\n",start_addr,end_addr,count);
    }

    while( count--) {
        if (dis_mem_acc) mcode = 0;
        else mcode = mosu_load(addr);
        cdp_sum = cyclic_checksum( cdp_sum, mcode );
        mcode |= COMMON_CODE_MARKER_SIGN;
        err = put_code_into_cbuf_reg(mcode);
        addr++;
    }

    if (add_only_flag) {
      if (sim_deb && cdp_dev.dctrl) fprintf (sim_deb, "cdp: punch_card: update buffer only.\n");
      return SCPE_OK;
    }

    /* output end marker */
    mcode = END_MARKER_SIGN;
    err = put_code_into_cbuf_reg(mcode);

    /* print codes from buffer */
    count = 0;
    out_codes = 0;
    while( count < MSU_DRUM_PRINT_BUF_SIZE) {
        mcode = msu_drum_print_buf[count];
        if (mcode & END_MARKER_SIGN) break;
        if (mcode & ADDRESS_CODE_MARKER_SIGN) {
            err = output_cdp_line("\n; address code\n");
            if (err) return err;
            mcode &= ~ADDRESS_CODE_MARKER_SIGN;
            addr = mcode >> BITS_24 & MAX_ADDR_VALUE;
            _snprintf(cdp_line_buf, sizeof(cdp_line_buf),
                      "0  0 00 %04o 0000 0000  1\n", addr );
            err = output_cdp_line(cdp_line_buf);
            if (err) return err;
            out_codes++;
            if (ocodes != NULL) *ocodes = out_codes;
            code_section = 0;
            goto print_next_code;
        }
        if (mcode & COMMON_CODE_MARKER_SIGN) {
            if (!code_section) {
              code_section = 1;
              err = output_cdp_line("\n; common codes section\n");
              if (err) return err;
            }
            mcode &= ~COMMON_CODE_MARKER_SIGN;
            if (bcd_print) {
                /* main marker */
                _snprintf(cdp_line_buf, sizeof(cdp_line_buf), "1 " );
                err = output_cdp_line(cdp_line_buf);
                if (err) return err;
                /* output code as bcd */
                output_bcd_cdp_line(mcode);
                /* auxiliary marker */
                _snprintf(cdp_line_buf, sizeof(cdp_line_buf), " 0\n" );
                err = output_cdp_line(cdp_line_buf);
                if (err) return err;
            }
            else {
                _snprintf(cdp_line_buf, sizeof(cdp_line_buf),
                      "1  %01o %02o %04o %04o %04o  0\n", 
                      mcode >> BITS_42 & MAX_ADDR_TAG_VALUE, mcode >> BITS_36 & MAX_OPCODE_VALUE, 
                      mcode >> BITS_24 & MAX_ADDR_VALUE, mcode >> BITS_12 & MAX_ADDR_VALUE, 
                      mcode >> BITS_0 & MAX_ADDR_VALUE );
                err = output_cdp_line(cdp_line_buf);
                if (err) return err;
            }
            out_codes++;
            if (ocodes != NULL) *ocodes = out_codes;
            goto print_next_code;
        }
      print_next_code:
        count++;
    }


    /* write checksum and end-of-input marker */
    if (dis_chksum) goto flush_buffer;
    err = output_cdp_line("\n; end-of-input marker and checksum\n");
    if (err) return err;
    mcode = cdp_sum;
    if (bcd_print) {
        /* main marker */
        _snprintf(cdp_line_buf, sizeof(cdp_line_buf), "1 " );
        err = output_cdp_line(cdp_line_buf);
        if (err) return err;
        /* output code as bcd */
        output_bcd_cdp_line(mcode);
        /* auxiliary marker */
        _snprintf(cdp_line_buf, sizeof(cdp_line_buf), " 1\n" );
        err = output_cdp_line(cdp_line_buf);
        if (err) return err;
     }
     else {
         _snprintf(cdp_line_buf, sizeof(cdp_line_buf),
                   "1  %01o %02o %04o %04o %04o  1\n\n", 
                    mcode >> BITS_42 & MAX_ADDR_TAG_VALUE, mcode >> BITS_36 & MAX_OPCODE_VALUE, 
                    mcode >> BITS_24 & MAX_ADDR_VALUE, mcode >> BITS_12 & MAX_ADDR_VALUE, 
                    mcode >> BITS_0 & MAX_ADDR_VALUE );
         err = output_cdp_line(cdp_line_buf);
         if (err) return err;
    }
    if (ocodes != NULL) *ocodes = out_codes+1;

flush_buffer:
    if ((sum != NULL) && !dis_mem_acc) {
      *sum = cdp_sum;
      if (sim_deb && cdp_dev.dctrl) {
        fprintf (sim_deb, "cdp: punch_card: chksum=%015llo\n",cdp_sum);
      }
    }

    /* reset output buffer */
    cdp_buf_full = 0;
    output_codes_count = 0;
    cdp_sum = 0;
    msu_drum_print_last_pos = 0;
    memset( msu_drum_print_buf, 0, sizeof(msu_drum_print_buf) );

    return SCPE_OK;
}
