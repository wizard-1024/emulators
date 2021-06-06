/*
 * File:     m20_cpu.c
 * Purpose:  M-20 CPU and memory simulator
 *
 * Copyright (c) 2009, Serge Vakulenko
 * Copyright (c) 2014, Dmitry Stefankov
 *
 * $Id$
 *
 * Revision History.
 *
 *  03-Nov-2014  DVS  Initial Implemementation
 *  08-Nov-2014  DVS  Fixed a bugs
 *  09-Nov-2014  DVS  Fixed a bugs
 *  11-Nov-2014  DVS  Added card reader
 *  12-Nov-2014  DVS  Added card punch
 *  13-Nov-2014  DVS  Added line printer
 *  14-Nov-2014  DVS  Added drum
 *  17-Nov-2014  DVS  Cleanup and more debugging
 *  18-Nov-2014  DVS  Right cyclic_sum
 *  20-Nov-2014  DVS  Added CPU device modifiers
 *  22-Nov-2014  DVS  Fixed a square root operation error 
 *                    (forgetten exponent shift to left)
 *  30-Nov-2014  DVS  Fixed a square root operation error 
 *                    (forgetten exponent shift if exponent is odd)
 *  01-Dec-2014  DVS  Fixed a cdr boot error
 *  03-Dec-2014  DVS  Changed a cdr boot logic
 *  04-Dec-2014  DVS  Added debug dump for regs and mem
 *                    Fixed a problem with rouding of addition (my wrong!)
 *  05-Dec-2014  DVS  Minor fixes
 *  06-Dec-2014  DVS  Change logic for buffered print (update buffer only)
 *  07-Dec-2014  DVS  First version of new computer arithemetic because
 *                    even corrected arithmetic failed testing
 *  08-Dec-2014  DVS  New versions of ADD,SUB,SUB_MOD (passed all tests!)
 *                    (we need new MULT and DIV)
 *  12-Jan-2015  DVS  Fixed problems and bugs of addition arithmetic operation
 *                    SSR passed tests successfully at first time!
 *  13-Jan-2015  DVS  Added control for rounding 
 *                    Removed some code for readability
 *  21-Jan-2015  DVS  Returned back Shura-Bura and Starkman algorithm implementation
 *  23-Jan-2015  DVS  Checked operations times and conditions accoring M-20 technical description
 *  26-Jan-2015  DVS  Disabled workaround hack for BESM-4 opcode
 *  13-Feb-2015  DVS  Fixed a cyclic shift command implementation (op=67)
 *  27-Feb-2015  DVS  Removed auto skip zero address option
 *  06-Mar-2015  DVS  Added MOSU mode I and II, fixed cmd 020 error (must be used only 3 bits of A1)
 *  06-Mar-2015  DVS  Added statistics print on execution break
 *  09-Mar-2015  DVS  Some fixes and corrections for some instructions
 *                    But problems with arithmetic are remained, wee see these on complex tests
 *  14-Mar-2015  DVS  Cleanup code, removed numeric constants, added memory breakpoints
 *
 */

#include "m20_defs.h"
#include <math.h>
#include <float.h>


/* Register core */
uint16   regKRA;
uint16   regRA;
uint16   regSMA;
int      trgSW;
int      regROP;
t_value  regRK;
t_value  regRR;
t_value  regP1;
t_value  regP2;

int    old_opcode = 0;

/* control engineering panel */
t_value  RPU1 = 0;
//t_value  RPU1 = 0111000100010377;  /* just for testing */
//t_value  RPU1 = 0100000100010377;
t_value  RPU2 = 0;
t_value  RPU3 = 0;
t_value  RPU4 = 0;


/* card reader i/o addresses */ 
t_value cr_io_addr_1;
t_value cr_io_addr_2;
t_value cr_io_addr_3;


/* internal counters */
double delay;

/* special variable */




/*
 * External devices exchange parameters
 */

int ext_io_op;			/* CW       - conditional i/o word */
int ext_io_dev_zone_addr;	/* MSU_S    - starting address for drum/tape/print */
int ext_io_ram_start;		/* MOSU_S   - memory buffer starting address */
int ext_io_ram_end;		/* MOSE_E   - memory buffer ending address */
int ext_io_ram_jump;            /* MOSU_T   - memory address of control transfer if failed checksum */
int ext_io_ram_chksum;          /* MOSU_CHK - memory address for checksum writing */



/* MOSU - Magnetic Operating Storage Unit (main memory) */

t_value  MOSU[MAX_MEM_SIZE] = {0};


/* SIMH required declarations */

extern int32 sim_emax;

/* external devices */
extern t_stat read_card (t_value * csum, t_value * rsum, int * rcodes, 
                         int * stop_blocking, int * control_blocking);
extern t_stat punch_card (int start_addr, int end_addr, int zone_buf_addr, int add_only_flag,
                          int dis_mem_acc, int dis_chksum, int * ocodes, t_value *sum );
extern t_stat write_line_printer (int start_addr, int end_addr, int zone_buf_addr, int pr_type, 
                                  int add_only_flag, int dis_mem_acc, int dis_chksum, 
                                  int * ocodes );
extern t_stat drum_io (t_value * sum, int * ocodes);
//extern t_stat mt_format_tape(t_value *sum, int * ocodes);
extern t_stat mt_format_tape (t_value *sum, int * ocodes, int user_first, int user_last);
extern t_stat mt_tape_io(t_value *sum, int * ocodes);


/* SYS module references */

extern t_value ieee_to_m20 (double d);

extern const char *m20_opname [M20_SYM_OPCODE_TABLE_SIZE];
extern const char *m20_short_opname [M20_SYM_OPCODE_TABLE_SIZE];


t_value  cdr_csum;
t_value  cdr_rsum;
int      cdr_rcodes;
int      cdr_stop_blocking;
int      cdr_control_blocking;
int      boot_device_req_cdr = 0;
int      debug_dump_regs = 0;
int      debug_dump_mem = 0;
int      debug_dump_modern_mem = 0;
int      disable_is2_trace = 0;
int      arithmetic_op_debug = 0;
int      print_sys_stat = 1;
int      memory_45_checking = 1;

int      use_add_sbst = 0;
int      new_add = 0;
int      rounding_error_bits_off = 1;
int      rounding_up_on = 0;
int      new_mult = 0;
int      new_div = 0;
int      new_sqrt = 0;

static  int  enable_m20_print_ascii_text = 0;

int  diag_print = 0;
int  enable_opcode_040_hack = 0;

int  print_stat_on_break = 1;

int  run_mode = M20_AUTO_MODE;
int  mosu_mode = MOSU_MODE_I;

/* workaround for buffered print */
int active_lpt = 0;
int active_cdp = 0;


/* Functions */
t_stat cpu_examine (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_deposit (t_value val, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_reset (DEVICE *dptr);



/*
 * CPU data structures
 *
 * cpu_dev      CPU device descriptor
 * cpu_unit     CPU unit descriptor
 * cpu_reg      CPU register list
 * cpu_mod      CPU modifiers list
 */

UNIT cpu_unit = { UDATA (NULL, UNIT_FIX, MAX_MEM_SIZE) };


extern char  reg_rk_name[];
extern char  reg_ra_name[];
extern char  reg_rop_name[];
extern char  reg_kra_name[];
extern char  reg_sma_name[];
extern char  trg_sw_name[];
extern char  reg_rr_name[];
extern char  reg_rpu1_name[];
extern char  reg_rpu2_name[];
extern char  reg_rpu3_name[];
extern char  reg_rpu4_name[];

extern char  reg_rk_desc[];
extern char  reg_ra_desc[];
extern char  reg_rop_desc[];
extern char  reg_kra_desc[];
extern char  reg_sma_desc[];
extern char  trg_sw_desc[];
extern char  reg_rr_desc[];
extern char  reg_rpu1_desc[];
extern char  reg_rpu2_desc[];
extern char  reg_rpu3_desc[];
extern char  reg_rpu4_desc[];


REG cpu_reg[] = {
	{ reg_kra_name,  &regKRA,   8, 12, 0, 1, reg_kra_desc },	
	{ reg_rk_name,   &regRK,    8, 45, 0, 1, reg_rk_desc  },	
	{ reg_rop_name,  &regROP,   8,  6, 0, 1, reg_rop_desc },	
	{ reg_ra_name,   &regRA,    8, 12, 0, 1, reg_ra_desc  },	
	{ reg_sma_name,  &regSMA,   8, 12, 0, 1, reg_sma_desc },	
	{ trg_sw_name,	 &trgSW,    8, 1,  0, 1, trg_sw_desc  },
	{ reg_rr_name,   &regRR,    8, 45, 0, 1, reg_rr_desc  },
	{ reg_rpu1_name, &RPU1,     8, 45, 0, 1, reg_rpu1_desc },
	{ reg_rpu2_name, &RPU2,     8, 45, 0, 1, reg_rpu2_desc },
	{ reg_rpu3_name, &RPU3,     8, 45, 0, 1, reg_rpu3_desc },
	{ reg_rpu4_name, &RPU4,     8, 45, 0, 1, reg_rpu4_desc },
        { DRDATA (PRINT_STAT_ON_BREAK, print_stat_on_break, 8), PV_LEFT },
        { DRDATA (RUN_MODE, run_mode, 8), PV_LEFT },
        { DRDATA (MOSU_MODE, mosu_mode, 8), PV_LEFT },
        { DRDATA (PRINT_SYS_STAT, print_sys_stat, 8), PV_LEFT },
        { DRDATA (DIAG_PRINT, diag_print, 8), PV_LEFT },
        { DRDATA (DEBUG_DUMP_REGS, debug_dump_regs, 8), PV_LEFT },
        { DRDATA (DEBUG_DUMP_MEM,  debug_dump_mem,  8), PV_LEFT },
        { DRDATA (DEBUG_DUMP_MODERM_MEM, debug_dump_modern_mem,  8), PV_LEFT },
        { DRDATA (ENABLE_M20_PRINT_ASCII_TEXT, enable_m20_print_ascii_text, 8), PV_LEFT },
        { DRDATA (DISABLE_IS2_TRACE, disable_is2_trace, 8), PV_LEFT },
        { DRDATA (MEMORY_45_CHECKING, memory_45_checking, 8), PV_LEFT },
        { DRDATA (ENABLE_OPCODE_040_HACK, enable_opcode_040_hack, 8), PV_LEFT },
        { DRDATA (ARITHMETIC_OP_DEBUG, arithmetic_op_debug, 8), PV_LEFT },
        { DRDATA (ROUND_ERROR_BITS_OFF, rounding_error_bits_off, 8), PV_LEFT },
        { DRDATA (ROUNDING_UP_ON, rounding_up_on, 8), PV_LEFT },
        { DRDATA (USE_NEW_ADD, new_add, 8), PV_LEFT },
        { DRDATA (USE_NEW_MULT, new_mult, 8), PV_LEFT },
        { DRDATA (USE_NEW_DIV, new_div, 8), PV_LEFT },
        { DRDATA (USE_NEW_SQRT, new_sqrt, 8), PV_LEFT },
        { DRDATA (USE_ADD_SBST, use_add_sbst, 8), PV_LEFT },
	{ 0 }
};



MTAB cpu_mod[] = {
    { SHORT_SYM_OP, SHORT_SYM_OP, "short symbolic instruction name", "SHORT_SYM_OPCODE", NULL },
    { SHORT_SYM_OP, 0,            "long  symbolic instruction name", "LONG_SYM_OPCODE", NULL },
    { 0 }
};


DEVICE cpu_dev = {
	"CPU", &cpu_unit, cpu_reg, cpu_mod,
	1, 8, 12, 1, 8, 45,
	&cpu_examine, &cpu_deposit, &cpu_reset,
	NULL, NULL, NULL, NULL,
	DEV_DEBUG
};


typedef  struct command_profile_stat {
    int      op_code;
    double   us_count;
    double   us_time;
} COMMAND_PROFILE_STAT, * PCOMMAND_PROFILE_STAT;


COMMAND_PROFILE_STAT   cmd_profile_table[M20_SYM_OPCODE_TABLE_SIZE] = {
   {OPCODE_TRANSFER_MEM2MEM, 0, 0},
   {OPCODE_ADD_ROUND_NORM, 0, 0},
   {OPCODE_SUB_ROUND_NORM, 0, 0},
   {OPCODE_SUB_MOD_ROUND_NORM, 0, 0},
   {OPCODE_DIV_ROUND_NORM, 0, 0},
   {OPCODE_MULT_ROUND_NORM, 0, 0},
   {OPCODE_ADD_ADDR_TO_EXP, 0, 0},
   {OPCODE_ADD_CYCLIC, 0, 0},
   {OPCODE_INPUT_CODES_FROM_PUNCH_CARDS_WITH_STOP, 0, 0},
   {OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_1_011, 0, 0},
   {OPCODE_GOTO_AFTER_CYCLE_BY_PA_012, 0, 0},
   {OPCODE_ADD_CMDS, 0, 0},
   {OPCODE_SHIFT_MANTISSA_BY_ADDR, 0, 0},
   {OPCODE_COMPARE, 0, 0},
   {OPCODE_JUMP_WITH_RETURN, 0, 0},
   {OPCODE_STOP_017, 0, 0},
   {OPCODE_LOAD_FROM_KEY_REGISTER, 0, 0},
   {OPCODE_ADD_NORM, 0, 0},
   {OPCODE_SUB_NORM, 0, 0},
   {OPCODE_SUB_MOD_NORM, 0, 0},
   {OPCODE_DIV_NORM, 0, 0},
   {OPCODE_MULT_NORM, 0, 0},
   {OPCODE_ADD_EXP_TO_EXP, 0, 0},
   {OPCODE_SUB_CYCLIC, 0, 0},
   {OPCODE_INPUT_CODES_FROM_PUNCH_CARDS, 0, 0},
   {OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_1_031, 0, 0},
   {OPCODE_GOTO_AFTER_CYCLE_BY_PA_032, 0, 0},
   {OPCODE_SUB_CMDS, 0, 0},
   {OPCODE_SHIFT_MANTISSA_BY_EXP, 0, 0},
   {OPCODE_COMPARE_WITH_STOP, 0, 0},
   {OPCODE_COND_JUMP_BY_SIG_W_1, 0, 0},
   {OPCODE_STOP_037, 0, 0},
   {OPCODE_BLANKING_040, 0, 0},
   {OPCODE_ADD_ROUND, 0, 0},
   {OPCODE_SUB_ROUND, 0, 0},
   {OPCODE_SUB_MOD_ROUND, 0, 0},
   {OPCODE_SQRT_ROUND_NORM, 0, 0},
   {OPCODE_MULT_ROUND, 0, 0},
   {OPCODE_SUB_ADDR_FROM_EXP, 0, 0},
   {OPCODE_OUT_LOWER_BITS_OF_MULT, 0, 0},
   {OPCODE_IO_EXT_DEV_TO_MEM_050, 0, 0},
   {OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_0_051, 0, 0},
   {OPCODE_CHANGE_RA_BY_ADDR, 0, 0},
   {OPCODE_ADD_OPCS, 0, 0},
   {OPCODE_SHIFT_CODE_BY_ADDR, 0, 0},
   {OPCODE_LOGICAL_MULT, 0, 0},
   {OPCODE_JUMP_BY_ADDR, 0, 0},
   {OPCODE_STOP_057, 0, 0},
   {OPCODE_BLANKING_060, 0, 0},
   {OPCODE_ADD, 0, 0},
   {OPCODE_SUB, 0, 0},
   {OPCODE_SUB_MOD, 0, 0},
   {OPCODE_SQRT_NORM, 0, 0},
   {OPCODE_MULT, 0, 0},
   {OPCODE_SUB_EXP_FROM_EXP, 0, 0},
   {OPCODE_SHIFT_CYCLIC, 0, 0},
   {OPCODE_IO_EXT_DEV_TO_MEM_070, 0, 0},
   {OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_0_071, 0, 0},
   {OPCODE_CHANGE_RA_BY_CODE, 0, 0},
   {OPCODE_SUB_OPCS, 0, 0},
   {OPCODE_SHIFT_CODE_BY_EXP, 0, 0},
   {OPCODE_LOGICAL_ADD, 0, 0},
   {OPCODE_COND_JUMP_BY_SIG_W_0, 0, 0},
   {OPCODE_STOP_077, 0, 0}
};




/* Moderm floating math */
double m20_to_ieee (t_value w)
{
    double d;
    int exponent;

    //d = word & 0xfffffffffLL;
    d = (double)(w & 0xfffffffffLL);
    exponent = (w >> BITS_36) & 0x7f;
    d = ldexp (d, exponent - M20_MANTISSA_SHIFT - BITS_36);
    if ((w >> BITS_43) & 1) d = -d;

    return d;
}



/*
 *  Checksum calculation
 */

t_value  cyclic_checksum( t_value x, t_value y)
{
   t_value  t1,t2;
   t1 = (x & EXP_SIGN_TAG) + (y & EXP_SIGN_TAG);
   t2 = (x & MANTISSA) + (y & MANTISSA);
   if (t1 >= BIT46) { t1 -= BIT46; t1 += BIT37; }
   t1 &= WORD45;
   if (t2 >= BIT37) { t2 -= BIT37; t2 += 1; }
   t1 |= (t2 & MANTISSA);
   t1 &= WORD45;

   return t1;
}




/*
 * Memory examine implementaton
 */
t_stat cpu_examine (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw)
{
   t_value t;

   if (addr >= MAX_MEM_SIZE) return SCPE_NXM;

   if (vptr != NULL) *vptr = MOSU[addr];
   if (mosu_mode == MOSU_MODE_II) {
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+0) { t = 0;     *vptr=t; }
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+1) { t = RPU1;  *vptr=t; }
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+2) { t = RPU2;  *vptr=t; }
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+3) { t = RPU3;  *vptr=t; }
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+4) { t = RPU4;  *vptr=t; }
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+5) { t = regRR; *vptr=t; }
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+6) { t = 0;     *vptr=t; }
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+7) { t = 0;     *vptr=t; }
   }

   return SCPE_OK;
}



