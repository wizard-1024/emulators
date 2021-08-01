/*
 * File:     code2pcard.c
 * Purpose:  Convert M-20 format file to punch card format file
 *
 * Copyright (c) 2015, Dmitry Stefankov
 *
 * $Id$
 *
 * Revision History.
 *
 *  10-Feb-2015  DVS  Initial Implemementation
 *  28-Jun-2021  DVS  Added support of getopt() for xBSD
 *
 */


#include "m20_defs.h"

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


#define  MAX_TEXT_BUF_SIZE  2048

/* Local data */

extern  int        optind;
extern  int        opterr;
extern  char     * optarg;

char         * out_file = NULL;
char         * in_file = NULL;
int           verbose = 0;
int           quiet = 0;
int           out_address_code = 0;

static unsigned char big_text_buf[MAX_TEXT_BUF_SIZE];
static unsigned char out_text_buf[MAX_TEXT_BUF_SIZE+128];


const char prog_ver[] = "1.0.0";
const char rcs_id[] = "$Id$";




/*----------------------- Functions ---------------------------------------*/

 
/*
 *  Print help screen
 */
void usage(void)
{
  fprintf( stderr, "\n" );
  fprintf( stderr, "Convert M-20 format file to punch card format file, version %s\n", prog_ver );
  fprintf( stderr, "Copyright (C) 2015 Dmitry Stefankov. All rights reserved.\n" );
  fprintf( stderr, "Usage: code2pcard [-hva] [-i m20-file] [-o pcard-file]\n" );
  fprintf( stderr, "       -h   this help\n" );
  fprintf( stderr, "       -v   verbose output\n" );
  fprintf( stderr, "       -a   output address code\n" );
  fprintf( stderr, "Default parameters:\n" );
  fprintf( stderr, "Sample command line:\n" );
  fprintf( stderr, "   ./code2pcard -a -i test.m20 -o test.cdr\n" );
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
  size_t              slen;
  FILE *              fp_in = NULL;
  FILE *              fp_out = NULL;
  char *              s;
  char                ch;
  int                 address_code;

/* Initialize */

/* Process command line  */  
  opterr = 0;
  while( (op = getopt(argc,argv,"vhi:o:a")) != -1)
    switch(op) {
      case 'a':
               out_address_code++;
               break;
      case 'o':
               out_file = optarg;
      	       break;       
      case 'i':
               in_file = optarg;
      	       break;       
      case 'v':
               verbose = 1;
      	       break;       
      case 'h':
               usage();
               break;   
      default:
               break;
    }

  if ((out_file == NULL) || (in_file == NULL)) {
       usage();
  }

  fp_in = fopen( in_file, "rt" );
  if (fp_in == NULL) {
    fprintf( stderr, "ERROR: cannot open file %s!\n", in_file );
    return(10);
  }

  fp_out = fopen( out_file, "wt" );
  if (fp_out == NULL) {
    fprintf( stderr, "ERROR: cannot create file %s!\n", out_file );
    ret_code = 11;
    goto all_done;
  }

  memset( big_text_buf, 0, sizeof(big_text_buf) );

  /* read-convert-write */

  while( fgets(big_text_buf,sizeof(big_text_buf)-1,fp_in) != NULL) {

       //printf( "%s", big_text_buf );
       s = strchr(big_text_buf,'\r');
       if (s != NULL) *s = '\0';
       s = strchr(big_text_buf,'\n');
       if (s != NULL) *s = '\0';

       slen = strlen(big_text_buf);

       memset( out_text_buf, 0, sizeof(out_text_buf) );

       if (slen < 2) {
         strncpy( out_text_buf, big_text_buf, sizeof(out_text_buf)-1 );
         goto write_out_buffer;
       }

       ch = big_text_buf[0];

       /* comment */
       if (ch == ';') {
         strncpy( out_text_buf, big_text_buf, sizeof(out_text_buf)-1 );
         goto write_out_buffer;
       }

       /* address code */
       if (ch == ':') {
         //address_code = atoi(&big_text_buf[1]);
         address_code = strtol(&big_text_buf[1],NULL,8);
         //printf( "%04o\n", address_code );
         if (out_address_code)
             _snprintf( out_text_buf, sizeof(out_text_buf)-1,   "0   0 00 %04o 0000 0000   1", address_code );
         else
             _snprintf( out_text_buf, sizeof(out_text_buf)-1, ";;0   0 00 %04o 0000 0000   1", address_code );
         goto write_out_buffer;
       }

       /* data */
       if (ch == '=') {
         s = strchr(big_text_buf, ';' );
         if (s == NULL) 
           _snprintf( out_text_buf, sizeof(out_text_buf)-1,   "1  %s  0", big_text_buf );
         else {
           //s--; s--; *s = '0';
           *s='0'; s++; *s=';';
           _snprintf( out_text_buf, sizeof(out_text_buf)-1,   "1 %s", big_text_buf );
         }
         goto write_out_buffer;
       }

       /* start */
       if (ch == '@') {
         _snprintf( out_text_buf, sizeof(out_text_buf)-1, ";;%s", big_text_buf );
         goto write_out_buffer;
       }

       /* code */
        s = strchr(big_text_buf, ';' );
       if (s == NULL) 
         _snprintf( out_text_buf, sizeof(out_text_buf)-1,   "1   %s   0", big_text_buf );
       else {
         //s--; s--; *s = '0';
         *s='0'; s++; *s=';';
         _snprintf( out_text_buf, sizeof(out_text_buf)-1,   "1   %s", big_text_buf );
       }
       goto write_out_buffer;

    write_out_buffer:
       fprintf( fp_out, "%s\n", out_text_buf );
  }

  fprintf( fp_out, "\n" );
  fprintf( fp_out, "; end-of-input marker and checksum\n" );
  fprintf( fp_out, ";1  0 00 0000 0000 0000   1" );
  fprintf( fp_out, "\n");

all_done:
  if (fp_in  != NULL) fclose(fp_in);
  if (fp_out != NULL) fclose(fp_out);

  return(ret_code);
}
