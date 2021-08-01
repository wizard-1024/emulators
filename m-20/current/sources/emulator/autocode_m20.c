/*
 * File:     autocode_m20.c
 * Purpose:  Symbolic assembly coding system for M-20
 *
 * Copyright (c) 2015, Dmitry Stefankov
 *
 * $Id$
 *
 * Revision History.
 *
 *  22-Feb-2015  DVS  Initial Implemementation
 *  23-Feb-2015  DVS  Improved implementation
 *  26-Feb-2015  DVS  Added absolute values to parse for coded instructions
 *  28-Feb-2015  DVS  Added sym_values into numeric values (data lines)
 *  26-Mar-2015  DVS  Minor changes
 *  01-May-2021  DVS  Fixed a bug with data line parsing (bug found by Leonid Yadrennikov).
 *  29-Jun-2021  DVS  Added support of getopt() for xBSD annd compile for xBSD
 *  04-Jul-2021  DVS  Added around for FreeBSD 10.4 on PowerPC Mac G4
 *
 */


#include  "m20_defs.h"
#include  <stdlib.h>
#include  <math.h>
#include  <locale.h>

#if 0
#if _WIN32
#include <windows.h>
#endif
#include  <stdlib.h>
#include  <sys/types.h>
#include  <stdio.h>
#include  <string.h>
#include  <errno.h>
#include  <time.h>
#endif

#if _UNIX
#include <unistd.h>
#endif
#if _WIN32
#include "getopt.h"
#endif


/*------------------------------- GNU C library -----------------------------*/
#if _WIN32
extern int       opterr;
extern int       optind;
extern char     *optarg;
#endif

#if 0
#if !defined(strtoull) 
#define  strtoull  _strtoui64
#endif
#endif


#define  MAX_TEXT_BUF_SIZE  2048


enum encoding_defs {
   AUTO_DETECTION_ENCODING = 0,
   ENGLISH_ASCII_ENCODING = 1,
   RUSSIAN_CP866_ENCODING = 2,
   RUSSIAN_CP1251_ENCODING = 3,
   RUSSIAN_KOI8R_ENCODING = 4,
   RUSSIAN_UTF8_ENCODING = 5
};


/* messages */
#define   MAX_MESSAGE_SIZE    128
#define   MAX_MESSAGES_NUM    1024

typedef  struct  text_message {
  int   msg_idx;
  char  msg_text[MAX_MESSAGE_SIZE+1];
} TEXT_MESSAGE, *PTEXT_MESSAGE;


enum messages_idxs {
    COMPLETE_SUCCESS_IDX    =  1,
    COMPLETE_UNSUCCESS_IDX  =  2,
    MODULE_IDX              =  3,
    TITLE_IDX               =  4,
    AUTHOR_IDX              =  5,
    PAGE_IDX                =  6,
    DATE_TIME_IDX           =  7
};


/* pseudo operations */

#define   MAX_PSEUDO_OP_SIZE     32
#define   MAX_PSEUDO_OPS_NUM    128

enum name_directives {
    NAME_DIRECTIVE       = 1,
    TITLE_DIRECTIVE      = 2,
    START_DIRECTIVE      = 3,
    FINISH_DIRECTIVE     = 4,
    ADDRESS_DIRECTIVE    = 5,
    ABSOLUTE_DIRECTIVE   = 6,
    MEMORY_DIRECTIVE     = 7,
    AUTHOR_DIRECTIVE     = 8,
    LIST_DIRECTIVE       = 9,
    NOLIST_DIRECTIVE     = 10
};

typedef  struct  pseudo_operation {
  int   pseudo_op_idx;
  char  pseudo_op_name[MAX_PSEUDO_OP_SIZE+1];
} PSEUDO_OPERATION, *PPSEUDO_OPERATION;


/* opcodes, symbolic instructions */

#define   MAX_SHORT_SYM_OP_SIZE     32
#define   MAX_LONG_SYM_OP_SIZE     128
#define   MAX_INSTRUCTIONS_NUM      64

typedef  struct  symbolic_operation {
  int   op_code;
  char  short_op_name[MAX_SHORT_SYM_OP_SIZE+1];
  char  long_op_name[MAX_LONG_SYM_OP_SIZE+1];
} SYMBOLIC_OPERATION, *PSYMBOLIC_OPERATION;


/* final general table */

typedef struct sym_tables {
    int                  encoding_type;
    TEXT_MESSAGE         messages_table[MAX_MESSAGES_NUM];
    PSEUDO_OPERATION     pseudo_op_table[MAX_PSEUDO_OPS_NUM];
    SYMBOLIC_OPERATION   sym_op_instr_table[MAX_INSTRUCTIONS_NUM];
} SYM_TABLES, * PSYM_TABLES;


/* Parsing */


#if defined(__USE_AUTOCODE_SMALL_TABLES__)
#define  MAX_PARSED_LINES                 3000
#define  MAX_TEXT_LINE_SIZE                128
#else
#define  MAX_TEXT_LINE_SIZE                2000
#define  MAX_PARSED_LINES                 10000
#endif
#define  MAX_LEXICAL_WORD_SIZE              128
#define  MAX_LEXICAL_WORD_NUM                32
#define  MAX_CHECK_LEXICAL_WORD_NUM           8
#define  MAX_LABEL_NAME_SIZE                 32
#define  MAX_ADDRESS_NAME_SIZE               64
#define  MAX_OPCODE_SYM_SIZE                 32

typedef struct lexical_word {
   int   lex_word_num;
   char  lex_word_value[MAX_LEXICAL_WORD_SIZE+1];
} LEXICAL_WORD, *PLEXICAL_WORD;

typedef struct object_code {
  int      location_addr;
  int      tag_sign;
  int      op;
  int      addr_1;
  int      addr_2;
  int      addr_3;
  t_value  mcode;
} OBJECT_CODE, *POBJECT_CODE;

typedef  struct parsed_line {
  int    line_num;
  int    this_code;
  int    this_data;
  int    do_list;
  int    skip_this_line;
  int    location_addr;
  int    opcode_value;
  int    mod_addr_1;
  int    mod_addr_2;
  int    mod_addr_3;
  char   orig_text_line[MAX_TEXT_LINE_SIZE+1];
  char   cleaned_text_line[MAX_TEXT_LINE_SIZE+1];
  LEXICAL_WORD  lexical_word_array[MAX_LEXICAL_WORD_NUM];
  char   label_name[MAX_LABEL_NAME_SIZE+1];
  char   addr_1_name[MAX_ADDRESS_NAME_SIZE+1];
  char   addr_2_name[MAX_ADDRESS_NAME_SIZE+1];
  char   addr_3_name[MAX_ADDRESS_NAME_SIZE+1];
  char   opcode_sym[MAX_OPCODE_SYM_SIZE+1];
  OBJECT_CODE  obj_code_line;
} PARSED_LINE, *PPARSED_LINE;


#define  MAX_ABS_VALUE_NAME_SIZE    64
#define  MAX_ABS_VALUES_NUM        512

typedef struct absolute_value {
  t_value  abs_value;
  char     abs_name[MAX_ABS_VALUE_NAME_SIZE+1];
} ABSOLUTE_VALUE, *PABSOLUTE_VALUE;


#define  MAX_SYM_VALUE_NAME_SIZE    64
#define  MAX_SYM_VALUES_NUM        512

typedef struct symbolic_value {
  int      sym_value;
  char     sym_name[MAX_SYM_VALUE_NAME_SIZE+1];
} SYMBOLIC_VALUE, *PSYMBOLIC_VALUE;


/* Local data */

extern  int        optind;
extern  int        opterr;
extern  char     * optarg;

char         * out_file = NULL;
char         * in_file = NULL;
char         * list_file = NULL;
int           verbose = 0;
int           quiet = 0;
int           out_address_code = 0;
int           encoding_type = AUTO_DETECTION_ENCODING;
int           debug_parsing = 0;
int           object_build_done = 0;
int           disable_setlocale_call = 0;
int           print_tables_per_listing = 1;

char  m20_eng_tab_filename[]          = "autocode_m20_eng.tab";
char  m20_rus_cp_866_tab_filename[]   = "autocode_m20_dos_cp866.tab";
char  m20_rus_cp_1251_tab_filename[]  = "autocode_m20_win_cp1251.tab";
char  m20_rus_koi8r_tab_filename[]    = "autocode_m20_unix_koi8r.tab";
char  m20_rus_utf8_tab_filename[]     = "autocode_m20_rus_utf8.tab";


const char prog_ver[] = "1.0.0";
const char rcs_id[] = "$Id$";

PSYM_TABLES  p_cur_sym_tables = NULL;

SYM_TABLES  sym_table_eng        = { 0 };
SYM_TABLES  sym_table_rus_cp866  = { 0 };
SYM_TABLES  sym_table_rus_cp1251 = { 0 };
SYM_TABLES  sym_table_rus_koi8r  = { 0 };
SYM_TABLES  sym_table_rus_utf8   = { 0 };

int  read_lines_num = 0;
int  parsed_lines_num = 0;
//PARSED_LINE   parsed_lines_array[MAX_PARSED_LINES] = { 0 };
PARSED_LINE   parsed_lines_array[MAX_PARSED_LINES];

int  abs_values_num = 0;
ABSOLUTE_VALUE  abs_values_table[MAX_ABS_VALUES_NUM] = { 0 };