/*
 * Memory deposit
 */
t_stat cpu_deposit (t_value val, t_addr addr, UNIT *uptr, int32 sw)
{
   if (addr >= MAX_MEM_SIZE) return SCPE_NXM;

   /* Word by address 0 always contains 0. */
   //if (addr != 0) MOSU[addr] = val;
   if (addr == 0) return STOP_WRITE_TO_RO_MEM_LOC;

   if (mosu_mode == MOSU_MODE_II) {
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+0) return STOP_WRITE_TO_RO_MEM_LOC;
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+1) return STOP_WRITE_TO_RO_MEM_LOC;
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+2) return STOP_WRITE_TO_RO_MEM_LOC;
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+3) return STOP_WRITE_TO_RO_MEM_LOC;
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+4) return STOP_WRITE_TO_RO_MEM_LOC;
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+5) return STOP_WRITE_TO_RO_MEM_LOC;
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+6) return STOP_WRITE_TO_RO_MEM_LOC;
     if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+7) return STOP_WRITE_TO_RO_MEM_LOC;
   }

   MOSU[addr] = val;

   return SCPE_OK;
}



/*
 * Reset routine
 */
t_stat cpu_reset (DEVICE *dptr)
{
    if (sim_deb && cpu_dev.dctrl)
	fprintf (sim_deb, "cpu: reset\n" );

    //regRA   = 0;
    //regKRA  = 0;
    regSMA  = 0;
    //trgSW   = 0;
    regRK   = 0;
    regP1  = 0;
    regP2  = 0;
    ext_io_op = MAX_ADDR_VALUE;

    //sim_brk_types = (SWMASK('E') | SWMASK('I') | SWMASK('M'));
    //sim_brk_types = sim_brk_dflt = (SWMASK ('R') | SWMASK('W') | SWMASK('E'));
    sim_brk_types = (SWMASK('E')|SWMASK ('R')|SWMASK('W'));
    sim_brk_dflt = (SWMASK ('E'));

    //memset( MOSU, 0, sizeof(MOSU) );

    return SCPE_OK;
}




/*
 * Get codeword from memory core.
 */
t_value mosu_load (int addr)
{
    t_value val;
    //uint32  res32;

    addr &= MAX_ADDR_VALUE;
    //if (addr == 0) return 0;

    val = MOSU[addr];

    if (mosu_mode == MOSU_MODE_II) {
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+0) { val = 0;     }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+1) { val = RPU1;  }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+2) { val = RPU2;  }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+3) { val = RPU3;  }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+4) { val = RPU4;  }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+5) { val = regRR; }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+6) { val = 0;     }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+7) { val = 0;     }
    }

    return val;
}



/*
 * Put codeword into memory core
 */
void mosu_store (int addr, t_value val)
{
    addr &= MAX_ADDR_VALUE;
    if (addr == 0) return;

    if (mosu_mode == MOSU_MODE_II) {
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+0) { val = 0;     }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+1) { val = RPU1;  }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+2) { val = RPU2;  }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+3) { val = RPU3;  }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+4) { val = RPU4;  }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+5) { val = regRR; }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+6) { val = 0;     }
      if (addr == MOSU_MODE_II_SPEC_BASE_ADDR+7) { val = 0;     }
    }

    MOSU[addr] = val;
}



/*
 *  Test non-signed number for zero
 */
static int is_zero (t_value x)
{
    //x &= ~(TAG | SIGN);
    x &= WORD44;

    return (x == 0);
}



/*
 * Left common normalization
 */
t_value left_norm (t_value x)
{
    int exp_val;
    t_value m;

    exp_val = x >> BITS_36 & EXPONENT_VALUE_MASK;
    m = x & MANTISSA;

    if (m == 0) return (x & TAG);

    while(1) {
 	if (m & BIT36) break;
	m <<= 1;
	--exp_val;
	if (exp_val < 0) return (x & TAG);
    }

    x &= (TAG|SIGN);
    x |= ((t_value) exp_val << BITS_36) | m;

    return x;
}





/*
 * Add two numbers, using a blocking of rounding and blocking of normalization if required.
 */
t_stat addition (t_value *result, t_value x, t_value y, int no_round, int no_norm)
{
    int xexp, yexp, rexp;
    t_value xm, ym, r;

    if (arithmetic_op_debug) 
      fprintf( stderr, "ADD: ENTER: no_round=%d no_norm=%d x=%015llo y=%015llo\n", no_round, no_norm, x, y );

    if (is_zero (x)) {
	if (! no_norm) y = left_norm (y);
	*result = y | (x & TAG);
	return 0;
    }

    if (is_zero (y)) {
        if (! no_norm) x = left_norm (x);
	*result = x | (y & TAG);
	return 0;
    }

    /* Get exponent */
    xexp = x >> BITS_36 & EXPONENT_VALUE_MASK;
    yexp = y >> BITS_36 & EXPONENT_VALUE_MASK;

    if (arithmetic_op_debug) fprintf( stderr, "add: xexp=%d y_exp=%d\n", xexp, yexp );

    if (yexp > xexp) {
	/* Let x is greater value, and y is lesser value by modile. */
	t_value t = x;
	int texp = xexp;
	x = y;
	xexp = yexp;
	y = t;
	yexp = texp;
    }

    if (arithmetic_op_debug) fprintf( stderr, "add: xexp=%d y_exp=%d x=%015llo y=%015llo\n", xexp, yexp, x, y );

    if (xexp - yexp >= BITS_36) {      
 	/* Too small value  */
        if (! no_norm) x = left_norm (x);
	*result = x | (y & TAG);
        if (arithmetic_op_debug) fprintf( stderr, "add: LEAVE: FINAL 0: r=%015llo\n\n", *result );
	return 0;
    }

    /* Get mantissa */
    xm = x & MANTISSA;
    ym = (y & MANTISSA) >> (xexp - yexp);


    /* Add */
    rexp = xexp;
    if (arithmetic_op_debug) fprintf( stderr, "add: rexp=%d xm=%015llo ym=%015llo\n", rexp, xm, ym );

    if ((x ^ y) & SIGN) {
	/* Different signs */
	r = xm - ym;
        if (arithmetic_op_debug) fprintf( stderr, "add: A1: r=%015llo\n", r );
	if (r & SIGN) {
	    t_int64 r1;
	    r1 = r; r = -r1;
	    r |= SIGN;
            if (arithmetic_op_debug) fprintf( stderr, "add: A2: r=%015llo\n", r );
	}
    } else {
	/* Same signs */
	r = xm + ym;
        if (arithmetic_op_debug) fprintf( stderr, "add: B1: r=%015llo\n", r );
	if (! no_round) {
	   if ((xexp != yexp) && ((x & MANTISSA) && (y&MANTISSA))) {
	     /* Rounding */
	     r += 1;
             if (arithmetic_op_debug) fprintf( stderr, "add: B2: r=%015llo\n", r );
	   }	
	}
	if (r >> BITS_36) {
	    /* Out of 36 bits - do right normalization */
	    if (! no_round) {
		/* Rounding */
		r += 1; 
	    }
	    r >>= 1;
	    ++rexp;
            if (arithmetic_op_debug) fprintf( stderr, "add: C1: rexp=%d r=%015llo\n", rexp, r );
	    if (rexp > MAX_EXP_MACHINE_VAL) {
		/* Overflow on addition */
		return STOP_ADDOVF;
            }
	}
    }

    /* Check for special cases */
    if (arithmetic_op_debug) fprintf( stderr, "add: CHECK 1: rexp=%d r=%015llo\n", rexp, r );

    /* check for machine zero */
    if ((r == 0) || (rexp < 0)) {
      r = 0; rexp = 0;
      //goto make_result;
      goto done;
    }

    goto make_result;

    /* make a result */
make_result:
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 1: r=%015llo rexp=%d\n", r, rexp );

    r |= (t_value) rexp << BITS_36;
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 2: r=%015llo\n", r );

    r ^= (x & SIGN);
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 3: r=%015llo\n", r );

    if (! no_norm) r = left_norm (r);
    if (arithmetic_op_debug) fprintf( stderr, "add: LEAVE: FINAL 10: r=%015llo\n\n", r );

    if (rounding_error_bits_off && !no_round) {
      t_value t;
      t = r & MANTISSA;
      //fprintf( stderr, "add: FINAL TEMP: t1=%015llo, t2=%015llo\n", (BIT36|BIT01), ~(BIT36|BIT01) );
      if ((t & BIT36) && (t & BIT01) && (((t & ~(BIT36|BIT01)) & MANTISSA) == 0)) { 
        if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 13: r=%018llo\n", r );
        r &= ~1; r &= WORD45; 
        if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 14: r=%018llo\n", r );
      }
    }

done:
    *result = r | ((x | y) & TAG);
    if (arithmetic_op_debug) fprintf( stderr, "add: LEAVE: FINAL 20: r=%015llo\n\n", r );

    return 0;
}



/*
 * Add value to exponent
 */
t_stat add_exponent (t_value *result, t_value x, int n, int opcode)
{
    int exp_val;

    exp_val = (int) (x >> BITS_36 & EXPONENT_VALUE_MASK) + n;

    if ((exp_val > MAX_EXP_MACHINE_VAL) && (x & MANTISSA) == 0) {
      x = TAG;
      *result = x;
      return 0;
    }

    if ((exp_val < 0) || (x & MANTISSA) == 0) {
      /* Zero */
      x = TAG;
      *result = x;
      return 0;
    }

    if (exp_val > MAX_EXP_MACHINE_VAL) {
	/* Overflow on exponents addition */
	return STOP_EXPOVF;
    }

    //*result = x;
    *result = (x&SIGN) | (x&MANTISSA) | (x&TAG) | ((t_value)exp_val << BITS_36);

    return 0;
}


/*
 * Add two 36-bit values, output two parts of results.
 */
void mul36x36 (t_value x, t_value y, t_value *hi, t_value *lo)
{
    int yhi, ylo;
    t_value rhi, rlo;

    /* Split 2nd multiplier into 2 parts */
    //yhi = y >> 18;
    yhi = (int)(y >> BITS_18);
    ylo = y & WORD18;

    /* Partial 54-bit products */
    rhi = x * yhi;
    rlo = x * ylo;

    /* Make results */
    rhi += rlo >> BITS_18;
    *hi = rhi >> BITS_18;
    *lo = (rhi & WORD18) << BITS_18 | (rlo & WORD18);
}



/*
 * Multiply two numbers, using a blocking of rounding and blocking of normalization if required.
 */
t_stat multiplication (t_value *result, t_value x, t_value y, int no_round, int no_norm)
{
    int xexp, yexp, rexp;
    t_value xm, ym, r;

    if (arithmetic_op_debug) 
      fprintf( stderr, "MULT: ENTER: x=%015llo, y=%015llo no_round=%d no_norm=%d\n", x, y, no_round, no_norm );

    /* Get exponent */
    xexp = x >> BITS_36 & EXPONENT_VALUE_MASK;
    yexp = y >> BITS_36 & EXPONENT_VALUE_MASK;

    /* get mantissa */
    xm = x & MANTISSA;
    ym = y & MANTISSA;

    if (arithmetic_op_debug) 
      fprintf( stderr, "mult: xm=%015llo, ym=%015llo xexp=%d yexp=%d\n", xm, ym, xexp, yexp );

    /* Multiply */
    rexp = xexp + yexp - M20_MANTISSA_SHIFT;
    mul36x36 (xm, ym, &r, &regP1);

    if (arithmetic_op_debug) fprintf( stderr, "mult: r=%015llo, regP1=%015llo rexp=%d\n", r, regP1, rexp );

    if (! no_norm && !(r & 0400000000000LL)) {
	/* Left normalization */
	--rexp;
	r <<= 1;
	regP1 <<= 1;
	if (regP1 & BIT37) {
 	    r |= 1; 
	    //regP1 &= MANTISSA;
	}
       if (arithmetic_op_debug) fprintf( stderr, "mult: NORM_DONE: r=%015llo, regP1=%015llo rexp=%d\n", r, regP1, rexp );
    }
   if (! no_round) {
	/* Rounding */
	if (regP1 & 0400000000000LL) {
	    r += 1;
	}
       if (arithmetic_op_debug) fprintf( stderr, "mult: ROUND_DONE: r=%015llo, regP1=%015llo rexp=%d\n", r, regP1, rexp );
    }

    if (arithmetic_op_debug) fprintf( stderr, "mult: r=%015llo, regP1=%015llo rexp=%d\n", r, regP1, rexp );

    if ((r == 0) || (rexp < 0)) {
	/* Zero */
	*result = (x | y) & TAG;
        if (arithmetic_op_debug) 
          fprintf( stderr, "mult: result=%015llo, regP1=%015llo rexp=%d\n\n", *result, regP1, rexp );
	return 0;
    }

    if (rexp > MAX_EXP_MACHINE_VAL) {
	/* Overflow on multiply */
	return STOP_MULOVF;
    }

    /* Make result */
    if (arithmetic_op_debug) fprintf( stderr, "mult: FINAL 1: r=%015llo, rexp=%d\n", r, rexp );

     r |= (t_value) rexp << BITS_36;
     r |= ((x ^ y) & SIGN) | ((x | y) & TAG);

     regP1 &= MANTISSA;
     if (arithmetic_op_debug) 
       fprintf( stderr, "mult: FINAL 2: r=%015llo, regP1=%015llo rexp=%d\n", r, regP1, rexp );
     regP1 |= (t_value) rexp << BITS_36;
     regP1 |= ((x ^ y) & SIGN) | ((x | y) & TAG);

     if (arithmetic_op_debug) 
       fprintf( stderr, "mult: FINAL 3: r=%015llo, regP1=%015llo rexp=%d\n\n", r, regP1, rexp );

     *result = r;

     return 0;
}



/*
 * Division of two numbers, using a blocking of rounding and blocking of normalization if required.
 */
