/*
 * File:     dump_mt.c
 * Purpose:  dump magnetic tape storage in text fromat
 *
 * Copyright (c) 2015, Dmitry Stefankov
 *
 * $Id$
 *
 * Revision History.
 *
 *  04-Mar-2015  DVS  Initial Implemementation
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

char         * in_file = NULL;
int           verbose = 0;
int           quiet = 0;

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
  fprintf( stderr, "Dump magnetic tape storage in text format, version %s\n", prog_ver );
  fprintf( stderr, "Copyright (C) 2015 Dmitry Stefankov. All rights reserved.\n" );
  fprintf( stderr, "Usage: dump_mt [-hv [-i mt-file]\n" );
  fprintf( stderr, "       -h   this help\n" );
  fprintf( stderr, "       -v   verbose output\n" );
  fprintf( stderr, "Default parameters:\n" );
  fprintf( stderr, "Sample command line:\n" );
  fprintf( stderr, "   ./dump_mt  -i mytape.mt0 \n" );
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
  t_value             value;
  size_t              read_count;
  size_t              total_nwords = 0;
  int                 cur_zone_num, cur_zone_size;

/* Initialize */

/* Process command line  */  
  opterr = 0;
  while( (op = getopt(argc,argv,"vhi:")) != -1)
    switch(op) {
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

  if (in_file == NULL) {
       usage();
  }

  fp_in = fopen( in_file, "rb" );
  if (fp_in == NULL) {
    fprintf( stderr, "ERROR: cannot open file %s!\n", in_file );
    return(10);
  }


  printf( "File: %s\n\n", in_file );

  if (verbose) printf( "Dump mtape storage contents.\n" );

  while( 1 ) {
     value = 0;
     read_count = fread( &value, sizeof(value), 1, fp_in );
     if (read_count != 1) {
       break;
     }
     total_nwords++;
     /* extract zone number and length */
     cur_zone_num = value & 0xFFFFFFF;
     cur_zone_size = value >> BITS_32;
     /* bad zone size? */
     if (cur_zone_num,cur_zone_size > MAX_TAPE_ZONE_SIZE) break;
     printf( "****** ZONE %lu, LEN = %lu\n", cur_zone_num, cur_zone_size );
     /* read user words */
     while( cur_zone_size-- ) {
       value = 0;
       read_count = fread( &value, sizeof(value), 1, fp_in );
       if (read_count != 1) {
         //break;
         goto done;
       }
       printf( "%015llo\n", value );
       total_nwords++;
     }
     /* read checksum */
     value = 0;
     read_count = fread( &value, sizeof(value), 1, fp_in );
     if (read_count != 1) {
         break;
     }
     printf( "*** CHKSUM: %015llo\n\n", value );
     total_nwords++;
  }

done:
  if (verbose) printf( "%lu words read from drum.\n", total_nwords );

  if (verbose) printf( "Dump drum storage contents completed.\n" );


  if (fp_in  != NULL) fclose(fp_in);

  return(ret_code);
}