int  sym_values_num = 0;
SYMBOLIC_VALUE  sym_values_table[MAX_SYM_VALUES_NUM] = { 0 };

int   program_start_address = 1;

char  author_name[128]   = { 0 };
char  module_name[128]   = { 0 };
char  program_title[256] = { 0 };

static unsigned char big_text_buf[MAX_TEXT_BUF_SIZE];
static unsigned char out_text_buf[MAX_TEXT_BUF_SIZE+128];



/*----------------------- Functions ---------------------------------------*/


#if _WIN32 || _UNIX
int sim_toupper (int c)
{
return ((c >= 'a') && (c <= 'z')) ? ((c - 'a') + 'A') : c;
}

int sim_isdigit (int c)
{
return ((c >= '0') && (c <= '9'));
}

/* strcasecmp() is not available on all platforms */
int sim_strcasecmp (const char *string1, const char *string2)
{
size_t i = 0;
unsigned char s1, s2;

while (1) {
    s1 = (unsigned char)string1[i];
    s2 = (unsigned char)string2[i];
    s1 = (unsigned char)sim_toupper (s1);
    s2 = (unsigned char)sim_toupper (s2);
    if (s1 == s2) {
        if (s1 == 0)
            return 0;
        i++;
        continue;
        }
    if (s1 < s2)
        return -1;
    if (s1 > s2)
        return 1;
    }
return 0;
}
#endif