t_stat division (t_value *result, t_value x, t_value y, int no_round)
{
    int xexp, yexp, rexp;
    t_value xm, ym, r;

    /* Get exponent */
    xexp = x >> BITS_36 & EXPONENT_VALUE_MASK;
    yexp = y >> BITS_36 & EXPONENT_VALUE_MASK;

    /* Get mantissa */
    xm = x & MANTISSA;
    ym = y & MANTISSA;
    if (xm >= 2*ym) {
 	/* Overflow on division (mantissa) */
	return STOP_DIVMOVF;
    }

    /* Divide */
    rexp = xexp - yexp + M20_MANTISSA_SHIFT;
    r = (t_value)((double) xm / ym * BIT37);

    if (r >> BITS_36) {
	/* Out of 36-bits, do a right normalization */
	if (! no_round) {
  	    /* Rounding */
	    r += 1;
	}
	r >>= 1;
	++rexp;
    }

    if ((r == 0) || (rexp < 0)) {
	/* Zero */
	*result = (x | y) & TAG;
	return 0;
    }

    if (rexp > MAX_EXP_MACHINE_VAL) {
	/* Overflow on division (exponent) */
	return STOP_DIVOVF;
    }

    /* Make result */
    r |= (t_value) rexp << BITS_36;
    r |= ((x ^ y) & SIGN) | ((x | y) & TAG);

    *result = r;

    return 0;
}




/*
 * Square root calculation, using a blocking of rounding if required.
 */
t_stat square_root (t_value *result, t_value x, int no_round)
{
    int exponent;
    int exp_shift = 0;
    t_value r;
    double q;

    if (x & SIGN) {
	/* Negative number */
	return STOP_NEGSQRT;
    }

    /* Get exponent */
    exponent = x >> BITS_36 & EXPONENT_VALUE_MASK;

    /* Get mantissa */
    r = x & MANTISSA;

    /* Calculate SQRT */
    if (exponent & 1) {
	/* Odd order */
	r >>= 1;
        exp_shift = 1;
    }

    exponent = (exponent >> 1) + (M20_MANTISSA_SHIFT/2);
    q = sqrt ((double) r) * BIT19;
    r = (t_value) q;
    if (! no_round) {
	/* Check a remainder */
	if (q - r >= 0.5) {
		/* Rounding */
		r += 1;
	}
    }

    if (r == 0) {
	/* Zero */
	*result = x & TAG;
	return 0;
    }

    if (r & ~MANTISSA) {
	/* SQRT calculation error */
	return STOP_SQRTERR;
    }

    /* Make result */
    r |= ((t_value)exponent+exp_shift) << BITS_36;
    r |= x & TAG;

    *result = r;

    return 0;
}




/*
 *  New arithmetic operations implementations
 *  (used shura-bura and other sources)
 */




/*
 * Two numbers addition. If required then blocking of rounding and normalization.
 */
t_stat new_addition (t_value *result, t_value x, t_value y, int no_round, int no_norm)
{
    int xexp, yexp, rexp, texp, fix_sign;
    t_value xm, ym, r, xm1, ym1, t;

    r = 0;
    if (arithmetic_op_debug) 
      fprintf( stderr, "NEW ADD: no_round=%d no_norm=%d x=%015llo y=%015llo\n", no_round, no_norm, x, y );

    /* Get exponent */
    xexp = x >> BITS_36 & EXPONENT_VALUE_MASK;
    yexp = y >> BITS_36 & EXPONENT_VALUE_MASK;

    if (arithmetic_op_debug) fprintf( stderr, "add: xexp=%d y_exp=%d\n", xexp, yexp );

    if (yexp > xexp) {
	/* Assume always that x > y */
	t = x; texp = xexp;
	x = y; xexp = yexp;
	y = t; yexp = texp;
    }

    /* Get mantissa */
    xm = x & MANTISSA;
    ym = y & MANTISSA;

    xm1 = xm << 1;
    ym1 = ym << 1;

    if (arithmetic_op_debug) fprintf( stderr, "add: xm=%015llo ym=%015llo xm1=%018llo ym1=%018llo\n", xm, ym, xm1, ym1 );

    /* Mantissa alignment */
    ym1 >>= (xexp - yexp);

    /* Addition */
    rexp = xexp;
    if (arithmetic_op_debug) fprintf( stderr, "add: rexp=%d xm1=%018llo ym1=%018llo\n", rexp, xm1, ym1 );

    /* Opposite signs? */
    if ((x ^ y) & SIGN) {
	r = xm1 - ym1;
        if (arithmetic_op_debug) fprintf( stderr, "add: A1: r=%018llo\n", r );
	if (r & (SIGN<<1)) {
	    t_int64 r1;
	    r1 = r; r = -r1;
	    r |= (SIGN<<1);
            if (arithmetic_op_debug) fprintf( stderr, "add: A2: r=%015llo\n", r );
	}
    }


    /* Same signs? */
    if (((x ^ y) & SIGN) == 0) {
	r = xm1 + ym1;
        if (arithmetic_op_debug) fprintf( stderr, "add: B1: r=%018llo\n", r );
	if (!no_round) {
           if (arithmetic_op_debug) fprintf( stderr, "add: B2: r=%018llo\n", r );
           if ((xexp != yexp) && ((xm1 & MANTISSA<<1) && (ym1 & MANTISSA<<1))) {
	     /* Rounding */
	     r += 1;
             if (arithmetic_op_debug) fprintf( stderr, "add: B3: r=%018llo\n", r );
             if (rounding_up_on) {
               if (r & 3) {
                 if (arithmetic_op_debug) fprintf( stderr, "add: B4: r=%018llo\n", r );
                 r += 1;
               }
             }
	   }	
	}
    }

    /* normalization to right */
    if (r & (BIT37<<1)) {
        if (arithmetic_op_debug) fprintf( stderr, "add: C1: rexp=%d r=%018llo\n", rexp, r );
        r >>= 1;
	++rexp;
        if (arithmetic_op_debug) fprintf( stderr, "add: C2: rexp=%d r=%018llo\n", rexp, r );
	if (rexp > MAX_EXP_MACHINE_VAL) {
	    /* addition overflow  */
	    return STOP_ADDOVF;
        }
        if (arithmetic_op_debug) fprintf( stderr, "add: C3: rexp=%d r=%018llo\n", rexp, r );
    }

    /* normalization to left */
    if (!no_norm) {
        if (arithmetic_op_debug) fprintf( stderr, "add: D1: rexp=%d r=%018llo\n", rexp, r );
        while(1) {
           if (r == 0) {
             /* Zero mantissa - make a null */
	     break;
           }
 	   if (r & BIT37)  break;
 	   fix_sign = 0;
 	   if (r & (SIGN<<1)) fix_sign =1 ;
	   r <<= 1;
	   r &= WORD45;
	   if (fix_sign) r |= SIGN<<1;
	   --rexp;
           if (arithmetic_op_debug) fprintf( stderr, "add: D5: rexp=%d r=%018llo\n", rexp, r );
	   if (rexp < 0) break;
        }
        if (arithmetic_op_debug) fprintf( stderr, "add: D9: rexp=%d r=%018llo\n", rexp, r );
    }

    if (arithmetic_op_debug) fprintf( stderr, "add: E1: rexp=%d r=%018llo\n", rexp, r );

    r >>= 1;
    if (arithmetic_op_debug) fprintf( stderr, "add: E3: rexp=%d r=%018llo\n", rexp, r );

    /* check for machine zero */
    if ((r == 0) || (rexp < 0)) {
      r = 0; rexp = 0;
      goto make_result;
    }

    /* Make final result. */
  make_result:
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 10: r=%018llo\n", r );

    r |= (t_value) rexp << BITS_36;
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 11: r=%018llo\n", r );

    r ^= (x & SIGN);   /* sign of bigger number */
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 12: r=%018llo\n", r );

    if (rounding_error_bits_off && !no_round) {
      t = r & MANTISSA;
      //fprintf( stderr, "add: FINAL TEMP: t1=%015llo, t2=%015llo\n", (BIT36|BIT01), ~(BIT36|BIT01) );
      if ((t & BIT36) && (t & BIT01) && (((t & ~(BIT36|BIT01)) & MANTISSA) == 0)) { 
        if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 13: r=%018llo\n", r );
        r &= ~1; r &= WORD45; 
        if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 14: r=%018llo\n", r );
      }
    }

    *result = r | ((x | y) & TAG);
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 15: r=%018llo\n\n", r );

    return SCPE_OK;
}



/*
 * Two numbers addition. If required then blocking of rounding/normalization.
 */
t_stat new_addition_v20 (t_value *result, t_value x, t_value y, int no_round, int no_norm)
{
    int xexp, yexp, rexp, texp, fix_sign, r_bit;
    t_value xm, ym, r, xm1, ym1, t;

    r = 0;
    if (arithmetic_op_debug) 
      fprintf( stderr, "NEW ADD v20: no_round=%d no_norm=%d x=%015llo y=%015llo\n", no_round, no_norm, x, y );

    /* Get exponent */
    xexp = x >> BITS_36 & EXPONENT_VALUE_MASK;
    yexp = y >> BITS_36 & EXPONENT_VALUE_MASK;

    if (arithmetic_op_debug) fprintf( stderr, "add: xexp=%d y_exp=%d\n", xexp, yexp );

    if (yexp > xexp) {
	/* Assume always that x > y */
	t = x; texp = xexp;
	x = y; xexp = yexp;
	y = t; yexp = texp;
    }

    /* Get mantissa */
    xm = x & MANTISSA;
    ym = y & MANTISSA;

    xm1 = xm << 1;
    ym1 = ym << 1;

    if (arithmetic_op_debug) fprintf( stderr, "add: xm=%015llo ym=%015llo xm1=%018llo ym1=%018llo\n", xm, ym, xm1, ym1 );

    if (!no_round && (((x ^ y) & SIGN) == 0)) {
        if ((xexp != yexp) && ((xm1 & MANTISSA<<1) && (ym1 & MANTISSA<<1))) {
          xm1 |= 1;
          ym1 |= 1;
          if (arithmetic_op_debug) fprintf( stderr, "add: ROUND: xm=%015llo ym=%015llo xm1=%018llo ym1=%018llo\n", xm, ym, xm1, ym1 );
       }
    }

    //if (arithmetic_op_debug) fprintf( stderr, "add: xm=%015llo ym=%015llo xm1=%018llo ym1=%018llo\n", xm, ym, xm1, ym1 );

    /* Mantissa alignment */
    ym1 >>= (xexp - yexp);

    /* Addition */
    rexp = xexp;
    if (arithmetic_op_debug) fprintf( stderr, "add: rexp=%d xm1=%018llo ym1=%018llo\n", rexp, xm1, ym1 );

    /* Opposite signs? */
    if ((x ^ y) & SIGN) {
	r = xm1 - ym1;
        if (arithmetic_op_debug) fprintf( stderr, "add: A1: SUM: r=%018llo\n", r );
	if (r & (SIGN<<1)) {
	    t_int64 r1;
	    r1 = r; r = -r1;
	    r |= (SIGN<<1);
            if (arithmetic_op_debug) fprintf( stderr, "add: A2: SUM: r=%015llo\n", r );
	}
    }


    /* Same signs? */
    if (((x ^ y) & SIGN) == 0) {
	r = xm1 + ym1;
        if (arithmetic_op_debug) fprintf( stderr, "add: B1: SUM: r=%018llo\n", r );
#if 0
	if (!no_round) {
           if (arithmetic_op_debug) fprintf( stderr, "add: B2: r=%018llo\n", r );
           if ((xexp != yexp) && ((xm1 & MANTISSA<<1) && (ym1 & MANTISSA<<1))) {
	     /* Rounding */
	     r += 1;
             if (arithmetic_op_debug) fprintf( stderr, "add: B3: r=%018llo\n", r );
             if (rounding_up_on) {
               if (r & 3) {
                 if (arithmetic_op_debug) fprintf( stderr, "add: B4: r=%018llo\n", r );
                 r += 1;
               }
             }
	   }	
	}
#endif
    }

    /* normalization to right */
    if (r & (BIT37<<1)) {
        if (arithmetic_op_debug) fprintf( stderr, "add: C1: NORM_R: rexp=%d r=%018llo\n", rexp, r );
        r_bit = r & 1;
        r >>= 1;
        if (!no_round && r_bit) { 
           r += 1;
           if (arithmetic_op_debug) fprintf( stderr, "add: C3: ROUND: rexp=%d r=%018llo\n", rexp, r );
        }
	++rexp;
        if (arithmetic_op_debug) fprintf( stderr, "add: C5: rexp=%d r=%018llo\n", rexp, r );
	if (rexp > MAX_EXP_MACHINE_VAL) {
	    /* addition overflow  */
	    return STOP_ADDOVF;
        }
        if (arithmetic_op_debug) fprintf( stderr, "add: C8: rexp=%d r=%018llo\n", rexp, r );
    }

    /* normalization to left */
    if (!no_norm) {
        if (arithmetic_op_debug) fprintf( stderr, "add: D1: NORM_L: rexp=%d r=%018llo\n", rexp, r );
        while(1) {
           if (r == 0) {
             /* Zero mantissa - make a null */
	     break;
           }
 	   if (r & BIT37)  break;
 	   fix_sign = 0;
 	   if (r & (SIGN<<1)) fix_sign =1 ;
	   r <<= 1;
	   r &= WORD45;
	   if (fix_sign) r |= SIGN<<1;
	   --rexp;
           if (arithmetic_op_debug) fprintf( stderr, "add: D5: rexp=%d r=%018llo\n", rexp, r );
	   if (rexp < 0) break;
        }
        if (arithmetic_op_debug) fprintf( stderr, "add: D9: rexp=%d r=%018llo\n", rexp, r );
    }

    if (arithmetic_op_debug) fprintf( stderr, "add: E1: rexp=%d r=%018llo\n", rexp, r );

    r >>= 1;
    if (arithmetic_op_debug) fprintf( stderr, "add: E3: rexp=%d r=%018llo\n", rexp, r );

    /* check for machine zero */
    if ((r == 0) || (rexp < 0)) {
      r = 0; rexp = 0;
      goto make_result;
    }

    /* Make final result. */
  make_result:
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 10: r=%018llo\n", r );

    r |= (t_value) rexp << BITS_36;
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 11: r=%018llo\n", r );

    r ^= (x & SIGN);   /* sign of bigger number */
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 12: r=%018llo\n", r );

#if 0
    if (rounding_error_bits_off && !no_round) {
      t = r & MANTISSA;
      //fprintf( stderr, "add: FINAL TEMP: t1=%015llo, t2=%015llo\n", (BIT36|BIT01), ~(BIT36|BIT01) );
      if ((t & BIT36) && (t & BIT01) && (((t & ~(BIT36|BIT01)) & MANTISSA) == 0)) { 
        if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 13: r=%018llo\n", r );
        r &= ~1; r &= WORD45; 
        if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 14: r=%018llo\n", r );
      }
    }
#endif

    *result = r | ((x | y) & TAG);
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 15: r=%018llo\n\n", r );

    return SCPE_OK;
}



/*
 * Two numbers addition. If required then blocking of rounding/normalization.
 */
t_stat new_addition_v44 (t_value *result, t_value x, t_value y, int no_round, int no_norm)
{
    int xexp, yexp, rexp, texp, fix_sign, xs, ys, rs, r_bit, shift_count, delta_exp;
    t_value r, t;
    t_int64 xm, ym, xm1, ym1, rr;

    r = 0;
    if (arithmetic_op_debug) 
      fprintf( stderr, "NEW ADD 44: no_round=%d no_norm=%d x=%015llo y=%015llo\n", no_round, no_norm, x, y );

    /* Get exponent */
    xexp = x >> BITS_36 & EXPONENT_VALUE_MASK;
    yexp = y >> BITS_36 & EXPONENT_VALUE_MASK;

    if (arithmetic_op_debug) fprintf( stderr, "add: xexp=%d y_exp=%d\n", xexp, yexp );

    if (yexp > xexp) {
	/* Assume always that x > y */
	t = x; texp = xexp;
	x = y; xexp = yexp;
	y = t; yexp = texp;
    }

    /* Get mantissa */
    xm = x & MANTISSA;
    ym = y & MANTISSA;

    xs = ys = 1;
    if (x & SIGN) xs = -1;
    if (y & SIGN) ys = -1;

    xm1 = (xs * xm) << 1;
    ym1 = (ys * ym) << 1;

    if (arithmetic_op_debug) fprintf( stderr, "add: xm=%015llo ym=%015llo xm1=%018llo ym1=%018llo\n", xm, ym, xm1, ym1 );

    if (!no_round && (((x ^ y) & SIGN) == 0)) {
        if ((xexp != yexp) && ((xm1 & MANTISSA<<1) && (ym1 & MANTISSA<<1))) {
          xm1 |= 1;
          ym1 |= 1;
          if (arithmetic_op_debug) fprintf( stderr, "add: ROUND: xm=%015llo ym=%015llo xm1=%018llo ym1=%018llo\n", xm, ym, xm1, ym1 );
       }
    }

    delta_exp = xexp - yexp;
    if (arithmetic_op_debug) fprintf( stderr, "add: delta_exp=%d xm1=%018llo ym1=%018llo\n", delta_exp, xm1, ym1 );

    /* Mantissa alignment */
    if (delta_exp > 0) ym1 >>= (xexp - yexp);
    else if (delta_exp < 0) xm1 >>= (xexp - yexp);

    /* Addition */
    rexp = xexp;
    if (arithmetic_op_debug) fprintf( stderr, "add: rexp=%d xm1=%018llo ym1=%018llo\n", rexp, xm1, ym1 );

    rr = xm1 + ym1;
    if (arithmetic_op_debug) fprintf( stderr, "add: rr=%018llo\n", rr );
    rs = 1;
    if (rr < 0) {
      rs = -1;
      rr = rr * rs;
    }
    if (arithmetic_op_debug) fprintf( stderr, "add: rr=%018llo\n", rr );
    r = (rr & (MANTISSA|BIT37|BIT38));
    if (arithmetic_op_debug) fprintf( stderr, "add: rr=%018llo\n", rr );

    r_bit = 0;
    shift_count = 0;

    // here must be rouding

    /* normalization to right */
    if (r & (BIT37<<1)) {
        if (arithmetic_op_debug) fprintf( stderr, "add: C1: NORM_R: rexp=%d r=%018llo\n", rexp, r );
        r_bit = r & 1;
        if (!no_round && r_bit) { 
           r += 1;
           if (arithmetic_op_debug) fprintf( stderr, "add: C3: ROUND: rexp=%d r=%018llo\n", rexp, r );
        }
        r >>= 1;
        rexp++;
        if (arithmetic_op_debug) fprintf( stderr, "add: C5: rexp=%d r=%018llo\n", rexp, r );
	if (rexp > MAX_EXP_MACHINE_VAL) {
	    /* addition overflow  */
	    return STOP_ADDOVF;
        }
        if (arithmetic_op_debug) fprintf( stderr, "add: C7: rexp=%d r=%018llo\n", rexp, r );
    }


    /* normalization to left */
    if (!no_norm) {
        if (arithmetic_op_debug) fprintf( stderr, "add: D1: NORM_L: rexp=%d r=%018llo\n", rexp, r );
        while(1) {
           if (r == 0) {
             /* Zero mantissa - make a null */
	     break;
           }
 	   if (r & BIT37)  break;
 	   fix_sign = 0;
 	   if (r & (SIGN<<1)) fix_sign =1 ;
	   r <<= 1;
	   r &= WORD45;
	   if (fix_sign) r |= SIGN<<1;
	   --rexp;
           if (arithmetic_op_debug) fprintf( stderr, "add: D5: rexp=%d r=%018llo\n", rexp, r );
	   if (rexp < 0) break;
        }
        if (arithmetic_op_debug) fprintf( stderr, "add: D9: rexp=%d r=%018llo\n", rexp, r );
    }

    if (arithmetic_op_debug) fprintf( stderr, "add: E1: rexp=%d r=%018llo\n", rexp, r );

    r >>= 1;
    if (arithmetic_op_debug) fprintf( stderr, "add: E3: rexp=%d r=%018llo\n", rexp, r );

    /* check for machine zero */
    if ((r == 0) || (rexp < 0)) {
      r = 0; rexp = 0;
      goto make_result;
    }

    /* Make final result. */
  make_result:
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 10: r=%018llo\n", r );

    r |= (t_value) rexp << BITS_36;
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 11: r=%018llo\n", r );

    //r ^= (x & SIGN);   /* sign of bigger number */
    if (rs < 0) r |= SIGN;
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 12: r=%018llo\n", r );

#if 0
    if (rounding_error_bits_off) {
      t = r & MANTISSA;
      //fprintf( stderr, "add: FINAL TEMP: t1=%015llo, t2=%015llo\n", (BIT36|BIT01), ~(BIT36|BIT01) );
      if ((t & BIT36) && (t & BIT01) && (((t & ~(BIT36|BIT01)) & MANTISSA) == 0)) { 
        if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 13: r=%018llo\n", r );
        r &= ~1; r &= WORD45; 
        if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 14: r=%018llo\n", r );
      }
    }
#endif

    *result = r | ((x | y) & TAG);
    if (arithmetic_op_debug) fprintf( stderr, "add: FINAL 15: r=%018llo\n\n", r );

    return SCPE_OK;
}







#define  AUX_BIT_SHIFT     1


static int  get_number_sign( t_value num )
{
  return( num & SIGN ? -1 : 1 );
}


static int  is_norm_zero( t_value num )
{
    if ((num & SIGN) && ((num & MANTISSA) == 0) && ((num & EXPONENT) == 0)) 
        return 1;
    else
        return 0;
}   


static t_value  norm_zero( void )
{
  t_value t;

  t = 0 | ((t_value)0 << BITS_36);

  return ( t );
}


#define  MATH_OP_ADD       1
#define  MATH_OP_SUB       2
#define  MATH_OP_SUB_MOD   3

#if !defined(max) 
t_value max(t_value a,t_value b)
{
  if (a>b)return a;
  else return b;
}
#endif

/*
 *  Addition,subtraction,subtraction by module arithmetic operations implementation.
 *  According this book: Shura-Bura,Starkman pp. 70-75
 *  (russian edition, 1962)
 */
t_stat  new_arithmetic_op( t_value * result, t_value x, t_value y, int op_code )
{
   int math_op_type = 0, rr, e0, n0, j;
   int u, u1, sigma, sigma1, p, q, beta_round, beta_norm, rr_shift;
   int sign1, sign2, sign_x, sign_y, sign_z, v, v1, v2, delta, delta1, ro;
   t_value x1,y1, xx1, yy1, z, t, mask;

   if (result == NULL) return STOP_INVARG;

   if (arithmetic_op_debug) fprintf( stderr, "op=%02o, x=%015llo, y=%015llo\n", op_code, x, y );

   /* STEP 0. Prepare source data */

   switch( op_code) {
	case OPCODE_ADD_ROUND_NORM:         /* 001 = сложение с округлением и нормализацией */
	case OPCODE_ADD_NORM:               /* 021 = сложение без округления с нормализацией */
	case OPCODE_ADD_ROUND:              /* 041 = сложение с округлением без нормализации */
	case OPCODE_ADD:                    /* 061 = сложение без округления и без нормализации */
            math_op_type = MATH_OP_ADD;
	    break;
	case OPCODE_SUB_ROUND_NORM:         /* 002 = вычитание с округлением и нормализацией */
	case OPCODE_SUB_NORM:               /* 022 = вычитание без округления с нормализацией */
	case OPCODE_SUB_ROUND:              /* 042 = вычитание с округлением без нормализации */
	case OPCODE_SUB:                    /* 062 = вычитание без округления и без нормализации */
            math_op_type = MATH_OP_SUB;
	    break;
	case OPCODE_SUB_MOD_ROUND_NORM:     /* 003 = вычитание модулей с округлением и нормализацией */
	case OPCODE_SUB_MOD_NORM:           /* 023 = вычитание модулей без округления с нормализацией */
	case OPCODE_SUB_MOD_ROUND:          /* 043 = вычитание модулей с округлением без нормализации */
	case OPCODE_SUB_MOD:                /* 063 = вычитание модулей без округления и без нормализации */
            math_op_type = MATH_OP_SUB_MOD;
	    break;
	default:
            math_op_type = -1;
	    break;
   }

   if (math_op_type < 0) return STOP_INVARG;

   if (math_op_type == MATH_OP_SUB_MOD) {
       u = -1; u1 = 1;
   }

   if (math_op_type == MATH_OP_SUB) {
       sign1 = get_number_sign(x);
       sign2 = get_number_sign(y);
       u1 = sign1;
       u = (-1)*(sign1 * sign2);
   }

   if (math_op_type == MATH_OP_ADD) {
       sign1 = get_number_sign(x);
       sign2 = get_number_sign(y);
       u1 = sign1;
       u = (sign1 * sign2);
   }

   sign_x = get_number_sign(x);
   sign_y = get_number_sign(y);

   if (arithmetic_op_debug) 
     fprintf( stderr, "math_op_type=%d u=%d u1=%d sign_x=%d sign_y=%d x=%015llo y=%015llo\n", 
                       math_op_type, u, u1, sign_x, sign_y, x, y );

   p = (x & EXPONENT) >> BITS_36;
   q = (y & EXPONENT) >> BITS_36;

   x1 = (x & MANTISSA);
   y1 = (y & MANTISSA);

   beta_round = (op_code >> 4 & 1);
   beta_norm = (op_code >> 5 & 1);

   if (arithmetic_op_debug) 
     fprintf( stderr, "p=%d q=%d round=%d norm=%d x1=%015llo y1=%015llo\n", p,q,beta_round,beta_norm,x1,y1 );

   /* STEP 1. Preliminary analysis */

   if (u == -1) sigma = 1;
   if (u1 == -1) sigma1 = 1;
   if (u == 1) sigma = 0;
   if (u1 == 1) sigma1 = 0;

   if (is_norm_zero(x)) v1=1;
   else v1 = 0;

   if (is_norm_zero(y)) v2=1;
   else v2 = 0;

   v = v1 || v2;

   delta = p - q;
   delta1 = (delta == 0);
   ro = (delta <= 0);

   if (arithmetic_op_debug) 
     fprintf( stderr, "sigma=%d sigma1=%d v1=%d v2=%d v=%d delta=%d delta1=%d ro=%d\n", 
                       sigma, sigma1, v1, v2, v, delta, delta1, ro );


   /* STEP 2. Exponent alignment and get preliminary result */

   rr = max(p,q);

   n0 = (!beta_round)*(!v)*(!sigma)*(!delta1);
   e0 = n0;

   fprintf( stderr, "rr=%d n0=%d e0=%d\n", rr, n0, e0 );

   if (delta == 0) {
     xx1 = x1 << AUX_BIT_SHIFT;
     yy1 = y1 << AUX_BIT_SHIFT;
     if (arithmetic_op_debug) fprintf( stderr, "DELTA==0: xx1=%015llo yy1=%015llo\n", xx1, yy1 );
   }

   if (delta < 0) {
     xx1 = (x1 << AUX_BIT_SHIFT) >> -delta;
     yy1 = y1 << AUX_BIT_SHIFT;
     yy1 |= n0;
     if (arithmetic_op_debug) fprintf( stderr, "DELTA<0: xx1=%015llo yy1=%015llo\n", xx1, yy1 );
   }

   if (delta > 0) {
     xx1 = x1 << AUX_BIT_SHIFT;
     xx1 |= e0;
     yy1 = (y1 << AUX_BIT_SHIFT) >> delta;
     if (arithmetic_op_debug) fprintf( stderr, "DELTA>0: xx1=%015llo yy1=%015llo\n", xx1, yy1 );
   }

   //z = xx1 + yy1;
   if (sigma == 0) z=xx1 + yy1;
   if (sigma == 1) z=xx1 - yy1;
   if (arithmetic_op_debug) fprintf( stderr, "z=%015llo\n", z );
   z &= (MANTISSA|BIT37|BIT38);
   if (arithmetic_op_debug) fprintf( stderr, "z=%015llo\n", z );


   /* get sign of preliminary result*/
   if (sigma1 == 1) {
     sign_z = 1;
     if (sigma == 0) t=xx1 + yy1;
     if (sigma == 1) t=xx1 - yy1;
     if (t & SIGN) sign_z = -1;
     sign_z = -sign_z;
     if (arithmetic_op_debug) fprintf( stderr, "SIGMA1==1: sign_z=%d, t=%015llo\n", sign_z, t );
   }

   if (sigma1 == 0) {
     sign_z = 1;
     if (sigma == 0) t=xx1 + yy1;
     if (sigma == 1) t=xx1 - yy1;
     if (t & SIGN) sign_z = -1;
     if (arithmetic_op_debug) fprintf( stderr, "SIGMA1==0: sign_z=%d, t=%015llo\n", sign_z, t );
   }



   /* STEP 3. Construction code result */

   if (z & BIT38) {
     z = z + (!beta_round)*1;
     if (arithmetic_op_debug) fprintf( stderr, "A1: z=%015llo rr=%d\n", z, rr );
     rr = rr + 1;
     if (rr > MAX_EXP_MACHINE_VAL) return STOP_ADDOVF;
     z >>= 1;
     if (arithmetic_op_debug) fprintf( stderr, "A2: z=%015llo rr=%d\n", z, rr );
   }

   if (arithmetic_op_debug) fprintf( stderr, "TEMP: z=%015llo rr=%d\n", z, rr );

   //if (beta_norm && (z & BIT37)) {
   // Shura-Bura
   if (beta_norm || (z & BIT37)) {
       if (arithmetic_op_debug) fprintf( stderr, "B0: z=%015llo rr=%d\n", z, rr );
       //z >>= AUX_BIT_SHIFT;
       //if (z != 0) rr = rr;
       if (arithmetic_op_debug) fprintf( stderr, "B1: z=%015llo rr=%d\n", z, rr );
       if (z == 0) { 
           t = norm_zero(); 
           *result = t;
           if (arithmetic_op_debug) fprintf( stderr, "B1A FINAL: t=%015llo\n\n", t );
           return SCPE_OK; 
       }
   }

   fprintf( stderr, "TEMP: z=%015llo rr=%d\n", z, rr );

   // not so strong
   //if ((!beta_norm)) {
   // Shura-Bura
   if (!beta_norm && !(z & BIT38) && !(z & BIT37)) {
        if (arithmetic_op_debug) fprintf( stderr, "C0: z=%015llo rr=%d\n", z, rr );
         j = BITS_36;
         mask = (t_value)1 << j;
         if (arithmetic_op_debug) fprintf( stderr, "C1: m=%015llo j=%d\n", mask, j );
         while( j>0 && !(z & mask)) {
             j--;
             mask >>= 1;
         }
         rr_shift = BITS_36-j;
         if (arithmetic_op_debug) fprintf( stderr, "C2: m=%015llo j=%d, rr=%d, rr_shift=%d, rr-rr_shift=%d\n", mask, j, rr, rr_shift, rr - rr_shift );
         if ((rr - rr_shift) < 0) {
            t = norm_zero(); 
            *result = t;
            if (arithmetic_op_debug) fprintf( stderr, "C2A FINAL: t=%015llo\n\n", t );
            return SCPE_OK; 
         }
         rr = rr - rr_shift;
         z <<= rr_shift; 
         if (arithmetic_op_debug) fprintf( stderr, "C3: rr=%d zz=%015llo\n", rr, z );
         if (z == 0) { 
            t = norm_zero(); 
            *result = t;
            if (arithmetic_op_debug) fprintf( stderr, "C3A FINAL: t=%015llo\n\n", t );
            return SCPE_OK; 
         }
   }

   /* Final result writing */
   if (arithmetic_op_debug) fprintf( stderr, "FINAL 0: z=%015llo rr=%d\n", z, rr );
   z >>= AUX_BIT_SHIFT;
   z &= MANTISSA;
   if (arithmetic_op_debug) fprintf( stderr, "FINAL 1: z=%015llo rr=%d\n", z, rr );

   t = z | ((t_value)rr << BITS_36);
   //if (sign_z) t |= SIGN;
   if (sign_z<0) t |= SIGN;
   t |= (x|y) & TAG;

   if (arithmetic_op_debug) fprintf( stderr, "FINAL 2: t=%015llo\n\n", t );

   *result = t;

   return SCPE_OK;   
}