#if !defined(strcasecmp) && !defined(strncasecmp)  && !defined(__FreeBSD__) \
    && !defined(__linux__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
static int strcasecmp( const char * s1, const char * s2 );
static int strncasecmp( const char * s1, const char * s2, size_t n );

int
strcasecmp(s1, s2)
	const char *s1, *s2;
{
	register const u_char
			*us1 = (const u_char *)s1,
			*us2 = (const u_char *)s2;

	while (tolower(*us1) == tolower(*us2++))
		if (*us1++ == '\0')
			return (0);
	return (tolower(*us1) - tolower(*--us2));
}



int
strncasecmp(s1, s2, n)
	const char *s1, *s2;
	register size_t n;
{
	if (n != 0) {
		register const u_char
				*us1 = (const u_char *)s1,
				*us2 = (const u_char *)s2;

		do {
			if (tolower(*us1) != tolower(*us2++))
				return (tolower(*us1) - tolower(*--us2));
			if (*us1++ == '\0')
				break;
		} while (--n != 0);
	}
	return (0);
}
#endif



#if !defined(strcasestr) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
static char * strcasestr( const char * big, const char * little );

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Find the first occurrence of find in s, ignore case.
 */
static char *
strcasestr(s, find)
	register const char *s, *find;
{
	register char c, sc;
	register size_t len;

	if ((c = *find++) != 0) {
		c = tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while ((char)tolower((unsigned char)sc) != c);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}
#endif





/*
 *  Build work tables from data files of selected encoding
 */

void  process_tables_file( char * filename, int encoding, PSYM_TABLES  p_sym_tables )
{

  FILE *  fp = NULL;
  char *  s;
  char *  s1, * s2;
  char    ch;
  size_t  slen, slen2;
  int     section_num = 0;
  int     messages_num = 0;
  int     ps_ops_num = 0;
  int     ops_num = 0;
  int     msg_idx;
  int     opcode;
  int     i;
  int     line_num = 0;
  char    temp_buf[1024];
  char    temp_buf_2[1024];

  if (filename == NULL) return;
  if (p_sym_tables == NULL) return;

  if (verbose) printf( "Processing filename: %s\n", filename );

  fp = fopen( filename, "rt" );
  if (fp == NULL) return;

  memset( big_text_buf, 0, sizeof(big_text_buf) );

  while( fgets(big_text_buf,sizeof(big_text_buf)-1,fp) != NULL) {

       //printf( "%s", big_text_buf );
       line_num++;
       s = strchr(big_text_buf,'\r');
       if (s != NULL) *s = '\0';
       s = strchr(big_text_buf,'\n');
       if (s != NULL) *s = '\0';

       slen = strlen(big_text_buf);
       if (slen < 8) continue;

       /* comment */
       ch = big_text_buf[0];
       if (ch == '#') continue;
       if (ch == ';') continue;
       if (ch == '*') continue;

       if (strcasecmp(big_text_buf,"[MessagesTable]") == 0) {
           section_num = 1;
           continue;
       }
       if (strcasecmp(big_text_buf,"[PseudoOperationsTable]") == 0) {
           section_num = 2;
           continue;
       }
       if (strcasecmp(big_text_buf,"[InstructionTables]") == 0) {
           section_num = 3;
           continue;
       }

       //printf( "section_num=%d\n", section_num );
       memset( temp_buf, 0, sizeof(temp_buf) );
       memset( temp_buf_2, 0, sizeof(temp_buf_2) );

       if (section_num == 1) {
         s = big_text_buf;
         msg_idx = atoi(s);
         s1 = strchr(s,'"');
         if (s1 != NULL) {
           s2 = strchr(s1+1,'"');
           if (s2 != NULL) {
             s1++; s2--;
             slen = s2 - s1 + 1;
             if (slen > (sizeof(temp_buf)-1)) slen = sizeof(temp_buf)-1;
             strncpy( temp_buf, s1, slen );
             //printf( "slen=%d, msg_idx=%d, msg_text='%s'\n", slen, msg_idx, temp_buf );
             if (messages_num < MAX_MESSAGES_NUM ) {
               slen = strlen(temp_buf);
               if (slen > MAX_MESSAGE_SIZE) slen = MAX_MESSAGE_SIZE;
               p_sym_tables->messages_table[messages_num].msg_idx = msg_idx;
               strncpy( p_sym_tables->messages_table[messages_num].msg_text, temp_buf, slen );
               messages_num++;
               continue;
             }
           }
         }
         fprintf( stderr, "ERROR: wrong format per line %d: '%s'\n", line_num, big_text_buf );
         return;
       }


       if (section_num == 2) {
         s = big_text_buf;
         msg_idx = atoi(s);
         s1 = strchr(s,'"');
         if (s1 != NULL) {
           s2 = strchr(s1+1,'"');
           if (s2 != NULL) {
             s1++; s2--;
             slen = s2 - s1 + 1;
             if (slen > (sizeof(temp_buf)-1)) slen = sizeof(temp_buf)-1;
             strncpy( temp_buf, s1, slen );
             //printf( "slen=%d, msg_idx=%d, msg_text='%s'\n", slen, msg_idx, temp_buf );
             if (ps_ops_num < MAX_PSEUDO_OPS_NUM) {
               slen = strlen(temp_buf);
               if (slen > MAX_PSEUDO_OP_SIZE) slen = MAX_PSEUDO_OP_SIZE;
               p_sym_tables->pseudo_op_table[ps_ops_num].pseudo_op_idx = msg_idx;
               strncpy( p_sym_tables->pseudo_op_table[ps_ops_num].pseudo_op_name, temp_buf, slen );
               ps_ops_num++;
               continue;
             }
           }
         }
         fprintf( stderr, "ERROR: wrong format per line %d: '%s'\n", line_num, big_text_buf );
         return;
       }


       if (section_num == 3) {
         s = big_text_buf;
         opcode = strtol(s,NULL,8);
         s1 = strchr(s,'"');
         if (s1 != NULL) {
           s2 = strchr(s1+1,'"');
           if (s2 != NULL) {
             s1++;
             slen = s2 - s1;
             if (slen > (sizeof(temp_buf)-1)) slen = sizeof(temp_buf)-1;
             strncpy( temp_buf, s1, slen );
             s1 = strchr( s2+1, '"');
             if (s1 != NULL) {
               s2 = strchr(s1+1, '"');
               if (s2 != NULL) {
                 s1++;
                 slen = s2 - s1;
                 if (slen > (sizeof(temp_buf_2)-1)) slen = sizeof(temp_buf_2)-1;
                 strncpy( temp_buf_2, s1, slen );
                 //printf( "op=%02o, short='%s' long='%s'\n", opcode, temp_buf, temp_buf_2 );
                 if (ops_num < MAX_INSTRUCTIONS_NUM) {
                   slen = strlen(temp_buf);
                   if (slen > MAX_SHORT_SYM_OP_SIZE) slen = MAX_SHORT_SYM_OP_SIZE;
                   slen2 =strlen(temp_buf_2);
                   if (slen2 > MAX_LONG_SYM_OP_SIZE) slen2 = MAX_LONG_SYM_OP_SIZE;
                   p_sym_tables->sym_op_instr_table[ops_num].op_code = opcode;
                   strncpy( p_sym_tables->sym_op_instr_table[ops_num].short_op_name, temp_buf, slen );
                   strncpy( p_sym_tables->sym_op_instr_table[ops_num].long_op_name, temp_buf_2, slen2 );
                   ops_num++;
                   continue;
                 }
               }
             }
           }
         }
         fprintf( stderr, "ERROR: wrong format per line %d: '%s'\n", line_num, big_text_buf );
         return;
       }

  }

  fclose(fp);

  if (verbose) printf( "messages_num=%d, ps_ops_num=%d, ops_num=%d\n", messages_num, ps_ops_num, ops_num );

  if (debug_parsing) {
    if (ps_ops_num) {
      for(i=0;i<ps_ops_num;i++) {
         printf( "PS_OPS: idx='%02d', name='%s'\n", p_sym_tables->pseudo_op_table[i].pseudo_op_idx,
                     p_sym_tables->pseudo_op_table[i].pseudo_op_name );
      }
    }
  }

  if (ops_num && ps_ops_num && messages_num ) {
     p_sym_tables->encoding_type = encoding;
     if (verbose) printf( "File parsed and accepted.\n" );
  }
}



/*
 * Transform real number into M-20 format.
 *
 * IEEE 754 number presentation (double):
 *	64   63———53  52————–1
 *	sign exponent mantissa
 * High (53th) bit of mantissa is not stored and always equal to 1.
 *
 * Number presentation in M-20:
 *	44   43—--37  36————–1
 *      sign exponent mantissa
 */
t_value ieee_to_m20 (double d)
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
    if (exponent < -64)
	exponent = -64;		/* Nearest number to zero */
    if (exponent > 63) {
	word = 0xfffffffffLL;
	exponent = 63;		/* Max.number */
    }
    word |= ((t_value) (exponent + 64)) << BITS_36;
    word |= (t_value) sign << 43;	/* Sign. */

    return word;
}



int is_delim( int ch )
{
  if (ch == ' ')  return 0;
  if (ch == '\t') return 0;
  if (ch == ',')  return 0;

  return 1;
}



int  is_numeric_exp( char * expr )
{
  int  ret_code = -1;
  char *s = expr;
  int  ch;

  if (s == NULL) return ret_code;

  if (*s == '=') return 0;

  while( *s ) {
     ch = *s++;
     if (!isdigit(ch)) return 1;
  }

  return 0;
}



t_value  search_abs_value_per_abs_values_table( char * abs_name )
{
  t_value m_code = 0;
  int i;

  if (abs_name == NULL) return m_code;

  for( i=0; i<abs_values_num; i++ ) {
     //printf( "%s,%s\n", abs_name,abs_values_table[i].abs_name );
     if (strcasecmp(abs_name,abs_values_table[i].abs_name) == 0) {
       m_code = abs_values_table[i].abs_value;
       return m_code;
     }
  }

  return m_code;
}




int  search_sym_value_per_sym_values_table( char * sym_name )
{
  int i_code = 0;
  int i;

  if (sym_name == NULL) return i_code;

  for( i=0; i<sym_values_num; i++ ) {
     if (strcasecmp(sym_name,sym_values_table[i].sym_name) == 0) {
       i_code = sym_values_table[i].sym_value;
       return i_code;
     }
  }

  return i_code;
}



void  add_new_sym_to_symtab( char * name, int value )
{
    int i;

    if (name == NULL) return;

    if (sym_values_num < MAX_SYM_VALUES_NUM) {
        for( i=0; i<sym_values_num; i++ ) {
          if (strcasecmp(name,sym_values_table[i].sym_name) == 0) {
            fprintf( stderr, "ERROR: duplicate symbolic name found (%s,%d:%s).\n", name,i,sym_values_table[i].sym_name );
            return;
          }
        }
        i = sym_values_num;
        sym_values_table[i].sym_value = value;
        strncpy( sym_values_table[i].sym_name, name, MAX_SYM_VALUE_NAME_SIZE );
        sym_values_num++;
    }
    else {
        fprintf( stderr, "ERROR: symbolic name table is fully occupied (%d,%d).\n", sym_values_num, MAX_SYM_VALUES_NUM );
        return;
    }
}



void  add_new_abs_to_abstab( char * name, t_value value )
{
    int i;

    if (name == NULL) return;

    if (abs_values_num < MAX_ABS_VALUES_NUM) {
        for( i=0; i<abs_values_num; i++ ) {
          if (strcasecmp(name,abs_values_table[i].abs_name) == 0) {
            fprintf( stderr, "ERROR: duplicate absolute value found (%s,%d:%s).\n", name,i,abs_values_table[i].abs_name );
            return;
          }
        }
        i = abs_values_num;
        abs_values_table[i].abs_value = value;
        strncpy( abs_values_table[i].abs_name, name, MAX_ABS_VALUE_NAME_SIZE );
        abs_values_num++;
    }
    else {
        fprintf( stderr, "ERROR: absolute values table is fully occupied (%d,%d).\n", abs_values_num, MAX_ABS_VALUES_NUM );
        return;
    }
}


/*
 *  Skip space and tab
 */
char *skip_spaces (char *p)
{
    while (*p == ' ' || *p == '\t') ++p;

    return p;
}


int  get_octal_digits_num( char *s )
{
  int digits = 0;

  if (s == NULL) return digits;

  while( *s ) {
     if (*s < '0' || *s > '7') goto done;
     s++; digits++;
  }

done:
  return digits;
}


int get_addr_by_sym_or_value_or_expr( int i, int k );


t_value  parse_numeric_expression( int j, int n)
{
  t_value mcode = 0, t_code;
  int l=0, i, m, left_shift;
  char * s, *p;

  for( i=n; i<MAX_CHECK_LEXICAL_WORD_NUM; i++ ) {
     if (parsed_lines_array[j].lexical_word_array[i].lex_word_num > 0) l++;
  }
  //printf( "l=%d\n", l );

  if (l == 1) {
    s = parsed_lines_array[j].lexical_word_array[n].lex_word_value;
    //printf( "s: '%s'\n", s );
    if (*s == '=') {
      double d;
      p = NULL;
      //printf( "s+1: '%s'\n", s+1 );
      d = strtod (s+1, &p);
      //printf( "d: %f\n", d );
      mcode = ieee_to_m20(d);
      //mcode = ieee_to_m20 (strtod (s+1, &p));
      //printf( "mcode: '%015llo', '%s', '%s', %f\n", mcode, s+1, p, strtod (s+1,NULL) );
      return mcode;
    }
    //mcode = strtoull(s,NULL,8);
    p = s;
    if (*p < '0' || *p > '7') goto done;
    mcode = *p - '0';
    for (i=0; i<14; i++) {
      p = skip_spaces(p+1);
      if (*p < '0' || *p > '7') goto done;
      mcode = (mcode << 3) | (*p - '0');
    }
   done:
    return mcode;
  }  

  mcode = 0;
  //printf( "mcode=%015llo\n", mcode );
  left_shift = 0;
  m = 0;

  for( i=n; i<n+5; i++ ) {
      if (parsed_lines_array[j].lexical_word_array[i].lex_word_num > 0) {
       m++;
       //printf( "mcode=%015llo\n", mcode );
       s = parsed_lines_array[j].lexical_word_array[i].lex_word_value;
       //t_code = strtoul(s,NULL,8);
       t_code = get_addr_by_sym_or_value_or_expr(j,i);
       if (i>n) {
         left_shift = get_octal_digits_num(s);
         mcode <<= (left_shift*3);
       }
       mcode |= t_code;
       //printf( "mcode=%015llo, left_shift=%d, s='%s'\n", mcode, left_shift, s );
    }
  }

  if (m == 0) fprintf( stderr, "ERROR: wrong numeric value per line %d.\n", i );

  return mcode;
}




int get_addr_by_sym_or_value_or_expr( int i, int k )
{
  int  add_values = 0, sub_values=0;
  int this_addr = 0, res, value1, value2;
  char * s, *s1, *s2, *p, *s3;
  t_value t;
  char temp_buf[256];

  //if (parsed_lines_array[i].lexical_word_array[k].lex_word_num > 0) {
     //printf( "word='%s'\n", parsed_lines_array[i].lexical_word_array[k].lex_word_value );
     //printf( "S1: %s\n", parsed_lines_array[i].lexical_word_array[k].lex_word_value );
     res = is_numeric_exp(parsed_lines_array[i].lexical_word_array[k].lex_word_value);
     if (res == 0) {
       this_addr = strtoul(parsed_lines_array[i].lexical_word_array[k].lex_word_value,NULL,8);
     }
     else {
       s =  parsed_lines_array[i].lexical_word_array[k].lex_word_value;
       if (strcasecmp(s,"*") == 0) {
         this_addr = parsed_lines_array[i].location_addr;
         goto done;
       }
       memset( temp_buf, 0, sizeof(temp_buf) );
       strncpy( temp_buf, s, sizeof(temp_buf)-1 );
       //printf( "%s\n", temp_buf );
       p = strchr( s, '+');
       if (p != NULL) { add_values = 1; s3=p; }
       p = strchr( s, '-');
       if (p != NULL) { sub_values = 1; s3=p; }
       if (add_values || sub_values) {
         //printf( "%s\n", s );
         s2 = s3; s2++;
         *s3='\0';
         s1 = s;
         //printf( "s1='%s', s2='%s'\n", s1, s2 );
         res = is_numeric_exp(s1);
         if (res == 0) { value1 = strtoul(s1,NULL,8); }
         else {
            if (strcasecmp(s1,"*") == 0) value1 = parsed_lines_array[i].location_addr;
            else {
                t = search_abs_value_per_abs_values_table( s1 );
                if (t == 0) value1 = search_sym_value_per_sym_values_table( s1 );
                else value1 = (int)t;
            }
         }
         res = is_numeric_exp(s2);
         if (res == 0) { value2 = strtoul(s2,NULL,8); }
         else {
            if (strcasecmp(s2,"*") == 0) value2 = parsed_lines_array[i].location_addr;
            else {
                t = search_abs_value_per_abs_values_table( s2 );
                if (t == 0) value2 = search_sym_value_per_sym_values_table( s2 );
                else value2 = (int)t;
            }
         }
         if (add_values) this_addr = value1 + value2;
         if (sub_values) this_addr = value1 - value2;
         //printf( "value1=%04o, value2=%04o, this_addr=%04o, add=%d, sub=%d\n", value1, value2, this_addr, add_values, sub_values );
         goto done;
       }
       s = parsed_lines_array[i].lexical_word_array[k].lex_word_value;
       //printf( "S20: %s\n", s );
       t = search_abs_value_per_abs_values_table( s );
       if (t == 0) this_addr = search_sym_value_per_sym_values_table( s );
       else this_addr = (int)t;
     }
  //}
done:
  return this_addr;
}



int  check_addr_mod( char * addr )
{
  int     result = -1;
  size_t  slen;

  if (addr == NULL) return result;

  result = 1;

  slen = strlen(addr);
  if ((addr[0] == '(') && (addr[slen-1] == ')')) result = 0;

  return result;
}



/*
 *  Parse symbolic assembly file
 */

void  parse_input_assembly_file( char * filename, PSYM_TABLES  p_sym_tables )
{
  FILE *  fp = NULL;
  size_t  slen;
  int     line_idx;
  char *  p_cur_sym;
  char *  s;
  //char *  s1, * s2;
  char    ch;
  int     do_list = 1;
  int     cur_ch;
  int     in_word;
  int     i,j, k, n, m, t;
  int     res;
  int     opcode;
  int     new_addr, this_addr;
  //int     addr_resolve_done;
  size_t  res1, res2;
  int     cur_location_counter;
  t_value m_code;
  //t_value t_code;
  char    temp_buf[2048];
  char    work_buf[2048];
  char    word[128];

  if (filename == NULL) return;
  if (p_sym_tables == NULL) return;

  if (verbose) printf( "Parsing filename: %s\n", filename );

  fp = fopen( filename, "rt" );
  if (fp == NULL) return;

  memset( big_text_buf, 0, sizeof(big_text_buf) );

  while( fgets(big_text_buf,sizeof(big_text_buf)-1,fp) != NULL) {
       //if (debug_parsing) printf( "%s", big_text_buf );
       read_lines_num++;
       s = strchr(big_text_buf,'\r');
       if (s != NULL) *s = '\0';
       s = strchr(big_text_buf,'\n');
       if (s != NULL) *s = '\0';

       if (read_lines_num > MAX_PARSED_LINES) {
         fprintf( stderr, "ERROR: max. parsed lines limit reached (%d,%d).\n", read_lines_num, MAX_PARSED_LINES );
         return;
       }

       slen = strlen(big_text_buf);
       line_idx = read_lines_num-1;

       parsed_lines_array[line_idx].line_num = read_lines_num;
       if (slen > MAX_TEXT_LINE_SIZE) {
         slen = MAX_TEXT_LINE_SIZE;
         fprintf( stderr, "WARNING: line %d cutted for processing (max_line_size=%d)\n", read_lines_num, MAX_TEXT_LINE_SIZE );
       }
       strncpy( parsed_lines_array[line_idx].orig_text_line, big_text_buf, slen );
       //if (debug_parsing) printf( "%d: %s\n", line_idx, parsed_lines_array[line_idx].orig_text_line );

       if (slen < 3 ) continue;
  }

  fclose(fp);

  if (verbose) printf( "Read lines: %d\n", read_lines_num );

  if (debug_parsing) {
    for(i=0;i<read_lines_num;i++) printf( "orig_text: %s\n", parsed_lines_array[i].orig_text_line );
  }

  /* Skip full comments */
   for(i=0;i<read_lines_num;i++) {
      if (parsed_lines_array[i].skip_this_line) continue;
      memset( temp_buf, 0, sizeof(temp_buf) );
      strncpy( temp_buf, parsed_lines_array[i].orig_text_line, sizeof(temp_buf)-1 );
      ch = temp_buf[0];
      //if ((ch == '*') || (ch == ';') || (ch == '#')) {
      if ((ch == '*') || (ch == ';')) {
        parsed_lines_array[i].skip_this_line = 1;
        parsed_lines_array[i].cleaned_text_line[0] = '\0';
      }
   }

  /* Remove comments */
   for(i=0;i<read_lines_num;i++) {
       if (parsed_lines_array[i].skip_this_line == 0) {
         memset( temp_buf, 0, sizeof(temp_buf) );
         strncpy( temp_buf, parsed_lines_array[i].orig_text_line, sizeof(temp_buf)-1 );
         s = strchr( temp_buf, ';' );
         if (s != NULL) {
           *s = '\0';
         }
         slen = strlen(temp_buf);
         strncpy( parsed_lines_array[i].cleaned_text_line, temp_buf, slen );
       }
   }

  if (debug_parsing) {
    for(i=0;i<read_lines_num;i++) printf( "clean_text: %s\n", parsed_lines_array[i].cleaned_text_line );
  }

  /* Skip empty lines */
   for(i=0;i<read_lines_num;i++) {
     if (parsed_lines_array[i].skip_this_line) continue;
      slen = strlen(parsed_lines_array[i].cleaned_text_line);
      if (slen == 0) parsed_lines_array[i].skip_this_line = 1;
   }



  /* Lexical scanning */
  if (verbose) printf( "Lexical scanning.\n" );
  for(i=0;i<read_lines_num;i++) {
     if (parsed_lines_array[i].skip_this_line) continue;
     /* make a work buffer  */
     memset( work_buf, 0, sizeof(work_buf) );
     strncpy( work_buf, parsed_lines_array[i].cleaned_text_line, sizeof(work_buf)-1 );
     slen = strlen(work_buf);
     if (debug_parsing) printf( "work_buf (len=%d): %s\n", slen,work_buf );
     p_cur_sym = &work_buf[0];
     in_word = 0;
     j = 0; k = 0;
     word[0] = '0';
     while( slen-- ) {
       cur_ch = *p_cur_sym++;
       res = is_delim(cur_ch);
       if (res) {
         if (!in_word) {
           memset( word, 0, sizeof(word) );
           j = 0;
           word[j++] = cur_ch;
           in_word = 1;
         }
         else {
           word[j++] = cur_ch;
         }
       }
       else {
         if (in_word) {
           word[j] = '\0';
           if (k < MAX_LEXICAL_WORD_NUM) {
             strncpy( parsed_lines_array[i].lexical_word_array[k].lex_word_value,word,MAX_LEXICAL_WORD_SIZE);
             parsed_lines_array[i].lexical_word_array[k].lex_word_num = k+1;
             k++;
           }
           //printf( "%s\n", word );
           in_word = 0;
         }
       }
     }
     if (in_word) {
       word[j] = '\0';
       //printf( "%s\n", word );
       if (k < MAX_LEXICAL_WORD_NUM) {
           strncpy( parsed_lines_array[i].lexical_word_array[k].lex_word_value,word,MAX_LEXICAL_WORD_SIZE);
           parsed_lines_array[i].lexical_word_array[k].lex_word_num = k+1;
           //k++;
       }
     }
  }

  if (debug_parsing) {
    for(i=0;i<read_lines_num;i++) {
      if (parsed_lines_array[i].skip_this_line) continue;
      printf( "lex_words: " );
      for(k=0;k<MAX_LEXICAL_WORD_NUM;k++) {
        if (parsed_lines_array[i].lexical_word_array[k].lex_word_num > 0) 
          printf( "%d=%s;", parsed_lines_array[i].lexical_word_array[k].lex_word_num,
                            parsed_lines_array[i].lexical_word_array[k].lex_word_value );
      }
      printf( "\n" );
    }
  }


  /* Syntax parsing */
  if (verbose) printf( "Syntax parsing (pass 1).\n" );
  //cur_location_counter = 0;
  cur_location_counter = 1;
  for(i=0;i<read_lines_num;i++) {

      parsed_lines_array[i].do_list = do_list;

      if (parsed_lines_array[i].skip_this_line) continue;

      parsed_lines_array[i].location_addr = (cur_location_counter & MAX_ADDR_VALUE);
      if (debug_parsing) printf( "cur_loc_counter=%04o\n", cur_location_counter );

      /* detect label presence */
      k = 0;
      if (parsed_lines_array[i].lexical_word_array[k].lex_word_num == 1) {
        slen = strlen(parsed_lines_array[i].lexical_word_array[k].lex_word_value);
        ch = parsed_lines_array[i].lexical_word_array[k].lex_word_value[slen-1];
        //printf( "%c\n", ch );
        if (ch == ':') {
           this_addr = parsed_lines_array[i].location_addr;
           strncpy( parsed_lines_array[i].label_name, parsed_lines_array[i].lexical_word_array[k].lex_word_value, slen-1);
           if (debug_parsing) printf( "LABEL: addr=%04o, name=%s\n", this_addr, parsed_lines_array[i].label_name );
           add_new_sym_to_symtab( parsed_lines_array[i].label_name, this_addr );
           k++;  /* next lexical word */
         }
      }


      /* detect pseudo-directive (pseudo-operation) */
      for(j=0; j<MAX_PSEUDO_OPS_NUM; j++ ) {
          if (p_sym_tables->pseudo_op_table[j].pseudo_op_idx ==0) continue;
          //printf( "%d\n", p_sym_tables->pseudo_op_table[j].pseudo_op_idx );
          //printf( "%d, %s, %s\n", p_sym_tables->pseudo_op_table[j].pseudo_op_idx,parsed_lines_array[i].lexical_word_array[k].lex_word_value, p_sym_tables->pseudo_op_table[k].pseudo_op_name );
          res1 = strcasecmp(p_sym_tables->pseudo_op_table[j].pseudo_op_name,parsed_lines_array[i].lexical_word_array[k].lex_word_value);
          res2 = strcasecmp(p_sym_tables->pseudo_op_table[j].pseudo_op_name,parsed_lines_array[i].lexical_word_array[k+1].lex_word_value);
          if ((res1 == 0) || (res2 == 0)) {
              //printf( "%d, %s, %s\n", p_sym_tables->pseudo_op_table[j].pseudo_op_idx,parsed_lines_array[i].lexical_word_array[k].lex_word_value, p_sym_tables->pseudo_op_table[j].pseudo_op_name );

              if (p_sym_tables->pseudo_op_table[j].pseudo_op_idx == NAME_DIRECTIVE) {
                n = k+1;
                if (parsed_lines_array[i].lexical_word_array[n].lex_word_num > 0) {
                  strncpy( module_name, parsed_lines_array[i].lexical_word_array[n].lex_word_value, sizeof(module_name)-1) ;
                  if (debug_parsing) printf( "%s directive: '%s'\n", p_sym_tables->pseudo_op_table[j].pseudo_op_name, module_name );
                  //parsed_lines_array[i].location_addr=cur_location_counter;
                  goto done1;
                }
                fprintf( stderr, "ERROR: missing value per %s directive in line %d.\n", 
                             p_sym_tables->pseudo_op_table[j].pseudo_op_name, parsed_lines_array[i].line_num );
                return;
              }

              if (p_sym_tables->pseudo_op_table[j].pseudo_op_idx == TITLE_DIRECTIVE) {
                n = k+1;
                if (parsed_lines_array[i].lexical_word_array[n].lex_word_num > 0) {
                  for( m=n; m<MAX_LEXICAL_WORD_NUM; m++ ) {
                        if (parsed_lines_array[i].lexical_word_array[m].lex_word_num == 0) continue;
                        strncat( program_title, parsed_lines_array[i].lexical_word_array[m].lex_word_value, sizeof(program_title)-1 );
                        strncat( program_title, " ", sizeof(program_title)-1 );
                  }
                  if (debug_parsing) printf( "%s directive: '%s'\n", p_sym_tables->pseudo_op_table[j].pseudo_op_name, program_title );
                  //parsed_lines_array[i].location_addr=cur_location_counter;
                  goto done1;
                }
                fprintf( stderr, "ERROR: missing value per %s directive in line %d.\n", 
                            p_sym_tables->pseudo_op_table[j].pseudo_op_name, parsed_lines_array[i].line_num );
                return;
              }

              if (p_sym_tables->pseudo_op_table[j].pseudo_op_idx == START_DIRECTIVE) {
                n = k+1;
                if (parsed_lines_array[i].lexical_word_array[n].lex_word_num > 0) {
                  new_addr = strtol(parsed_lines_array[i].lexical_word_array[n].lex_word_value,NULL,8);
                  new_addr &= MAX_ADDR_VALUE;
                  //printf( "new_addr=%04o\n", new_addr );
                  cur_location_counter = new_addr;
                  program_start_address = new_addr;
                  if (debug_parsing) printf( "%s directive:  cur_location_counter=%04o, program_start_address=%04o\n", 
                           p_sym_tables->pseudo_op_table[j].pseudo_op_name, cur_location_counter, program_start_address );
                  //parsed_lines_array[i].location_addr=cur_location_counter;
                  goto done1;
                }
                fprintf( stderr, "ERROR: missing address value per %s directive in line %d.\n", 
                             p_sym_tables->pseudo_op_table[j].pseudo_op_name, parsed_lines_array[i].line_num );
                return;
              }

              if (p_sym_tables->pseudo_op_table[j].pseudo_op_idx == FINISH_DIRECTIVE) {
                n = k+1;
                if (parsed_lines_array[i].lexical_word_array[n].lex_word_num > 0) {
                  new_addr = strtol(parsed_lines_array[i].lexical_word_array[n].lex_word_value,NULL,8);
                  new_addr &= MAX_ADDR_VALUE;
                  //printf( "new_addr=%04o\n", new_addr );
                  program_start_address = new_addr;
                  if (debug_parsing) printf( "%s directive:  program_start_address=%04o\n", 
                                    p_sym_tables->pseudo_op_table[j].pseudo_op_name, program_start_address );
                  //parsed_lines_array[i].location_addr=cur_location_counter;
                  goto done1;
                }
                fprintf( stderr, "ERROR: missing address value per %s directive in line %d.\n", 
                         p_sym_tables->pseudo_op_table[j].pseudo_op_name, parsed_lines_array[i].line_num );
                return;
              }

              //printf( "idx1=%d\n", p_sym_tables->pseudo_op_table[j].pseudo_op_idx );
              if (p_sym_tables->pseudo_op_table[j].pseudo_op_idx == ADDRESS_DIRECTIVE) {
                 n = k+1;
                 if (parsed_lines_array[i].lexical_word_array[n].lex_word_num > 0) {
                   new_addr = strtol(parsed_lines_array[i].lexical_word_array[n].lex_word_value,NULL,8);
                   new_addr &= MAX_ADDR_VALUE;
                   //printf( "new_addr=%04o\n", new_addr );
                   cur_location_counter = new_addr;
                   if (debug_parsing) printf( "%s directive:  cur_location_counter=%04o\n", 
                                  p_sym_tables->pseudo_op_table[j].pseudo_op_name, cur_location_counter );
                   //parsed_lines_array[i].location_addr=cur_location_counter;
                   goto done1;
                 }
                 fprintf( stderr, "ERROR: missing address value per %s directive in line %d.\n", 
                            p_sym_tables->pseudo_op_table[j].pseudo_op_name, parsed_lines_array[i].line_num );
                 return;
              }

              //printf( "idx2=%d\n", p_sym_tables->pseudo_op_table[j].pseudo_op_idx );
              if (p_sym_tables->pseudo_op_table[j].pseudo_op_idx == ABSOLUTE_DIRECTIVE) {
                 n = k+2;
                 //printf( "k=%d, n=%d\n", k, n );
                 if (strcasecmp(p_sym_tables->pseudo_op_table[j].pseudo_op_name,parsed_lines_array[i].lexical_word_array[k+1].lex_word_value) != 0) {
                   fprintf( stderr, "ERROR: missing absolute_value name per %s directive in line %d.\n", 
                            p_sym_tables->pseudo_op_table[j].pseudo_op_name, parsed_lines_array[i].line_num );
                   return;
                 }
                 memset( temp_buf, 0, sizeof(temp_buf) );
                 strncpy( temp_buf, parsed_lines_array[i].lexical_word_array[k].lex_word_value, sizeof(temp_buf)-1 );
                 if (parsed_lines_array[i].lexical_word_array[n].lex_word_num > 0) {
                   m_code = parse_numeric_expression( i, n);
                   add_new_abs_to_abstab( temp_buf, m_code );
                   if (debug_parsing) printf( "abs_name=%s value=%015llo\n", temp_buf, m_code );
                   goto done1;
                 }
                 fprintf( stderr, "ERROR: missing absolute value per %s directive in line %d.\n", 
                            p_sym_tables->pseudo_op_table[j].pseudo_op_name, parsed_lines_array[i].line_num );
                 return;
              }

              if (p_sym_tables->pseudo_op_table[j].pseudo_op_idx == MEMORY_DIRECTIVE) {
                 n = k+1;
                 if (parsed_lines_array[i].lexical_word_array[n].lex_word_num > 0) {
                   new_addr = strtol(parsed_lines_array[i].lexical_word_array[n].lex_word_value,NULL,8);
                   new_addr &= MAX_ADDR_VALUE;
                   //printf( "new_addr=%04o\n", new_addr );
                   cur_location_counter += new_addr;
                   cur_location_counter &= MAX_ADDR_VALUE;
                   if (debug_parsing) printf( "%s directive:  cur_location_counter=%04o\n", 
                                  p_sym_tables->pseudo_op_table[j].pseudo_op_name, cur_location_counter );
                   goto done1;
                 }
                 fprintf( stderr, "ERROR: missing address value per %s directive in line %d.\n", 
                            p_sym_tables->pseudo_op_table[j].pseudo_op_name, parsed_lines_array[i].line_num );
                 return;
              }

              if (p_sym_tables->pseudo_op_table[j].pseudo_op_idx == AUTHOR_DIRECTIVE) {
                n = k+1;
                if (parsed_lines_array[i].lexical_word_array[n].lex_word_num > 0) {
                  for( m=n; m<MAX_LEXICAL_WORD_NUM; m++ ) {
                        if (parsed_lines_array[i].lexical_word_array[m].lex_word_num == 0) continue;
                        strncat( author_name, parsed_lines_array[i].lexical_word_array[m].lex_word_value, sizeof(author_name)-1 );
                        strncat( author_name, " ", sizeof(author_name)-1 );
                  }
                  if (debug_parsing) printf( "%s directive: '%s'\n", p_sym_tables->pseudo_op_table[j].pseudo_op_name, author_name );
                  //parsed_lines_array[i].location_addr=cur_location_counter;
                  goto done1;
                }
                fprintf( stderr, "ERROR: missing value per %s directive in line %d.\n", 
                            p_sym_tables->pseudo_op_table[j].pseudo_op_name, parsed_lines_array[i].line_num );
                return;
              }

              if (p_sym_tables->pseudo_op_table[j].pseudo_op_idx == LIST_DIRECTIVE) {
                  do_list = 1;
                  if (debug_parsing) printf( "%s directive: do_list=%d\n", p_sym_tables->pseudo_op_table[j].pseudo_op_name,do_list );
                  parsed_lines_array[i].do_list = do_list;
                  goto done1;
              }

              if (p_sym_tables->pseudo_op_table[j].pseudo_op_idx == NOLIST_DIRECTIVE) {
                  do_list = 0;
                  if (debug_parsing) printf( "%s directive: do_list=%d\n", p_sym_tables->pseudo_op_table[j].pseudo_op_name,do_list );
                  goto done1;

              }

          } 
     } /*for*/


     /* detect symbolic code operation */
     if (parsed_lines_array[i].lexical_word_array[k].lex_word_num > 0) {
         for(j=0; j<MAX_INSTRUCTIONS_NUM; j++ ) {
             res1 = strcasecmp(p_sym_tables->sym_op_instr_table[j].short_op_name,
                               parsed_lines_array[i].lexical_word_array[k].lex_word_value);
             res2 = strcasecmp(p_sym_tables->sym_op_instr_table[j].long_op_name,
                               parsed_lines_array[i].lexical_word_array[k].lex_word_value);
             if ((res1 == 0) || (res2 == 0)) {
               parsed_lines_array[i].location_addr = cur_location_counter;
               opcode = p_sym_tables->sym_op_instr_table[j].op_code;
               parsed_lines_array[i].opcode_value = opcode;
               parsed_lines_array[i].this_code = 1;
               strncpy( parsed_lines_array[i].opcode_sym, parsed_lines_array[i].lexical_word_array[k].lex_word_value, 
                        MAX_OPCODE_SYM_SIZE );
               m=k+1;
               if (parsed_lines_array[i].lexical_word_array[m].lex_word_num > 0) {
                 //printf( "a1s='%s'\n",parsed_lines_array[i].lexical_word_array[m].lex_word_value);
                 s = parsed_lines_array[i].lexical_word_array[m].lex_word_value;
                 res = check_addr_mod( s);
                 if (res == 0) {
                   memset( temp_buf, 0, sizeof(temp_buf) );
                   slen=strlen(s); strncpy( temp_buf, s+1, slen-2 ); s = temp_buf;
                   //printf( "a1s='%s'\n",s );
                   strncpy( parsed_lines_array[i].lexical_word_array[m].lex_word_value, s, MAX_LEXICAL_WORD_SIZE );
                   parsed_lines_array[i].mod_addr_1=1;
                 }
                 strncpy( parsed_lines_array[i].addr_1_name, s, MAX_ADDRESS_NAME_SIZE );
               }
               m++;
               if (parsed_lines_array[i].lexical_word_array[m].lex_word_num > 0) {
                 s = parsed_lines_array[i].lexical_word_array[m].lex_word_value;
                 res = check_addr_mod( s);
                 if (res == 0) {
                   memset( temp_buf, 0, sizeof(temp_buf) );
                   slen=strlen(s); strncpy( temp_buf, s+1, slen-2 ); s = temp_buf;
                   strncpy( parsed_lines_array[i].lexical_word_array[m].lex_word_value, s, MAX_LEXICAL_WORD_SIZE );
                   parsed_lines_array[i].mod_addr_2=1;
                 }
                 strncpy( parsed_lines_array[i].addr_2_name, s, MAX_ADDRESS_NAME_SIZE );
               }
               m++;
               if (parsed_lines_array[i].lexical_word_array[m].lex_word_num > 0) {
                 s = parsed_lines_array[i].lexical_word_array[m].lex_word_value;
                 res = check_addr_mod( s);
                 if (res == 0) {
                   memset( temp_buf, 0, sizeof(temp_buf) );
                   slen=strlen(s); strncpy( temp_buf, s+1, slen-2 ); s = temp_buf;
                   strncpy( parsed_lines_array[i].lexical_word_array[m].lex_word_value, s, MAX_LEXICAL_WORD_SIZE );
                   parsed_lines_array[i].mod_addr_3=1;
                 }
                 strncpy( parsed_lines_array[i].addr_3_name, s, MAX_ADDRESS_NAME_SIZE );
               }
               if (debug_parsing) printf( "CODE_LINE: line_num=%d op=%02o sym='%s'\n", parsed_lines_array[i].line_num,
                             parsed_lines_array[i].opcode_value, parsed_lines_array[i].opcode_sym );
               cur_location_counter++;   /* next location */
               goto done1;
             }
         }
     }

     /* check for data lines */
     if (parsed_lines_array[i].lexical_word_array[k].lex_word_num > 0) {
         parsed_lines_array[i].this_data = 1;
         parsed_lines_array[i].location_addr = cur_location_counter;
         //parsed_lines_array[i].location_addr = cur_location_counter;
         if (debug_parsing) printf( "DATA_LINE: line_num=%d: %s\n", parsed_lines_array[i].line_num, 
                    parsed_lines_array[i].lexical_word_array[k].lex_word_value );
         cur_location_counter++;   /* next location */
         //continue;
         goto done1;
     }

     //done2:
     //cur_location_counter++;   /* next location */

     done1: {};


  } /*for*/


  if (debug_parsing) {
      printf( "Print symbolic names table (%d names).\n", sym_values_num );
      for( i=0; i<sym_values_num; i++ ) 
          printf( "%d: name=%s value=%04o\n", i, sym_values_table[i].sym_name, sym_values_table[i].sym_value );
      printf( "Print absolute values table (%d names).\n", abs_values_num );
      for( i=0; i<abs_values_num; i++ ) 
          printf( "%d: name=%s value=%015llo\n", i, abs_values_table[i].abs_name, abs_values_table[i].abs_value );
  }


  /* References resolving */
  if (verbose) printf( "References resolving (pass 2).\n" );

  for(i=0;i<read_lines_num;i++) {
      if (parsed_lines_array[i].skip_this_line) continue;

      /* process data lines */
      if (parsed_lines_array[i].this_data) {
        k = 0;
        if (parsed_lines_array[i].label_name[0] != '\0') k++;
        if (debug_parsing) {
          printf( "DATA_LINE: line_num=%d: addr=%04o: ", parsed_lines_array[i].line_num, parsed_lines_array[i].location_addr );
        }
          for( j=k; j<MAX_CHECK_LEXICAL_WORD_NUM; j++) {
            if (parsed_lines_array[i].lexical_word_array[j].lex_word_num > 0)
                if (debug_parsing) printf( "%d='%s';",j,parsed_lines_array[i].lexical_word_array[j].lex_word_value );
                res = is_numeric_exp(parsed_lines_array[i].lexical_word_array[j].lex_word_value);
                if (debug_parsing) printf( "%d='%s'(res=%d);",j,parsed_lines_array[i].lexical_word_array[j].lex_word_value,res );
                if ((res > 0) && (j > 1)) {
                   t_value t1;
                   char s1[16];
                   this_addr = 0;
                   s = parsed_lines_array[i].lexical_word_array[j].lex_word_value;
                   t1 = search_abs_value_per_abs_values_table( s );
                   if (t1 == 0) this_addr = search_sym_value_per_sym_values_table( s );
                   else this_addr = (int)t1;
                   this_addr &= MAX_ADDR_VALUE;
                   if (debug_parsing) printf("this_addr=%04o;",this_addr);
                   _snprintf(s1,sizeof(s1)-1, "%04o",this_addr );
                   strncpy( parsed_lines_array[i].lexical_word_array[j].lex_word_value, s1, 5 );
                   if (debug_parsing) printf( "%d='%s';",j,parsed_lines_array[i].lexical_word_array[j].lex_word_value );
                }
          }
          if (debug_parsing) printf( "\n" );
        //}
        res = is_numeric_exp(parsed_lines_array[i].lexical_word_array[k].lex_word_value);
        //if (1) printf( "is_num_exp_res=%d (lex_value='%s')\n", res, parsed_lines_array[i].lexical_word_array[k].lex_word_value );
        if (res < 0) {
           fprintf( stderr, "ERROR: wrong data value in line %d (%s)\n", parsed_lines_array[i].line_num,
                             parsed_lines_array[i].lexical_word_array[k].lex_word_value );
           return;
        }
        m_code = 0;
        if (res == 0) {
          m_code = parse_numeric_expression( i, k);
          //printf( "m_code=%015llo\n", m_code );
        }
        if (res > 0) {
          m_code = search_abs_value_per_abs_values_table( parsed_lines_array[i].lexical_word_array[k].lex_word_value );
        }
        parsed_lines_array[i].obj_code_line.mcode = m_code;
        parsed_lines_array[i].obj_code_line.location_addr = parsed_lines_array[i].location_addr;
        if (debug_parsing) printf( "FIN_OBJ_MCODE: m_code=%015llo\n", parsed_lines_array[i].obj_code_line.mcode );
        continue;
      }

      /* process code lines */
      if (parsed_lines_array[i].this_code) {
        k = 0;
        if (parsed_lines_array[i].label_name[0] != '\0') k++;
        if (debug_parsing) {
          printf( "CODE_LINE: line_num=%d: addr=%04o: op=%02o: ", parsed_lines_array[i].line_num, 
                  parsed_lines_array[i].location_addr, parsed_lines_array[i].opcode_value );
          for( j=k; j<MAX_CHECK_LEXICAL_WORD_NUM; j++) {
            if (parsed_lines_array[i].lexical_word_array[j].lex_word_num > 0)
                printf( "%d='%s';",j,parsed_lines_array[i].lexical_word_array[j].lex_word_value );
          }
          printf( "\n" );
        }
        parsed_lines_array[i].obj_code_line.op = parsed_lines_array[i].opcode_value;
        t = 0;
        if (parsed_lines_array[i].mod_addr_1) t |= 4;
        if (parsed_lines_array[i].mod_addr_2) t |= 2;
        if (parsed_lines_array[i].mod_addr_3) t |= 1;
        parsed_lines_array[i].obj_code_line.tag_sign = t;
        // address 1
        k++;
        if (parsed_lines_array[i].lexical_word_array[k].lex_word_num > 0) {
          //printf( "a1s='%s'\n", parsed_lines_array[i].lexical_word_array[k].lex_word_value );
          this_addr = get_addr_by_sym_or_value_or_expr( i, k );
          parsed_lines_array[i].obj_code_line.addr_1 = this_addr;
        }
        // address 2
        k++;
        if (parsed_lines_array[i].lexical_word_array[k].lex_word_num > 0) {
          this_addr = get_addr_by_sym_or_value_or_expr( i, k );
          parsed_lines_array[i].obj_code_line.addr_2 = this_addr;
        }
        // address 3
        k++;
        if (parsed_lines_array[i].lexical_word_array[k].lex_word_num > 0) {
          this_addr = get_addr_by_sym_or_value_or_expr( i, k );
          parsed_lines_array[i].obj_code_line.addr_3 = this_addr;
        }
        m_code = 0;
        m_code |= ((t_value)parsed_lines_array[i].obj_code_line.addr_3 << BITS_0); 
        m_code |= ((t_value)parsed_lines_array[i].obj_code_line.addr_2 << BITS_12);
        m_code |= ((t_value)parsed_lines_array[i].obj_code_line.addr_1 << BITS_24);
        m_code |= ((t_value)parsed_lines_array[i].obj_code_line.op << BITS_36);
        m_code |= ((t_value)parsed_lines_array[i].obj_code_line.tag_sign << BITS_42);
        parsed_lines_array[i].obj_code_line.mcode = m_code;
        parsed_lines_array[i].obj_code_line.location_addr = parsed_lines_array[i].location_addr;
        if (debug_parsing) printf( "FIN_OBJ_MCODE: m_code=%015llo\n", parsed_lines_array[i].obj_code_line.mcode );
        continue;
      }

  } /*for*/



}



/*
 *  Produce object M-20 file (M-20 emulator text format)
 */

void  produce_output_object_file( char * filename )
{
  FILE * fp;
  int  i;
  int  cur_loc=1;
  int  ts,op,a1,a2,a3;
  t_value mcode;
  //char temp_buf[128];

  if (filename == NULL) return;

  if (verbose) printf( "Make output filename: %s\n", filename );

  fp = fopen( filename, "wt" );
  if (fp == NULL) return;

  for(i=0;i<read_lines_num;i++) {
      if (parsed_lines_array[i].skip_this_line) {
          fprintf( fp, "%s\n", parsed_lines_array[i].orig_text_line );
          continue;
      }

      if (parsed_lines_array[i].this_code || parsed_lines_array[i].this_data) {
        mcode = parsed_lines_array[i].obj_code_line.mcode;
        if (cur_loc != parsed_lines_array[i].obj_code_line.location_addr) {
          cur_loc = parsed_lines_array[i].obj_code_line.location_addr;
          fprintf( fp, ":%04o\n", cur_loc );
        }
        //fprintf( fp, ";%s\n",   parsed_lines_array[i].cleaned_text_line );
        //fprintf( fp, ";line %d: %s\n", parsed_lines_array[i].line_num, parsed_lines_array[i].orig_text_line );
        //mcode = parsed_lines_array[i].obj_code_line.mcode;
        if (verbose) printf( "out_mcode: addr=%04o code=%015llo\n", cur_loc, mcode );
        ts = (mcode >> BITS_42) & 07;
        op = (mcode >> BITS_36) & 077;
        a1 = (mcode >> BITS_24) & 07777;
        a2 = (mcode >> BITS_12) & 07777;
        a3 = (mcode >> BITS_0) & 07777;
        fprintf( fp, "%01o %02o %04o %04o %04o\n", ts, op, a1, a2, a3 );
#if 0
        fprintf( fp, "%01o %02o %04o %04o %04o\n", 
                 (mcode >> BITS_42) & 07, (mcode >> BITS_36) & 077,
                 (mcode >> BITS_24) & 07777, (mcode >> BITS_12) & 07777, (mcode >> BITS_0) & 07777 );
#endif
        cur_loc += 1;
      }

  }

  fprintf( fp, "\n" );
  fprintf( fp, "@%04o\n", program_start_address );
  fprintf( fp, "\n" );

  object_build_done = 1;

  fclose(fp);

}



char * get_text_message_by_index( int msg_idx )
{
    int  i;
    char * msg = NULL;

    for( i=0; i<MAX_MESSAGES_NUM; i++ ) {
        if (p_cur_sym_tables->messages_table[i].msg_idx == 0) continue;
        if (p_cur_sym_tables->messages_table[i].msg_idx == msg_idx) {
          msg = p_cur_sym_tables->messages_table[i].msg_text;
          return msg;
        }
    }

    return msg;
}




/*
 *  Produce object listing M-20 file (text format)
 */

void  produce_output_listing_file( char * filename )
{
  FILE * fp;
  int  i;
  int  line_num;
  int  cur_loc=1;
  t_value mcode;
  struct tm *  new_time;
  time_t       cur_clock;  
  char * p;
  char * s1, *s2, *s3, *s4, *s5;
  int  ts,op,a1,a2,a3;
  int  lines_on_page = 25;
  int  out_lines = 0;
  int  page_num = 1;
  char  time_buf[64];
  char  header[512];
  char  subheader[512];

  //char  footer[512];

  if (filename == NULL) return;

  if (verbose) printf( "Make listing filename: %s\n", filename );

  fp = fopen( filename, "wt" );
  if (fp == NULL) return;

  // Get time in seconds
  time( &cur_clock );
  // convert time to struct tm form 
  new_time = localtime( &cur_clock );
  memset( time_buf, 0, sizeof(time_buf) );
  _snprintf( time_buf, sizeof(time_buf)-1,"%s", asctime( new_time ) );
  p = strchr( time_buf, '\n' );
  if (p != NULL) *p='\0';

  s1 = get_text_message_by_index(MODULE_IDX);
  s2 = get_text_message_by_index(TITLE_IDX);
  s3 = get_text_message_by_index(AUTHOR_IDX);
  s4 = get_text_message_by_index(PAGE_IDX);
  s5 = get_text_message_by_index(DATE_TIME_IDX);

  memset( header,0,sizeof(header) );
  memset( subheader,0,sizeof(subheader) );

  _snprintf(header,sizeof(header),"%s: %s\t\t\t%s: %s\t\t\t%s: %s", 
                                   s1,module_name,s2,program_title,s3,author_name );
  _snprintf(subheader,sizeof(subheader),"%s: %04d\t\t\t\t\t\t\t\t%s: %s", s4,page_num,s5,time_buf );

  fprintf( fp, "%s\n", header );
  fprintf( fp, "%s\n", subheader );
  fprintf( fp, "\n" );

  for(i=0;i<read_lines_num;i++) {
      if (parsed_lines_array[i].do_list == 0) continue;

      line_num = parsed_lines_array[i].line_num;
      /* print original line */
      fprintf( fp, "%05d:   %s\n", line_num, parsed_lines_array[i].orig_text_line );
      out_lines++;

      /* object code & data */
      if (parsed_lines_array[i].this_code || parsed_lines_array[i].this_data) {
        if (cur_loc != parsed_lines_array[i].obj_code_line.location_addr) {
          cur_loc = parsed_lines_array[i].obj_code_line.location_addr;
          fprintf( fp, "%05d:   :%04o\n", line_num, cur_loc );
          out_lines++;
        }
        mcode = parsed_lines_array[i].obj_code_line.mcode;
        ts = (mcode >> BITS_42) & 07;
        op = (mcode >> BITS_36) & 077;
        a1 = (mcode >> BITS_24) & 07777;
        a2 = (mcode >> BITS_12) & 07777;
        a3 = (mcode >> BITS_0) & 07777;
        fprintf( fp, "%05d:   :%04o  %o %02o %04o %04o %04o\n", line_num, cur_loc, ts, op, a1, a2, a3 );
#if 0
        fprintf( fp, "%05d:   :%04o  %o %02o %04o %04o %04o\n", line_num, cur_loc,
                 (mcode >> BITS_42) & 07, (mcode >> BITS_36) & 077,
                 (mcode >> BITS_24) & 07777, (mcode >> BITS_12) & 07777, (mcode >> BITS_0) & 07777 );
#endif
        cur_loc += 1;
        out_lines++;
      }

      if (out_lines > lines_on_page) {
          out_lines = 1;
          page_num++;
          memset( subheader,0,sizeof(subheader) );
          _snprintf(subheader,sizeof(subheader),"%s: %04d\t\t\t\t\t\t\t\t%s: %s", s4,page_num,s5,time_buf );
          fprintf( fp, "\n" );
          fprintf( fp, "\n" );
          fprintf( fp, "%s\n", header );
          fprintf( fp, "%s\n", subheader );
          fprintf( fp, "\n" );
      }

  }


  if (print_tables_per_listing) {

      fprintf( fp, "\n" );
      fprintf( fp, "\n" );
      fprintf( fp, "\n" );
      fprintf( fp, "\n" );
      fprintf( fp, "\n" );

      page_num++;
      memset( subheader,0,sizeof(subheader) );
      _snprintf(subheader,sizeof(subheader),"%s: %04d\t\t\t\t\t\t\t\t%s: %s", s4,page_num,s5,time_buf );
      fprintf( fp, "\n" );
      fprintf( fp, "\n" );
      fprintf( fp, "%s\n", header );
      fprintf( fp, "%s\n", subheader );
      fprintf( fp, "\n" );

      fprintf( fp, "Symbolic names table - numerical order - (%d entries)\n", sym_values_num );
      for( i=0; i<sym_values_num; i++ ) {
          fprintf( fp, "%04d: %-32s\t%04o\n", i, sym_values_table[i].sym_name, sym_values_table[i].sym_value );
      }

      fprintf( fp, "\n" );
      fprintf( fp, "\n" );
      fprintf( fp, "\n" );
      fprintf( fp, "\n" );
      fprintf( fp, "\n" );

      page_num++;
      memset( subheader,0,sizeof(subheader) );
      _snprintf(subheader,sizeof(subheader),"%s: %04d\t\t\t\t\t\t\t\t%s: %s", s4,page_num,s5,time_buf );
      fprintf( fp, "\n" );
      fprintf( fp, "\n" );
      fprintf( fp, "%s\n", header );
      fprintf( fp, "%s\n", subheader );
      fprintf( fp, "\n" );

      fprintf( fp, "Absolute values table - natural order - (%d entries)\n", abs_values_num ); {
      for( i=0; i<abs_values_num; i++ ) 
          fprintf( fp, "%04d: %-32s\t%015llo\n", i, abs_values_table[i].abs_name, abs_values_table[i].abs_value );
      }
  }

  fclose(fp);

}




 
/*
 *  Print help screen
 */
void usage(void)
{
  fprintf( stderr, "\n" );
  fprintf( stderr, "Symbolic assembly coding system for M-20, version %s\n", prog_ver );
  fprintf( stderr, "Copyright (C) 2015 Dmitry Stefankov. All rights reserved.\n" );
  fprintf( stderr, "Usage: autocode_m20 [-hvapc] [-e enctype] [-i s20-file] [-o m20-file][-l l20-file]\n" );
  fprintf( stderr, "       -h   this help\n" );
  fprintf( stderr, "       -v   verbose output\n" );
  fprintf( stderr, "       -a   output address codes\n" );
  fprintf( stderr, "       -p   debugging messages for parsing phases\n" );
  fprintf( stderr, "       -c   disable setlocale() (Win32)\n" );
  fprintf( stderr, "       -e   encoding type (default=0 (autodetection)\n" );
  fprintf( stderr, "       -i   input file (M-20 symbolic coding file, assembly file)\n" );
  fprintf( stderr, "       -o   output file (M-20 text object file, M-20 emulator format)\n" );
  fprintf( stderr, "       -l   listing file (M-20 object code listing file, w/sym_tables)\n" );
  fprintf( stderr, "Default parameters:\n" );
  fprintf( stderr, "   encoding_types: 0=auto,1=ascii-7,2=cp866,3=cp1251,4=koi8r,5=utf8\n" );
  fprintf( stderr, "   table file for encoding type 1: %s\n", m20_eng_tab_filename );
  fprintf( stderr, "   table file for encoding type 2: %s\n", m20_rus_cp_866_tab_filename );
  fprintf( stderr, "   table file for encoding type 3: %s\n", m20_rus_cp_1251_tab_filename );
  fprintf( stderr, "   table file for encoding type 4: %s\n", m20_rus_koi8r_tab_filename );
  fprintf( stderr, "   table file for encoding type 5: %s\n", m20_rus_utf8_tab_filename );
  fprintf( stderr, "Sample command line:\n" );
  fprintf( stderr, "   ./autocode_m20 -v -i test.s20 -o test.m20\n" );
  fprintf( stderr, "\n" );
  exit(1);
}




/*
 *  Main program stream
 */
int main( int argc, char ** argv )
{
  int                 ret_code = 0;
  int                 op;
  FILE *              fp_in = NULL;
  FILE *              fp_out = NULL;
  char * s1, * s2;

/* Initialize */

/* Process command line  */  
  opterr = 0;
  while( (op = getopt(argc,argv,"acvphe:i:o:l:")) != -1)
    switch(op) {
      case 'e':
               encoding_type = atoi(optarg);
      	       break;       
      case 'o':
               out_file = optarg;
      	       break;       
      case 'i':
               in_file = optarg;
      	       break;       
      case 'l':
               list_file = optarg;
      	       break;       
      case 'a':
               out_address_code = 1;
               break;
      case 'v':
               verbose = 1;
      	       break;       
      case 'p':
               debug_parsing = 1;
               break;
      case 'c':
               disable_setlocale_call = 1;
               break;
      case 'h':
               usage();
               break;   
      default:
               break;
    }

  if (in_file == NULL) {
      fprintf( stderr, "ERROR: input filename not specified.\n" );
      exit(4);
  }

  if (out_file == NULL) {
      fprintf( stderr, "ERROR: output filename not specified.\n" );
      exit(5);
  }

  if (verbose) printf( "encoding type: %d\n", encoding_type );

  /* build work tables from symbolic file */
  process_tables_file( m20_eng_tab_filename, ENGLISH_ASCII_ENCODING, &sym_table_eng);
  process_tables_file( m20_rus_cp_866_tab_filename, RUSSIAN_CP866_ENCODING, &sym_table_rus_cp866);
  process_tables_file( m20_rus_cp_1251_tab_filename, RUSSIAN_CP1251_ENCODING, &sym_table_rus_cp1251);
  process_tables_file( m20_rus_koi8r_tab_filename, RUSSIAN_KOI8R_ENCODING,  &sym_table_rus_koi8r);
  process_tables_file( m20_rus_utf8_tab_filename, RUSSIAN_UTF8_ENCODING,  &sym_table_rus_utf8);

  /* auto detection of encoding to work */

  /* Select encoding tables for processing */
  switch( encoding_type ) {
      case ENGLISH_ASCII_ENCODING:
               p_cur_sym_tables = &sym_table_eng;
               break;
      case RUSSIAN_CP866_ENCODING:
               p_cur_sym_tables = &sym_table_rus_cp866;
#if _WIN32
               if (!disable_setlocale_call) {
                 setlocale(LC_ALL, "rus_rus.866");
                 //SetConsoleCP(866);
                 //SetConsoleOutputCP(866);
               }
#endif
               break;
      case RUSSIAN_CP1251_ENCODING:
               p_cur_sym_tables = &sym_table_rus_cp1251;
#if _WIN32
               if (!disable_setlocale_call) {
                 setlocale(LC_ALL, "rus_rus.1251");
                 //setlocale(LC_ALL, "Russian");
                 //SetConsoleCP(1251);
                 //SetConsoleOutputCP(1251);
               }
#endif
               break;
      case RUSSIAN_KOI8R_ENCODING:
#if _WIN32
               if (!disable_setlocale_call) {
                 //setlocale(LC_ALL, "rus_rus.KOI8");
                 setlocale(LC_ALL, "Russian_Russia.20866");
               }
#endif
               p_cur_sym_tables = &sym_table_rus_koi8r;
               break;
      case RUSSIAN_UTF8_ENCODING:
#if _WIN32
               if (!disable_setlocale_call) {
                 //setlocale(LC_ALL, "en_US.UTF-8");
                 //setlocale(LC_ALL, "ru_RU.UTF-8");
                 setlocale(LC_ALL, "Russian_Russia.65001");
                 //setlocale(LC_ALL, "Russian");
               }
#endif
               p_cur_sym_tables = &sym_table_rus_utf8;
               break;
      default:
               fprintf( stderr, "ERROR: undefined encoding type found.\n" );
               exit(10);
               break;
  }

  if (p_cur_sym_tables == NULL) {
      fprintf( stderr, "ERROR: encoding type was not selected.\n" );
      exit(11);
  }


  if (p_cur_sym_tables->encoding_type == AUTO_DETECTION_ENCODING) {
      fprintf( stderr, "ERROR: work tables not loaded for selected encoding type.\n" );
      exit(12);
  }

  // Zeroing
  memset( parsed_lines_array, 0, sizeof(parsed_lines_array) );

  parse_input_assembly_file( in_file, p_cur_sym_tables );
  produce_output_object_file( out_file );
  if (list_file != NULL) produce_output_listing_file( list_file );

  if (0) goto all_done;

all_done:
  if (fp_in  != NULL) fclose(fp_in);
  if (fp_out != NULL) fclose(fp_out);

  s1 = get_text_message_by_index(COMPLETE_SUCCESS_IDX);
  s2 = get_text_message_by_index(COMPLETE_UNSUCCESS_IDX);

  if (object_build_done) 
    if (verbose) printf( "%s\n", s1 );
  else 
    if (verbose) printf( "%s\n", s2 );

  return(ret_code);
}