/*
 *  Square root arithmetic operation implementation.
 *  According this book: Shura-Bura,Starkman pp. 86-89
 *  (russian edition, 1962)
 */
t_stat new_arithmetic_square_root (t_value *result, t_value x, int op_code)
{
    int beta_round, p, rr, do_shift_right_one, i;
    t_value m, t, us, qs, qqs, n, zero, x1, zz;

    if (arithmetic_op_debug) fprintf( stderr, "NEW_SQRT: op_type=%d x=%015llo\n", op_code, x );

    if (x & SIGN) {
	/* square root from negative number */
	return STOP_NEGSQRT;
    }

    /* rounding flag */
    beta_round = op_code >> 4 & 1;

    /* extract mantissa */
    m = x & MANTISSA;
    zz = 0;

    /* extract exponent */
    p = x >> BITS_36 & EXPONENT_VALUE_MASK;

    if (arithmetic_op_debug) fprintf( stderr, "round=%d p=%d m=%015llo\n", beta_round, p, m );

    /* make new exponent */
    do_shift_right_one = 0;
    rr = (p >> 1) + M20_MANTISSA_SHIFT/2;
    if (p & 1) { 
        rr += 1; 
       do_shift_right_one = 1; 
    }

    if (arithmetic_op_debug) fprintf( stderr, "rr=%d shift_right=%d\n", rr, do_shift_right_one );

    /* Build 37-bit mantissa */

    x1 = m;
    if (x1 == 0) { 
        t = norm_zero(); 
        *result = t;
        if (arithmetic_op_debug) fprintf( stderr, "ZERO: t=%015llo\n\n", t );
        return SCPE_OK; 
    }
    zero = 0;
    n = 0;
    us = ((t_value)1 << BITS_36);
    us >>= 1;
    qs = 0;
    if (do_shift_right_one) {
      qs = 0 - x1;
    }
    else {
      qs = 0 - (x1 + x1);
    }
   
    if (arithmetic_op_debug) fprintf( stderr, "INIT: us=%015llo qs=%015llo\n", us, qs );

    for( i=1; i<37; i++ ) {
       qqs = qs + us + (n << 1);
       if (qqs & SIGN) {
           qs = (qqs << 1);
           n = n + us;
       }
       else {
          t = qqs - n - n - us;
          qs = (t << 1);
          n = n;
       }
       us >>= 1;
       if (arithmetic_op_debug) fprintf( stderr, "LOOP: us=%015llo qs=%015llo qqs=%015llo, n=%015llo\n", us, qs, qqs, n );
    }

    if (arithmetic_op_debug) fprintf( stderr, "DONE: us=%015llo qs=%015llo qqs=%015llo, n=%015llo\n", us, qs, qqs, n );

    zz = n;
    if (arithmetic_op_debug) fprintf( stderr, "READY: zz=%015llo rr=%d\n", zz, rr );

    zz = zz + (!beta_round)*1;
    if (arithmetic_op_debug) fprintf( stderr, "READY: zz=%015llo rr=%d\n", zz, rr );

    zz &= MANTISSA;
    if (arithmetic_op_debug) fprintf( stderr, "FINAL 0: zz=%015llo rr=%d\n", zz, rr );

    if (zz == 0) { 
        t = norm_zero(); 
        *result = t;
        if (arithmetic_op_debug) fprintf( stderr, "FINAL ZERO 1: t=%015llo\n\n", t );
        return SCPE_OK; 
    }

    /* Make final result */
    if (arithmetic_op_debug) fprintf( stderr, "FINAL 10: zz=%015llo rr=%d\n", zz, rr );

    t = zz | ((t_value)rr) << BITS_36;
    t |= x & TAG;
    if (arithmetic_op_debug) fprintf( stderr, "FINAL 12: t=%015llo\n\n", t );

    *result = t;

    return SCPE_OK;
}





/*
 *  Multiply arithmetic operation implementation.
 *  According this book: Shura-Bura,Starkman pp. 77-82
 *  (russian edition, 1962)
 */
t_stat new_arithmetic_mult_op (t_value *result, t_value x, t_value y, int op_code)
{
    int beta_round, beta_norm, rr, sign_zz, p, q, sign_x, sign_y, i, sign_sigma_r;
    t_value t, x1, y1, sigma_r, delta_r, mask_e2r, mask_2r_1, rr_lo, rr_hi, zz;
    //t_value mask, t2, t1;
    //int rr_shift, j, r_bit;

   if (result == NULL) return STOP_INVARG;

   if (arithmetic_op_debug) fprintf( stderr, "NEW MULT: op=%02o, x=%015llo, y=%015llo\n", op_code, x, y );

   beta_round = (op_code >> 4 & 1);
   beta_norm = (op_code >> 5 & 1);

   sign_x = get_number_sign(x);
   sign_y = get_number_sign(y);

   if (arithmetic_op_debug) 
       fprintf( stderr, "round=%d norm=%d sign_x=%d sign_y=%d\n", beta_round, beta_norm, sign_x, sign_y );

   p = (x & (EXPONENT|EXPONENT_SIGN)) >> BITS_36;
   q = (y & (EXPONENT|EXPONENT_SIGN)) >> BITS_36;

   x1 = (x & MANTISSA);
   y1 = (y & MANTISSA);

   if (arithmetic_op_debug) fprintf( stderr, "p=%d, q=%d, x1=%015llo, y1=%015llo\n", p, q, x1, y1 );

   /* Step 1. Preliminary production */
   zz = 0;

   sign_zz = sign_x * sign_y;
   rr = p + q - M20_MANTISSA_SHIFT;
   sigma_r = (!beta_round)*1;

   mask_e2r = 2;
   mask_2r_1 = 1;

   rr_lo = 0;
   rr_hi = 0;

   if (arithmetic_op_debug) 
     fprintf( stderr, "rr=%d sign_zz=%d sigma_r=%015llo mask_e2r=%015llo mask_2r_1=%015llo\n", 
                       rr, sign_zz, sigma_r, mask_e2r, mask_2r_1 );


   for( i=1; i<20; i++ ) {
   //for( i=1; i<16; i++ ) {
       sign_sigma_r = get_number_sign(sigma_r);
       if (sign_sigma_r == 1) {
         if ((!(x1 & mask_e2r)) && (!(x1 & mask_2r_1))) delta_r = 0;
         if ((!(x1 & mask_e2r)) && (x1 & mask_2r_1)) delta_r = 1;
         if ((x1 & mask_e2r) && (!(x1 & mask_2r_1))) delta_r = 2;
         if ((x1 & mask_e2r) && (x1 & mask_2r_1)) delta_r = -1;
       }
       if (sign_sigma_r == -1) {
         if ((!(x1 & mask_e2r)) && (!(x1 & mask_2r_1))) delta_r = 1;
         if ((!(x1 & mask_e2r)) && (x1 & mask_2r_1)) delta_r = 2;
         if ((x1 & mask_e2r) && (!(x1 & mask_2r_1))) delta_r = -1;
         if ((x1 & mask_e2r) && (x1 & mask_2r_1)) delta_r = 0;
       }
       if (arithmetic_op_debug) {
         fprintf( stderr, "i=%d sign_sigma_r=%d delta_r=%d mask_e2r=%015llo mask_2r_1=%015llo\n", 
                           i, sign_sigma_r, delta_r, mask_e2r, mask_2r_1 );
       }
       t = delta_r * y1;
#if 0
       if (delta_r == 0) t = 0;
       if (delta_r == -1) t = (y1 ^ SIGN);
       if (delta_r == 1) t = y1;
       if (delta_r == 2) t = (y1 << 1);
#endif
       sigma_r = (sigma_r >> 2) + t;
       if (i <= 9)  rr_lo = sigma_r;
       if (i >= 10) rr_hi = sigma_r;
       if (arithmetic_op_debug) fprintf( stderr, "t=%015llo sigma_r=%015llo rr_lo=%015llo rr_hi=%015llo\n", 
                                                  t, sigma_r, rr_lo, rr_hi );
       mask_e2r <<= 2;
       mask_2r_1 <<= 2;
       if (i == 9) sigma_r = (sigma_r >> BITS_36);
   }



   /* Step 2. Produce final result */

   zz = rr_hi;
   if (arithmetic_op_debug) fprintf( stderr, "rr=%d zz=%015llo rr_lo=%015llo rr_hi=%015llo\n", rr, zz, rr_lo, rr_hi );

   if (!beta_round && !beta_norm) {
        if (arithmetic_op_debug) 
          fprintf( stderr, "A11: rr=%d zz=%015llo rr_lo=%015llo rr_hi=%015llo\n", rr, zz, rr_lo, rr_hi );
	/* Округление по 38 разряду. */
	if (rr_lo & 0200000000000LL) {
	    rr_lo += 1;
	    if (rr_lo & 0400000000000LL) {
	      zz += 1;
	      rr_lo &= MANTISSA;
	    }
	}
        if (arithmetic_op_debug) 
          fprintf( stderr, "A12: rr=%d zz=%015llo rr_lo=%015llo rr_hi=%015llo\n", rr, zz, rr_lo, rr_hi );
   }

   if (!beta_round && beta_norm) {
        if (arithmetic_op_debug) 
          fprintf( stderr, "A31: rr=%d zz=%015llo rr_lo=%015llo rr_hi=%015llo\n", rr, zz, rr_lo, rr_hi );
	/* Округление по 37 разряду. */
	if (rr_lo & 0400000000000LL) {
	    zz += 1;
	}
        if (arithmetic_op_debug) 
          fprintf( stderr, "A33: rr=%d zz=%015llo rr_lo=%015llo rr_hi=%015llo\n", rr, zz, rr_lo, rr_hi );
   }
   
   if (arithmetic_op_debug) 
       fprintf( stderr, "A4: rr=%d zz=%015llo rr_lo=%015llo rr_hi=%015llo\n", rr, zz, rr_lo, rr_hi );


   if (!beta_norm && ! (zz & 0400000000000LL)) {
        /* Округление. */
	/* Нормализация на один разряд влево. */
	--rr;
	zz <<= 1;
	rr_lo <<= 1;
	if (rr_lo & BIT37) {
            zz += 1; 
	    rr_lo &= MANTISSA;
	}
        if (arithmetic_op_debug) 
          fprintf( stderr, "rr=%d zz=%015llo rr_lo=%015llo rr_hi=%015llo\n", rr, zz, rr_lo, rr_hi );
   }

   if (arithmetic_op_debug) 
     fprintf( stderr, "rr=%d zz=%015llo rr_lo=%015llo rr_hi=%015llo\n", rr, zz, rr_lo, rr_hi );

   if ((zz != 0) && (rr > MAX_EXP_MACHINE_VAL)) {
       return STOP_MULOVF;
   }

   if ((zz == 0) || (rr < 0)) {
     t = norm_zero(); 
     *result = t;
     if (arithmetic_op_debug) fprintf( stderr, "FINAL ZERO: t=%015llo\n\n", t );
     return SCPE_OK; 
   }

    if (rr > MAX_EXP_MACHINE_VAL) {
	/* переполнение при умножении */
	return STOP_MULOVF;
    }

   /* Make final result */
//do_final_result:
   if (arithmetic_op_debug) 
     fprintf( stderr, "FINAL 1: rr=%d rr_lo=%015llo rr_hi=%015llo zz=%015llo\n", rr, rr_lo, rr_hi, zz );

   t = zz & MANTISSA;
   t |= ((t_value)rr << BITS_36);
   if (sign_zz < 0) t |= SIGN;
   t |= (x|y) & TAG;

   regP1 = rr_lo;
   regP1 |= (t_value) rr << BITS_36;
   regP1 |= ((x ^ y) & SIGN) | ((x | y) & TAG);

   if (arithmetic_op_debug) 
     fprintf( stderr, "FINAL 8: rr=%d zz=%015llo, t==%015llo, regP1=%015llo\n\n", rr, zz, t, regP1 );

   *result = t;

   return SCPE_OK;   
}





/*
 *  Division arithmetic operation implementation.
 *  According this book: Shura-Bura,Starkman pp. 77-82
 *  (russian edition, 1962)
 */
t_stat new_arithmetic_div_op (t_value *result, t_value x, t_value y, int op_code)
{
    int beta_round, rr, sign_zz, p, q, sign_x, sign_y, i, ak_prev, ak;
    t_value t, x1, y1, zz, qk, qk1, zz1, rr_shift;

   if (result == NULL) return STOP_INVARG;

   if (arithmetic_op_debug) fprintf( stderr, "NEW DIV: op=%02o, x=%015llo, y=%015llo\n", op_code, x, y );

   beta_round = (op_code >> 4 & 1);

   sign_x = get_number_sign(x);
   sign_y = get_number_sign(y);

   if (arithmetic_op_debug) fprintf( stderr, "round=%d sign_x=%d sign_y=%d\n", beta_round, sign_x, sign_y );

   p = (x & (EXPONENT|EXPONENT_SIGN)) >> BITS_36;
   q = (y & (EXPONENT|EXPONENT_SIGN)) >> BITS_36;

   x1 = (x & MANTISSA);
   y1 = (y & MANTISSA);

   if (arithmetic_op_debug) fprintf( stderr, "p=%d, q=%d, x1=%015llo, y1=%015llo\n", p, q, x1, y1 );

   if (x1 >= 2*y1) {
       /* mantissa overflow on division */
       return STOP_DIVMOVF;
   }

   if (is_zero(y)) {
       /* division by zero */
       return STOP_DIVZERO;
   }


   /* Step 1. Preliminary production */
   zz = 0;
   zz1 = 0;
   rr_shift = (t_value)1<<(BITS_36+1);

   sign_zz = sign_x * sign_y;
   rr = p - q + M20_MANTISSA_SHIFT;

   qk = x1 - y1;
   ak_prev=1;
   if (qk & SIGN) ak_prev=-1;

   if (arithmetic_op_debug) {
     fprintf( stderr, "rr=%d sign_zz=%d ak_prev=%d qk=%015llo, zz1=%015llo rr_shift=%015llo\n", 
                       rr, sign_zz, ak_prev, qk, zz1, rr_shift );
   }

   for( i=1; i<39; i++ ) {
#if 0
      if (qk >= 0) ak=1;
      if (qk < 0)  ak=-1;
      qk1 = (2*qk) - ak*y1;
#endif
      qk1 = (qk << 1);
      if (arithmetic_op_debug) fprintf( stderr, "i=%d: ak_prev=%d qk=%015llo, qk1=%015llo rr_shift=%015llo\n", 
                                                 i, ak_prev, qk, qk1, rr_shift );
      if (qk & SIGN) {
        /* qk < 0, ak=-1 */
         qk1 += y1;
         ak = -1;
         if (arithmetic_op_debug) fprintf( stderr, "i=%d: qk<0: qk1=%015llo, zz1=%015llo\n", i, qk1, zz1 );
      }
      else {
        /* qk >= 0), ak=+1 */
         qk1 += (0-y1);
         ak = 1;
         if (arithmetic_op_debug) fprintf( stderr, "i=%d: qk>=0: qk1=%015llo, zz1=%015llo\n", i, qk1, zz1 );
      }
      if (ak_prev > 0) {
         zz1 += (rr_shift<<1);
         if (arithmetic_op_debug) fprintf( stderr, "i=%d: ak_prev=%d qk1=%015llo, zz1=%015llo\n", 
                                                    i, ak_prev, qk1, zz1 );
      }
      rr_shift >>= 1;
      qk = qk1;
      ak_prev = ak;
      if (arithmetic_op_debug) fprintf( stderr, "i=%d: qk1=%015llo, zz1=%015llo rr_shift=%015llo\n", i, qk1, zz1, rr_shift );
   }

   if (arithmetic_op_debug) fprintf( stderr, "qk1=%015llo, zz1=%015llo\n", qk1, zz1 );

   zz1 >>= 1;
   if (arithmetic_op_debug) fprintf( stderr, "qk1=%015llo, zz1=%015llo\n", qk1, zz1 );

   /* Step 2. Produce final result */

   if (arithmetic_op_debug) fprintf( stderr, "FINAL 0: rr=%d zz1=%015llo\n", rr, zz1 );

   /* Normalize and round */
   if (zz1 & BIT37) {
      //zz1 = (zz1 >> 1) + !(beta_round)*1;
      zz1 += !(beta_round)*1;
      if (arithmetic_op_debug) fprintf( stderr, "FINAL 09: rr=%d zz1=%015llo\n", rr, zz1 );
      zz1 >>= 1;
      rr = rr + 1;
     if (arithmetic_op_debug) fprintf( stderr, "FINAL 10: rr=%d zz1=%015llo\n", rr, zz1 );
   }
   else {
     zz1 += !(beta_round)*1;
     if (arithmetic_op_debug) fprintf( stderr, "FINAL 11: rr=%d zz1=%015llo\n", rr, zz1 );
   }

   zz = zz1;

   if ((zz == 0) || (rr < 0)) {
	/* Computer's zero. */
        t = norm_zero(); 
        *result = t;
        //*result = t | ((x | y) & TAG);
        if (arithmetic_op_debug) fprintf( stderr, "FINAL 15 ZERO: t=%015llo\n\n", t );
	return SCPE_OK;
   }

   if (rr > MAX_EXP_MACHINE_VAL) {
     /* переполнение при делении */
     return STOP_DIVOVF;
   }


   if (arithmetic_op_debug) fprintf( stderr, "FINAL 20: rr=%d zz=%015llo\n", rr, zz );

   t = zz & MANTISSA;
   t |= ((t_value)rr << BITS_36);
   if (sign_zz < 0) t |= SIGN;
   t |= (x|y) & TAG;

   if (arithmetic_op_debug) fprintf( stderr, "FINAL 22: rr=%d zz=%015llo t=%015llo\n\n", rr, zz, t  );

   *result = t;

   return SCPE_OK;   
}





/*
 * Prepare setup for external device access.
 * Conditional word must selected only one type of work:
 * 1)drum; 2)tape; 3)tape format; 4)print; 5)punch.
 */
t_stat ext_io_setup (int a1, int a2, int a3)
{
    ext_io_op = a1;
    ext_io_dev_zone_addr = a2;
    ext_io_ram_end = a3;

    if (ext_io_op & EXT_PUNCH) {
        if (ext_io_op & (EXT_DRUM|EXT_TAPE|EXT_TAPE_FORMAT)) {
            return STOP_PUNCHBADBITS;
        }
        return SCPE_OK;
	/* Output to punch cards is NOT supported */
	//return STOP_PUNCHUNSUPP;
    }

    if (ext_io_op & EXT_PRINT) {
        if (ext_io_op & (EXT_DRUM|EXT_TAPE|EXT_TAPE_FORMAT)) {
            return STOP_PRINTBADBITS;
	}
        return SCPE_OK;
	/* Output to printer is NOT supported */
	//return STOP_PRINTUNSUPP;
    }

    if (ext_io_op & EXT_DRUM) {
        if (ext_io_op & (EXT_TAPE|EXT_TAPE_FORMAT|EXT_PRINT|EXT_PUNCH)) {
            return STOP_DRUMBADBITS;
        }
        return SCPE_OK;
        /* Magnetic drum storage device is NOT supported */
        //return STOP_DRUMUNSUPP;
    }


    if (ext_io_op & EXT_TAPE) {
       if (ext_io_op & (EXT_DRUM|EXT_TAPE_FORMAT|EXT_PRINT|EXT_PUNCH)) {
           return STOP_TAPEBADBITS;
       }
        return SCPE_OK;
       /* Magnetic tape storage device is NOT supported */
       //return STOP_TAPEUNSUPP;
    }

    if (ext_io_op & EXT_TAPE_FORMAT) {
        if (ext_io_op & (EXT_DRUM|EXT_TAPE|EXT_PRINT|EXT_PUNCH)) {
            return STOP_TAPEFMTBADBITS;
        }
        return SCPE_OK;
        /* Tape formatting is NOT supported */
        //return STOP_TAPEFMTUNSUPP;
    }

    return SCPE_OK;
}



/*
 * Execute i/o operation for external device.
 * On reading error is returned STOP_READERR.
 * Checksum is calculated in parameter sum.
 * Memory blocking (EXT_DIS_RAM) and control blocking (EXT_DIS_CHECK) ARE SUPPORTED.
 */
t_stat ext_io_operation (int a1, t_value * sum)
{
    t_stat err;
    int  codes_num;

    ext_io_ram_start = a1;        
    *sum = 0;

    if (ext_io_op & EXT_PUNCH) {
         int add_only_flag = 0;
         int disable_mem_access = 0;
         int disable_checksum = 0;
         if (ext_io_op & EXT_DIS_RAM) disable_mem_access = 1;
         if (ext_io_op & EXT_DIS_CHECK) disable_checksum = 1;
         if ((ext_io_op & EXT_PRINT) && (ext_io_op & EXT_PUNCH)) {
            if ((active_cdp == 0) && (active_lpt == 0)) return STOP_NOT_READY_PUNCH;
            if (active_cdp == 0) goto check_print_op;
            add_only_flag = 1;
         }
         codes_num = 0;
         err = punch_card (ext_io_ram_start, ext_io_ram_end, ext_io_dev_zone_addr, add_only_flag, 
                           disable_mem_access, disable_checksum, &codes_num, sum );
        if (sim_deb && cpu_dev.dctrl)
	    fprintf (sim_deb, "cpu: err=%d, codes_num=%04o\n", err,codes_num);
         delay += (100000*codes_num);
         return err;
	 /* Output to punch cards is NOT supported */
	 //return STOP_PUNCHUNSUPP;
    }

  check_print_op:
    if (ext_io_op & EXT_PRINT) {
         int print_type = PRINT_TYPE_DECIMAL;
         int add_only_flag = 0;
         int disable_mem_access = 0;
         int disable_checksum = 0;
         if (ext_io_op & EXT_DIS_STOP) print_type = PRINT_TYPE_OCTAL;
         if (enable_m20_print_ascii_text) {
           if (ext_io_op & EXT_TAPE_REV) {
	       /* Print of text (not present in real M-20 of 1958 year) */
               print_type = PRINT_TYPE_TEXT;
           }
         }
         if (ext_io_op & EXT_DIS_RAM) disable_mem_access = 1;
         if (ext_io_op & EXT_DIS_CHECK) disable_checksum = 1;
         if ((ext_io_op & EXT_PRINT) && (ext_io_op & EXT_PUNCH)) {
           if (active_lpt == 0) {
             return STOP_NOT_READY_PRINT;
           }
           add_only_flag = 1;
         }
         codes_num = 0;
         err = write_line_printer (ext_io_ram_start, ext_io_ram_end, ext_io_dev_zone_addr, 
                                   print_type, add_only_flag, disable_mem_access, disable_checksum,
                                   &codes_num );
        if (sim_deb && cpu_dev.dctrl)
	    fprintf (sim_deb, "cpu: err=%d, codes_num=%04o\n", err,codes_num);
         delay += (50000*codes_num);
         return err;
	 /* Output to printer is NOT supported */
	 //return STOP_PRINTUNSUPP;
    }

    if (ext_io_op & EXT_DRUM) {
	/* Drum (DRM,МБ) */
        codes_num = 0;
	err = drum_io (sum,&codes_num);
        if (sim_deb && cpu_dev.dctrl)
	    fprintf (sim_deb, "cpu: err=%d, codes_num=%04o sum=%015llo\n", err,codes_num,*sum);
        delay += (40000+((double)codes_num)/6400);
	return err;
        /* Magnetic drum storage device is NOT supported */
        //return STOP_DRUMUNSUPP;
    }

    if (ext_io_op & EXT_TAPE) {
	/* Magnetic tape (MT,МЛ) */
        codes_num = 0;
	err = mt_tape_io (sum,&codes_num);
        if (sim_deb && cpu_dev.dctrl)
	    fprintf (sim_deb, "cpu: err=%d, codes_num=%04o sum=%015llo\n", err,codes_num,*sum);
	delay += (75000+((double)codes_num/2500)); 
	return err;
       /* Magnetic tape storage device is NOT supported */
       //return STOP_TAPEUNSUPP;
    }

    if (ext_io_op & EXT_TAPE_FORMAT) {
        codes_num = 0;
	err = mt_format_tape (sum,&codes_num,ext_io_ram_start,ext_io_ram_end);
        if (sim_deb && cpu_dev.dctrl)
	    fprintf (sim_deb, "cpu: err=%d codes_num=%04o\n", err, codes_num );
	delay += (75000+((double)codes_num/2500)); 
	return err;
        /* Tape formatting is NOT supported */
        //return STOP_TAPEFMTUNSUPP;
    }

    return SCPE_OK;
}





/*
 * Execute one instruction, contained in register RK.
 */
t_stat cpu_one_inst ()
{
	int addr_tags, op, a1, a2, a3, n = 0;
	t_value x, y, t, xm, ym, xe, ye;
	t_stat err;
	t_stat ret_code = SCPE_OK;
	//unsigned __int64 t1;
	uint32 res32;

	addr_tags = regRK >> BITS_42 & MAX_ADDR_TAG_VALUE;
	op = regRK >> BITS_36 & MAX_OPCODE_VALUE;
	a1 = regRK >> BITS_24 & MAX_ADDR_VALUE;
	a2 = regRK >> BITS_12 & MAX_ADDR_VALUE;
	a3 = regRK >> BITS_0  & MAX_ADDR_VALUE;

	/* If set corresponding bit of address-sign,
	 * then to address is added value of address register (A+RA). */
	if (addr_tags & 4) a1 = (a1 + regRA) & MAX_ADDR_VALUE;
	if (addr_tags & 2) a2 = (a2 + regRA) & MAX_ADDR_VALUE;
	if (addr_tags & 1) a3 = (a3 + regRA) & MAX_ADDR_VALUE;


	/* test for memory contents overflow */
        if (memory_45_checking) {
	  t = mosu_load(a1);
	  if (t & ~WORD45) {
            if (sim_deb && cpu_dev.dctrl)
	      fprintf (sim_deb, "cpu: OVERFLOW BEFORE: a1: t[%04o]=%018llo, t=%018llo\n", a1, t, t & ~WORD45 );
            ret_code = STOP_MEMORY_GARBAGE_DETECTED;
            goto done;
          }
	  t = mosu_load(a2);
	  if (t & ~WORD45) {
            if (sim_deb && cpu_dev.dctrl)
	      fprintf (sim_deb, "cpu: OVERFLOW BEFORE: a2: t[%04o]=%018llo, t=%018llo\n", a2, t, t & ~WORD45  );
            ret_code = STOP_MEMORY_GARBAGE_DETECTED;
            goto done;
          }
	  t = mosu_load(a3);
	  if (t & ~WORD45) {
            if (sim_deb && cpu_dev.dctrl)
	      fprintf (sim_deb, "cpu: OVERFLOW BEFORE: a3: t[%04o]=%018llo, t=%018llo\n", a3, t, t & ~WORD45  );
            ret_code = STOP_MEMORY_GARBAGE_DETECTED;
            goto done;
          }
        }


        if (sim_brk_summ) {		/* breakpoint on read access? */
          res32 = sim_brk_test(a1, SWMASK ('R'));
          if (res32) {
             ret_code = STOP_MEM;	/* stop simulation */
             goto done;
          }
          res32 = sim_brk_test(a2, SWMASK ('R'));
          if (res32) {
             ret_code = STOP_MEM;	/* stop simulation */
             goto done;
          }
          res32 = sim_brk_test(a3, SWMASK ('W'));
          if (res32) {
             ret_code = STOP_MEM;	/* stop simulation */
             goto done;
          }
        }



	switch (op) {
	default:
	        delay += 24.0;
		ret_code = STOP_BADCMD;
		goto done;

	/*
         *   Numbers Operations 
         */

	case OPCODE_ADD_ROUND_NORM:         /* 001 = addition w/round and w/norm */
	case OPCODE_ADD_NORM:               /* 021 = addition wo/round and w/norm */
	case OPCODE_ADD_ROUND:              /* 041 = addition w/round and wo/norm */
	case OPCODE_ADD:                    /* 061 = addition wo/round and wo/norm */
		x = mosu_load (a1);
		y = mosu_load (a2);
		if (use_add_sbst) {
                  err = new_arithmetic_op( &regRR, x, y, op );
		  goto add2;
		}
add:		
                //if (new_add) err = new_addition_v20 (&regRR, x, y, op >> 4 & 1, op >> 5 & 1);
                if (new_add) err = new_addition_v44 (&regRR, x, y, op >> 4 & 1, op >> 5 & 1);
                else err = addition (&regRR, x, y, op >> 4 & 1, op >> 5 & 1);
add2:
		if (err) { ret_code = err; goto done; }
		mosu_store (a3, regRR);
		trgSW = (regRR & SIGN) != 0;
		delay += 28.5;
		break;


	case OPCODE_SUB_ROUND_NORM:         /* 002 = subtraction w/round and w/norm */
	case OPCODE_SUB_NORM:               /* 022 = subtraction wo/round and w/norm */
	case OPCODE_SUB_ROUND:              /* 042 = subtraction w/round and wo/norm */
	case OPCODE_SUB:                    /* 062 = subtraction wo/round and wo/norm */
                if (use_add_sbst) {
		    x = mosu_load (a1);
		    y = mosu_load (a2);
                    err = new_arithmetic_op( &regRR, x, y, op );
		    goto add2;
                }
		x = mosu_load (a1);
		y = mosu_load (a2) ^ SIGN;
		goto add;


	case OPCODE_SUB_MOD_ROUND_NORM:     /* 003 = modulus subtraction w/round and w/norm */
	case OPCODE_SUB_MOD_NORM:           /* 023 = modulus subtraction wo/round and w/norm */
	case OPCODE_SUB_MOD_ROUND:          /* 043 = modulus subtraction w/round and wo/norm */
	case OPCODE_SUB_MOD:                /* 063 = modulus subtraction wo/round and wo/norm */
	     {
                int no_norm = 1;
                if (use_add_sbst) {
		    x = mosu_load (a1);
		    y = mosu_load (a2);
                    err = new_arithmetic_op( &regRR, x, y, op );
		    goto add2;
                }
		x = mosu_load (a1) & ~SIGN;
		y = mosu_load (a2) | SIGN;
		if ((op==OPCODE_SUB_MOD_ROUND_NORM) || (op==OPCODE_SUB_MOD_NORM)) no_norm=0; 
                if (new_add) err = new_addition_v44 (&regRR, x, y, 1, no_norm);
                //if (new_add) err = new_addition_v20 (&regRR, x, y, 1, no_norm);
                else err = addition (&regRR, x, y, 1, no_norm);
		goto add2;
	     }

	case OPCODE_MULT_ROUND_NORM:        /* 005 = multiplication w/round and w/norm */
	case OPCODE_MULT_NORM:              /* 025 = multiplication wo/round and w/norm */
	case OPCODE_MULT_ROUND:             /* 045 = multiplication w/round and wo/norm */
	case OPCODE_MULT:                   /* 065 = multiplication wo/round and wo/norm */
		x = mosu_load (a1);
		y = mosu_load (a2);
                if (new_mult) err = new_arithmetic_mult_op (&regRR, x, y, op);
                else err = multiplication (&regRR, x, y, op >> 4 & 1, op >> 5 & 1);
		if (err) { ret_code = err; goto done; }
		mosu_store (a3, regRR);
		trgSW = (int) (regRR >> BITS_36 & EXPONENT_VALUE_MASK) > EXP_OVF_VALUE;
		delay += 69.5;
		break;


	case OPCODE_DIV_ROUND_NORM:         /* 004 = division w/round */
	case OPCODE_DIV_NORM:               /* 024 = division wo/round */
		x = mosu_load (a1);
		y = mosu_load (a2);
                if (new_div) err = new_arithmetic_div_op (&regRR, x, y, op);
                else err = division (&regRR, x, y, op >> 4 & 1);
		if (err) { ret_code = err; goto done; }
		mosu_store (a3, regRR);
		trgSW = (int) (regRR >> BITS_36 & EXPONENT_VALUE_MASK) > EXP_OVF_VALUE;
		delay += 136.5;
		break;


	case OPCODE_SQRT_ROUND_NORM:        /* 044 = square root calculation w/round */
	case OPCODE_SQRT_NORM:              /* 064 = square root calculation wo/round */
		x = mosu_load (a1);
                if (new_sqrt) err = new_arithmetic_square_root (&regRR, x, op);
                else err = square_root (&regRR, x, op >> 4 & 1);
		if (err) { ret_code = err; goto done; }
		mosu_store (a3, regRR);
		trgSW = (int) (regRR >> BITS_36 & EXPONENT_VALUE_MASK) > EXP_OVF_VALUE;
		delay += 275.0;
		break;


	case OPCODE_OUT_LOWER_BITS_OF_MULT: /* 047 = out lower part of muliply production  */
                                            /* Use only after op 025 or 065, but in real all otherwise */
                switch( old_opcode ) {
	            case OPCODE_MULT_ROUND_NORM:        /* 005 = multiplication w/round and w/norm */
	            case OPCODE_MULT_NORM:              /* 025 = multiplication wo/round and w/norm */
	            case OPCODE_MULT_ROUND:             /* 045 = multiplication w/round and wo/norm */
	            case OPCODE_MULT:                   /* 065 = multiplication wo/round and wo/norm */
		        regRR = regP1;             
                        trgSW = (int) (regRR >> BITS_36 & EXPONENT_VALUE_MASK) > EXP_OVF_VALUE;
	                break;
	            default:
                        //t = regRR & EXP_SIGN_TAG; 
                        //fprintf( stderr, "regP1=%015llo\n", regP1 );
                        t = (regRR & EXP_SIGN_TAG) | (regP1 & MANTISSA);
                        regRR = t;
                        trgSW = (regRR & MANTISSA) == 0;
	                break;
	        }
                //trgSW = (int) (regRR >> BITS_36 & EXPONENT_VALUE_MASK) > EXP_OVF_VALUE;
		mosu_store (a3, regRR);
		//trgSW = (regRR & MANTISSA) == 0; 		
		//if (trgSW) goto sw1;
		//if (!trgSW) trgSW = (int) (regRR >> BITS_36 & EXPONENT_VALUE_MASK) > EXP_OVF_VALUE;
             //sw1:
		delay += 24.0;
		break;


	case OPCODE_ADD_ADDR_TO_EXP:        /* 006 = addition exponent and address  */
		n = (a1 & EXPONENT_VALUE_MASK) - M20_MANTISSA_SHIFT;
		y = mosu_load (a2);
		delay += 61.5;
add_exp:		
                err = add_exponent (&regRR, y, n, op);
		if (err) { ret_code = err; goto done; }
		mosu_store (a3, regRR);
		trgSW = (int) (regRR >> BITS_36 & EXPONENT_VALUE_MASK) > EXP_OVF_VALUE;
		break;

	case OPCODE_ADD_EXP_TO_EXP:         /* 026 = addition of exponents */
		delay += 24.0;
                x = mosu_load (a1);
		n = (int) (x >> BITS_36 & EXPONENT_VALUE_MASK) - M20_MANTISSA_SHIFT; 
                y = mosu_load (a2);
		goto add_exp;

	case OPCODE_SUB_ADDR_FROM_EXP:      /* 046 = subtraction address from exponent */
		delay += 61.5;
		n = M20_MANTISSA_SHIFT - (a1 & EXPONENT_VALUE_MASK);
		y = mosu_load (a2);
		goto add_exp;

	case OPCODE_SUB_EXP_FROM_EXP:       /* 066 = subtraction of exponents */
		delay += 24.0;
                x = mosu_load (a1);
		n = M20_MANTISSA_SHIFT - (int) (x >> BITS_36 & EXPONENT_VALUE_MASK);
                y = mosu_load (a2);
		goto add_exp;


	/*
         *   Codes Operations
         */

	case OPCODE_TRANSFER_MEM2MEM: /* 000 = transfer */
	    regRR = mosu_load (a1);  
	    mosu_store (a3, regRR);
	    /* w NOT changed and no AUTO-STOP */
	    delay += 24.0;
	    break;


	case OPCODE_LOAD_FROM_KEY_REGISTER:    /* 020 = read panel key registers */
		switch (a1 & 7) {
		  case 0: regRR = 0;    break;
		  case 1: regRR = RPU1; break;
		  case 2: regRR = RPU2; break;
		  case 3: regRR = RPU3; break;
		  case 4: regRR = RPU4; break;
		  case 5: /* RR? */     break;
		  default: 
                    ret_code = STOP_INVARG; /* wrong index for register value */
                    goto done;
		}
		mosu_store (a3, regRR);
		/* w NOT changed */
		delay += 24.0;
		break;


	case OPCODE_BLANKING_040:           /* 040 = blank */
#if 1
                if (enable_opcode_040_hack) {
		  delay += 24.0;
                  x = mosu_load (a1);
                  n = (x >> BITS_12) & MAX_ADDR_VALUE;
		  if (regRA < n) regKRA = a2;
		  regRA = a3;
	          break;
	        }
#endif
	        regRR = 0;
	        mosu_store( a3, regRR );
		/* w NOT changed */
                delay += 24.0;
		break;

	case OPCODE_BLANKING_060:           /* 060 = blank */
	        regRR = 0;
	        mosu_store( a3, regRR );
		/* w NOT changed */
		delay += 24.0;
		break;


	case OPCODE_COMPARE:                /* 015 = bit-wise comparison (exclusive OR) */
	case OPCODE_COMPARE_WITH_STOP:      /* 035 = bit-wise comparison with AUTO-STOP */
		regRR = mosu_load (a1) ^ mosu_load (a2);
log_comp:		
		trgSW = (regRR == 0);
		delay += 24.0;
		if ((op == OPCODE_COMPARE_WITH_STOP) && !trgSW)  {
                    ret_code = STOP_ASSERT; /* STOP on miscompare */
                    goto done;
                }
                mosu_store (a3, regRR);     /* 035 must no store result, only from engineering panel! */
		break;

	case OPCODE_LOGICAL_MULT:           /* 055  = logical multiplcation = AND */
		regRR = mosu_load (a1) & mosu_load (a2);
		goto log_comp;

	case OPCODE_LOGICAL_ADD:            /* 075 = logical addition = OR */
		regRR = mosu_load (a1) | mosu_load (a2);
		goto log_comp;


	case OPCODE_ADD_CMDS:       /* 013 = addition of commands  */
		x = mosu_load (a1);
		y = mosu_load (a2);
		y = (x & MANTISSA) + (y & MANTISSA);
add_mant:		
                regRR = (x & ~MANTISSA & WORD45) | (y & MANTISSA);
		mosu_store (a3, regRR);
                trgSW = (y & BIT37) != 0;
		//if (op == 013) trgSW = (y & BIT37) != 0;
                //if (op == 033) trgSW = (regRR & SIGN) != 0; //?
		delay += 24.0;
		break;

	case OPCODE_SUB_CMDS:       /* 033 = subtraction of commands */
		x = mosu_load (a1);
		y = mosu_load (a2);
		y = (x & MANTISSA) - (y & MANTISSA);
		goto add_mant;


	case OPCODE_ADD_OPCS:      /* 053 = addition of operaion codes */
		x = mosu_load (a1);
		y = mosu_load (a2);
		y = (x & ~MANTISSA) + (y & ~MANTISSA);
add_opc:		
                regRR = (x & MANTISSA) | (y & ~MANTISSA & WORD45);
		mosu_store (a3, regRR);
                trgSW = (y & BIT46) != 0;
		//if (op == 053) trgSW = (y & BIT46) != 0;
                //if (op == 073) trgSW = (regRR & SIGN) != 0; 
		delay += 24.0;
		break;

	case OPCODE_SUB_OPCS:      /* 073 = subtraction of operaion codes */
		x = mosu_load (a1);
		y = mosu_load (a2);
		y = (x & ~MANTISSA) - (y & ~MANTISSA);
		goto add_opc;


	case OPCODE_SHIFT_MANTISSA_BY_ADDR:      /* 014 = shift mantissa by address */
		n = (a1 & EXPONENT_VALUE_MASK) - M20_MANTISSA_SHIFT;
		delay += 61.5 + 1.5 * (n>0 ? n : -n);
sh_mant:		
                y = mosu_load (a2);
		regRR = (y & ~MANTISSA);
		//fprintf( stderr, "n=%d y=%015lo, regRR=%015llo\n", n, y, regRR );
		if (n >= 0) regRR |= (((y & MANTISSA) << n) & MANTISSA);
		else if (n < 0) regRR |= (((y & MANTISSA) >> -n) & MANTISSA);
                //regRR &= WORD45;
		mosu_store (a3, regRR);
		trgSW = ((regRR & MANTISSA) == 0);
		break;

	case OPCODE_SHIFT_MANTISSA_BY_EXP:    /* 034 = shift mantissa by exponent of number */
		n = (int) (mosu_load (a1) >> BITS_36 & EXPONENT_VALUE_MASK) - M20_MANTISSA_SHIFT;
		delay += 24.0 + 1.5 * (n>0 ? n : -n);
		goto sh_mant;


	case OPCODE_SHIFT_CODE_BY_ADDR:       /* 054 = shift by address */
		n = (a1 & EXPONENT_VALUE_MASK) - M20_MANTISSA_SHIFT;
		delay += 61.5 + 1.5 * (n>0 ? n : -n);
shift_code:		
                regRR = mosu_load (a2);
		if (n > 0) regRR = (regRR << n); 
		else if (n < 0) regRR >>= -n;
                regRR &= WORD45;
		mosu_store (a3, regRR);
		trgSW = (regRR == 0);
		break;

	case OPCODE_SHIFT_CODE_BY_EXP:        /* 074 = shift by exponet of number */
		n = (int) (mosu_load (a1) >> BITS_36 & EXPONENT_VALUE_MASK) - M20_MANTISSA_SHIFT;
		delay += 24 + 1.5 * (n>0 ? n : -n);
		goto shift_code;

	case OPCODE_ADD_CYCLIC:        /* 007 = cyclic addition */
		x = mosu_load (a1);
		y = mosu_load (a2);
	//cyclic_sum:	
		regRR = (x & ~MANTISSA) + (y & ~MANTISSA);
		t = (x & MANTISSA) + (y & MANTISSA);
		trgSW = (t & BIT37) != 0;
                if (op == OPCODE_ADD_CYCLIC) {
                  if (regRR & BIT46) regRR += BIT37;
		  if (t & BIT37) t += 1;
                  regRR &= WORD45;
		}
		if (op == OPCODE_SUB_CYCLIC) {
                  if (regRR & BIT46) regRR -= BIT37;
		  if (t & BIT37) t -= 1;
		}
		//regRR &= WORD45;
		regRR |= (t & MANTISSA);
		mosu_store (a3, regRR);
		delay += 24.0;
		break;

	case OPCODE_SUB_CYCLIC:        /* 027 = cyclic subtraction */
		x = mosu_load (a1);
		y = mosu_load (a2);
#if 0
		y = mosu_load (a2);
		y = BIT46 - y;
		goto cyclic_sum;
#endif
#if 1
                fprintf( stderr, "x=%015llo y=%015llo\n", x, y );
                xm = x & MANTISSA; 
                ym = y & MANTISSA;
                xe = x & ~MANTISSA;
                ye = y & ~MANTISSA;
                t = 0; regRR = 0;
                fprintf( stderr, "xe=%015llo ye=%015llo\n", xe,ye );
                fprintf( stderr, "xm=%015llo ym=%015llo\n", xm, ym );
                if (xm < ym) { t += BIT37 + (xm - ym) - 1; }
                else t = xm - ym;
                fprintf( stderr, "regRR=%015llo t=%015llo\n", regRR, t );
                fprintf( stderr, "xm=%015llo ym=%015llo t=%015llo\n", xm, ym,t );
                if (xe < ye) { regRR += BIT46 + (xe - ye) - BIT37; t -= 1;  }  // temp.hack for tests pass
                else regRR = xe - ye;
                //regRR &= ~MANTISSA;
                fprintf( stderr, "xe=%015llo ye=%015llo regRR=%015llo\n", xe,ye,regRR );
#endif
                trgSW = (t & BIT37) != 0;
                fprintf( stderr, "regRR=%015llo t=%015llo\n", regRR, t );
		regRR |= (t & MANTISSA);
                fprintf( stderr, "regRR=%015llo\n", regRR );
		regRR &= WORD45;
                fprintf( stderr, "regRR=%015llo\n", regRR );
		mosu_store (a3, regRR);
		delay += 24.0;
		break;

	case OPCODE_SHIFT_CYCLIC:      /* 067 = cyclic shift */
		x = mosu_load (a1);
                regRR = (x & WORD21)  << BITS_24 | (x >> BITS_24 & WORD21);
		//regRR &= WORD45;
		mosu_store (a3, regRR);
                trgSW = (a3 == 0);
		/* w not chaned (wrong!). */
		//delay += 60.0;
                delay += 24.0;
		break;



	/*
         *   Control Operations
         */

        case OPCODE_STOP_017:    /* 017 = machine stop */
        case OPCODE_STOP_037:    /* 037 = machine stop */
        case OPCODE_STOP_057:    /* 057 = machine stop */
	case OPCODE_STOP_077:    /* 077 = machine stop */
		delay += 24.0;
		regRR = 0;
		mosu_store (a3, regRR);
		/* If addresses is equal 0, then assume that is normal condition (goo stop). (!) */
		ret_code = STOP_STOP;
		goto done;
		break;

	case OPCODE_CHANGE_RA_BY_ADDR :     /* 052 = change address register by address */
		regRR = ((t_value)OPCODE_CHANGE_RA_BY_ADDR<<BITS_36) | (a1 << BITS_12);
		mosu_store (a3, regRR);
		regRA = a2;
		//delay += 24.0;
                delay += 28.5;
		break;

	case OPCODE_CHANGE_RA_BY_CODE :     /* 072 = change address register by address codeword */
		regRR = ((t_value)OPCODE_CHANGE_RA_BY_ADDR<<BITS_36) | (a1 << BITS_12);
		mosu_store (a3, regRR);
		regRA = mosu_load (a2) >> BITS_12 & MAX_ADDR_VALUE;
		//delay += 24.0;
                delay += 28.5;
		break;


	case OPCODE_JUMP_WITH_RETURN:       /* 016 = jump with return */
		regRR = ((t_value)OPCODE_JUMP_WITH_RETURN<<BITS_36) | (a1 << BITS_12);
		mosu_store (a3, regRR);
		regKRA = a2;
		delay += 24.0;
		break;

	case OPCODE_COND_JUMP_BY_SIG_W_1:   /* 036 = transfer control by condition w=1 */
		regRR = mosu_load (a1);
		mosu_store (a3, regRR);
		if (trgSW) regKRA = a2;
		delay += 24.0;
		break;

	case OPCODE_JUMP_BY_ADDR:           /* 056 = unconditional transfer control */
		regRR = mosu_load (a1);
		mosu_store (a3, regRR);
		regKRA = a2;
		delay += 24.0;
		break;

	case OPCODE_COND_JUMP_BY_SIG_W_0:   /* 076 = transfer control by condition w=0 */
		regRR = mosu_load (a1);
		mosu_store (a3, regRR);
		if (!trgSW) regKRA = a2;
		delay += 24.0;
		break;


	case OPCODE_GOTO_AFTER_CYCLE_BY_PA_012:   /* 012 = transfer control by condition < */
		if (regRA < (unsigned)a1) regKRA = a2;
		regRA = a3;
		delay += 24.0;
		break;

	case OPCODE_GOTO_AFTER_CYCLE_BY_PA_032:   /* 032 = transfer control by condition >= */
                if (regRA >= (unsigned)a1) regKRA = a2;
		regRA = a3;
		delay += 24.0;
		break;


	case OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_1_011:    /* 011 = transfer control by condition < and w=1 */
                if (regRA < (unsigned)a1 && trgSW) regKRA = a2;
		regRA = a3;
		delay += 24.0;
		break;

	case OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_1_031:    /* 031 = transfer control by condition >= and w=1 */
		if (regRA >= (unsigned)a1 && trgSW) regKRA = a2;
		regRA = a3;
		delay += 24.0;
		break;

	case OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_0_051:    /* 051 = transfer control by condition < and w=0 */
                if (regRA < (unsigned)a1 && !trgSW) regKRA = a2;
		regRA = a3;
		delay += 24.0;
		break;

	case OPCODE_GOTO_AFTER_CYCLE_BY_PA_SIG_W_0_071:    /* 071 = transfer control by condition >= and w=0 */
		if (regRA >= (unsigned)a1 && !trgSW) regKRA = a2;
		regRA = a3;
		delay += 24.0;
		break;



	/*
         *   Input/Output Operations
         */

	case OPCODE_INPUT_CODES_FROM_PUNCH_CARDS_WITH_STOP:   /* 010 = punch cards input with stop on failed checksum */
                cr_io_addr_1 = a1;
                cr_io_addr_2 = a2;
                cr_io_addr_3 = a3;
                cdr_csum = 0;
                cdr_rsum = 0;
                cdr_rcodes = 0;
                cdr_stop_blocking = 0;
                cdr_control_blocking = 0;
                if (sim_deb && cpu_dev.dctrl)
	            fprintf (sim_deb, "cpu: opcode=10: regKRA=%d,a1=%d,a2=%d,a3=%d\n", regKRA,a1,a2,a3);
                /* check for boot operation request from card reader device */
                if (boot_device_req_cdr) {
                   if (sim_deb && cpu_dev.dctrl) fprintf (sim_deb, "cpu: cdr boot detected. Set regKRA=%d\n", a1);
                   regKRA = a1;
                   boot_device_req_cdr = 0;
                }
                err = read_card(&cdr_csum,&cdr_rsum,&cdr_rcodes,&cdr_stop_blocking,&cdr_control_blocking);
		if (err) {
		    if (err == STOP_CRBADSUM) {
		      /* A1 must contain last address code of input */
		    }
                    ret_code = err;
                    goto done;
                }
		delay += (50000*cdr_rcodes);
		if (cdr_control_blocking) goto store_chksum;
		if (cdr_stop_blocking) {
                  regKRA = a2;
                  goto store_chksum;
		}
		if (cdr_csum != cdr_rsum) {
		  regKRA = a2;
                  ret_code = STOP_CRBADSUM;
                  goto done;
                }
             store_chksum:
		mosu_store (a3, cdr_csum);
		break;

	case OPCODE_INPUT_CODES_FROM_PUNCH_CARDS:   /* 030 = punch cards input without stop on failed checksum */
                cr_io_addr_1 = a1;
                cr_io_addr_2 = a2;
                cr_io_addr_3 = a3;
                cdr_csum = 0;
                cdr_rsum = 0;
                cdr_rcodes = 0;     
                cdr_stop_blocking = 0;
                cdr_control_blocking = 0;
                if (sim_deb && cpu_dev.dctrl)
	            fprintf (sim_deb, "cpu: opcode=30: regKRA=%d,a1=%d,a2=%d,a3=%d\n", regKRA,a1,a2,a3);
                err = read_card(&cdr_csum,&cdr_rsum,&cdr_rcodes,&cdr_stop_blocking,&cdr_control_blocking);
		if (err) { ret_code = err; goto done; }
		delay += (50000*cdr_rcodes);
		if (cdr_control_blocking) goto store_chksum_30;
		if (cdr_csum != cdr_rsum) {
		  regKRA = a2;
                }
             store_chksum_30:
		mosu_store (a3, cdr_csum);
		break;


	case OPCODE_IO_EXT_DEV_TO_MEM_050:  /* 050 = external device i/o setup */
		err = ext_io_setup (a1, a2, a3);
		if (err) { ret_code = err; goto done; }
		delay += 24.0;
		break;

	case OPCODE_IO_EXT_DEV_TO_MEM_070:  /* 070 = external device i/o exec */
                if (sim_deb && cpu_dev.dctrl)
	             fprintf (sim_deb, "cpu: ext_io_op=%04o\n", ext_io_op);
		if (ext_io_op == MAX_ADDR_VALUE) { 
                    ret_code = STOP_IO_MISSING_SETUP; goto done; 
                }
		err = ext_io_operation (a1, &regRR);
                if (a3) mosu_store (a3, regRR);
		if (err) {
		   if (err == STOP_READERR) {
		       /* A1 must contain last location address of successful input */
		   }
		   if (err == STOP_TAPEREADERR) {
		       /* A1 must contain last zone number of successful input */
		   }
		   if (err != STOP_READERR || !(ext_io_op & EXT_DIS_STOP)) {
                       ret_code = err; goto done;
                   }
		   if (ext_io_op & (EXT_PUNCH|EXT_PRINT)) goto skip_done;
		   if (a2) regKRA = a2;
		  skip_done: ;
		}
		delay += 24.0; 
		break;
	}


	/* test for memory contents overflow */
	if (memory_45_checking) {
	  t = mosu_load(a1);
	  if (t & ~WORD45) {
            if (sim_deb && cpu_dev.dctrl)
	      fprintf (sim_deb, "cpu: OVERFLOW AFTER: a1: t[%04o]=%018llo, t=%018llo\n", a1, t, t & ~WORD45 );
            ret_code = STOP_MEMORY_GARBAGE_DETECTED;
            goto done;
          }
	  t = mosu_load(a2);
	  if (t & ~WORD45) {
            if (sim_deb && cpu_dev.dctrl)
	      fprintf (sim_deb, "cpu: OVERFLOW AFTER: a2: t[%04o]=%018llo, t=%018llo\n", a2, t, t & ~WORD45  );
            ret_code = STOP_MEMORY_GARBAGE_DETECTED;
            goto done;
          }
	  t = mosu_load(a3);
	  if (t & ~WORD45) {
            if (sim_deb && cpu_dev.dctrl)
	      fprintf (sim_deb, "cpu: OVERFLOW AFTER: a3: t[%04o]=%018llo, t=%018llo\n", a3, t, t & ~WORD45  );
            ret_code = STOP_MEMORY_GARBAGE_DETECTED;
            goto done;
          }
        }

done:	
	/* save reg P1 state */
	switch( old_opcode ) {
	  case OPCODE_MULT_ROUND_NORM:        /* 005 = multiplication w/round and w/norm */
	  case OPCODE_MULT_NORM:              /* 025 = multiplication wo/round and w/norm */
   	  case OPCODE_MULT_ROUND:             /* 045 = multiplication w/round and wo/norm */
	  case OPCODE_MULT:                   /* 065 = multiplication wo/round and wo/norm */
	    /* P1 contains a lower part of multiply product */
            break;
	  default:
	    //addr_tags = regRK >> BITS_42 & MAX_ADDR_TAG_VALUE;
	    //a1 = regRK >> BITS_24 & MAX_ADDR_VALUE;
	    if (addr_tags & 4) a1 = (a1 + regRA) & MAX_ADDR_VALUE;
            regP1 = MOSU[a1];
            //fprintf( stderr, "1: P1=%015llo\n", regP1 );
            break;
        }

	return ret_code;
}



void print_commad_run_profile_stat(void)
{
   int i, op;
   double sum_time, sum_count;

   printf("\n*** Command time profile stat ***\n");
   sum_time = sum_count = 0;
   for( i=0; i<M20_SYM_OPCODE_TABLE_SIZE; i++ ) {
       op = cmd_profile_table[i].op_code;
       if (cmd_profile_table[i].us_count > 0) {
         sum_time += cmd_profile_table[i].us_time;
         sum_count += cmd_profile_table[i].us_count;
         printf("opcode=%02o   count=%-9.0f  times=%-15.2f  avg_time=%-15.2f   (%s)\n",
                 op, cmd_profile_table[i].us_count, cmd_profile_table[i].us_time, 
                 cmd_profile_table[i].us_time/cmd_profile_table[i].us_count, m20_opname[i] );
       }
   }
   printf("Summary:  times=%.2f  count=%.0f  avg_time=%.2f\n", sum_time, sum_count, sum_time/sum_count );
   printf("**********\n\n");
}




/*
 * Main instruction fetch/decode loop
 */
t_stat sim_instr (void)
{
    t_stat r;
    int ticks;
    int addr_tags, a1, a2, a3, t_sw, op, i;
    t_value m1,m2,m3, t_ra, t_rr;
    char c1,c2,c3;
    double old_delay, instr_time;

    /* Restore register state */
    regKRA = regKRA & MAX_ADDR_VALUE;	        /* mask KRA */
    sim_cancel_step ();				/* defang SCP step */
    delay = 0;

    /* Main instruction fetch/decode loop */
    for (;;) {
	if (sim_interval <= 0) {		/* check clock queue */
  	  r = sim_process_event ();
	 if (r) return r;
	}

	if (regKRA >= MAX_MEM_SIZE) {		/* выход за пределы памяти */
            return STOP_RUNOUT;			/* stop simulation */
	}

	if (sim_brk_summ &&			/* breakpoint? */
	    sim_brk_test (regKRA, SWMASK ('E'))) {
            if (print_stat_on_break) print_commad_run_profile_stat();
	    return STOP_IBKPT;			/* stop simulation */
	}

	regRK = MOSU[regKRA];				/* get instruction */

	op = -1;
	if (print_sys_stat) {
          old_delay = delay;
          op = regRK >> BITS_36 & MAX_OPCODE_VALUE;
	}

	if (sim_deb && cpu_dev.dctrl) {
	    if (disable_is2_trace) {
	      if ((regKRA >= IS2_START_ADDRESS) && (regKRA <= IS2_END_ADDRESS)) goto trace_before_done;
	    }
	    addr_tags = regRK >> BITS_42 & MAX_ADDR_TAG_VALUE;
	    a1 = regRK >> BITS_24 & MAX_ADDR_VALUE;
	    a2 = regRK >> BITS_12 & MAX_ADDR_VALUE;
	    a3 = regRK >> BITS_0  & MAX_ADDR_VALUE;
	    if (addr_tags & 4) a1 = (a1 + regRA) & MAX_ADDR_VALUE;
	    if (addr_tags & 2) a2 = (a2 + regRA) & MAX_ADDR_VALUE;
	    if (addr_tags & 1) a3 = (a3 + regRA) & MAX_ADDR_VALUE;
	    /*fprintf (sim_deb, "*** (%.0f) %04o: ", sim_gtime(), RVK);*/
            if (debug_dump_regs || debug_dump_mem) {
                int i;
                for( i=0; i<100; i++ ) fprintf (sim_deb, "-"); 
                fprintf (sim_deb, "\n"); 
             }
	    fprintf (sim_deb, "cpu: %04o: ", regKRA);
	    fprint_sym (sim_deb, regKRA, &regRK, 0, SWMASK ('M'));
	    fprintf (sim_deb, "\n");
	    if (debug_dump_regs) {
	      t_ra = regRA; t_rr = regRR; t_sw = trgSW;
	      fprintf (sim_deb, "cpu: [dreg]: ra=%04o,  sw=%d,  rr=%015llo\n", t_ra, t_sw, t_rr );
	    }
	    if (debug_dump_mem) {
	      m1 = MOSU[a1]; m2 = MOSU[a2]; m3 = MOSU[a3];
	      fprintf (sim_deb, "cpu: [dmem]: a1[%04o]=%015llo,  a2[%04o]=%015llo,  a3[%04o]=%015llo\n", 
                       a1, m1, a2, m2, a3, m3 );
              if (debug_dump_modern_mem) {
	          fprintf (sim_deb, "cpu: [fmem]: a1[%04o]=%.12f,  a2[%04o]=%.12f,  a3[%04o]=%.12f\n", 
                           a1,m20_to_ieee(MOSU[a1]), a2,m20_to_ieee(MOSU[a2]), a3,m20_to_ieee(MOSU[a3]) );
              }
	    }
            if (debug_dump_regs || debug_dump_mem) fprintf (sim_deb, "\n");
          trace_before_done: {};
	}

	regKRA += 1;				/* increment RVK */

	if (0) fprintf( stderr, "regKRA=%04o\n", regKRA );
	r = cpu_one_inst ();
	//if (r) return r;			/* one instr; error? */
	if (0) fprintf( stderr, "regKRA=%04o\n", regKRA );

	// save some state
        old_opcode = (int) (regRK >> BITS_36) & MAX_OPCODE_VALUE;

	// special check for stop codes
        if ((r == STOP_NEGSQRT) || (r==STOP_CRBADSUM) || (r==STOP_READERR) || (r==STOP_STOP) || 
            (r==STOP_TAPEREADERR)) {
            if (regKRA > 0001) regKRA -= 1;	/* decrement RVK */
            regRK = MOSU[regKRA];
        }
        if ((r==STOP_ASSERT) || (r==STOP_NOCD) || (r == STOP_DIVMOVF) || (r==STOP_DIVZERO)) {
            //regKRA -= 1;	/* decrement RVK */
            regRK = MOSU[regKRA-1];
        }


	if (print_sys_stat) {
          instr_time = delay - old_delay;
          if (instr_time > 0) {
            for( i=0; i<M20_SYM_OPCODE_TABLE_SIZE; i++ ) {
              if (cmd_profile_table[i].op_code == op) {
                  cmd_profile_table[i].us_count += 1;
                  cmd_profile_table[i].us_time  += instr_time;
                  break;
              }
            }
          }
          if (r) {
            print_commad_run_profile_stat();
          }
	}

        if (sim_deb && cpu_dev.dctrl) {
	    if (disable_is2_trace) {
	      if ((regKRA >= IS2_START_ADDRESS) && (regKRA <= IS2_END_ADDRESS)) goto trace_after_done;
	    }
	           if (debug_dump_regs) {
	             c1='-'; c2='-'; c3='-';
	             if (t_ra != regRA) c1 = '*';
	             if (t_sw != trgSW) c2 = '*';
	             if (t_rr != regRR) c3 = '*';
	             fprintf (sim_deb, "cpu: [dreg]: ra=%04o%c, sw=%d%c, rr=%015llo%c\n", 
                              regRA, c1, trgSW, c2, regRR, c3 );
	           }
	           if (debug_dump_mem) {
                     c1='-'; c2='-'; c3='-';
                     if (m1 != MOSU[a1]) c1 = '*';
                     if (m2 != MOSU[a2]) c2 = '*';
                     if (m3 != MOSU[a3]) c3 = '*';
	             fprintf (sim_deb, "cpu: [dmem]: a1[%04o%c]=%015llo, a2[%04o%c]=%015llo, a3[%04o%c]=%015llo\n", 
                              a1, c1, MOSU[a1], a2, c2, MOSU[a2], a3, c3, MOSU[a3] );
                     if (debug_dump_modern_mem) {
	                 fprintf (sim_deb, "cpu: [fmem]: a1[%04o%c]=%.12f,  a2[%04o%c]=%.12f,  a3[%04o%c]=%.12f\n", 
                          a1,c1,m20_to_ieee(MOSU[a1]), a2,c2,m20_to_ieee(MOSU[a2]), a3,c3,m20_to_ieee(MOSU[a3]) );
                     }
	           }
	           if (debug_dump_regs || debug_dump_mem) fprintf (sim_deb, "\n");
          trace_after_done: {};
	}

	//getchar(); 

	ticks = 1;

	if (delay > 0)				/* delay to next instr */
	    //ticks += delay - DBL_EPSILON;
	    ticks += (int)(delay - DBL_EPSILON);

	delay -= ticks;				/* count down delay */
	sim_interval -= ticks;

        if (r) return r;			/* one instr; error? */

	if (sim_step && (--sim_step <= 0))	/* do step count */
	   return SCPE_STOP;
    }

}
