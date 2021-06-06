/*
 * File:     m20_drm.c
 * Purpose:  M-20 simulator magnetic drum
 *
 * Copyright (c) 2009, Serge Vakulenko
 * Copyright (c) 2014, Dmitry Stefankov
 *
 * $Id$
 *
 * All drum i/o is performed immediately.
 * There is no interrupt system in M20.
 * A real drum timing is implemented.
 *
 * Revision History.
 *
 *  10-Nov-2014  DVS  Initial Implemementation
 *  14-Nov-2014  DVS  Drum operations added
 *  17-Nov-2014  DVS  Cleanup and more debugging
 *                    Removed direct access to CPU core/memory
 *  18-Mov-2014  DVS  cyclic_sum
 *  20-Nov-2014  DVS  Updated. Added MOSU r/w blocking
 *  22-Nov-2014  DVS  Use OR to add code into print buffer
 *  20-Dec-2014  DVS  Added more strong checks for r/w ops
 *  14-Jan-2015  DVS  Fixed a problem for a drum numbering
 *  16-Jan-2015  DVS  Added drum mapping, access modes
 *  26-Feb-2015  DVS  Added auto skip zero address on read/write ops
 *  27-Feb-2015  DVS  Move auto skip zero address option into DRUM module
 *  06-Mar-2015  DVS  Added drum read/write data dump debugging option
 *  08-Mar-2015  DVS  Added more checksum control logic
 *
 */


#include "m20_defs.h"



/*
 * External devices exchange parameters
 */
int ext_io_op;			/* УЧ - условное число */
int ext_io_dev_zone_addr;	/* А_МЗУ - начальный адрес на барабане/ленте/буфере_печ. */
int ext_io_ram_start;		/* Н_МОЗУ - начальный адрес памяти */
int ext_io_ram_end;		/* К_МОЗУ - конечный адрес памяти */
int ext_io_ram_jump;            /* П_МОЗУ - адрес памяти передачи упрваления если нет совпадения кс */
int ext_io_ram_chksum;          /* КС_МОЗУ - адрес памяти для запис КС */


t_stat drum_svc (UNIT *uptr);
t_stat drum_reset (DEVICE *dptr);
t_stat drum_attach (UNIT *uptr, char *cptr);
t_stat drum_detach (UNIT *uptr);

static int drum_map_check = 1;
static int drum_auto_skip_zero_address = 1;
static int drum_read_data_dump = 0;
static int drum_write_data_dump = 0;

static int log_drum_map_array[MAX_LOG_DRUM_COUNT] = { 0,1,2,3 };

static int drum_access_mode_array[MAX_PHYS_DRUM_COUNT+1] = {0, 
                                                            DRUM_READ_MODE|DRUM_WRITE_MODE,
                                                            DRUM_READ_MODE|DRUM_WRITE_MODE,
                                                            DRUM_READ_MODE|DRUM_WRITE_MODE };


/*
 * DRUM data structures
 *
 * drum_dev	DRUM device descriptor
 * drum_unit	DRUM unit descriptor
 * drum_reg	DRUM register list
 */
UNIT drum_unit[] = {
	{ UDATA (&drum_svc, UNIT_FIX+UNIT_ATTABLE, DRUM_SIZE) },
	{ UDATA (&drum_svc, UNIT_FIX+UNIT_ATTABLE, DRUM_SIZE) },
	{ UDATA (&drum_svc, UNIT_FIX+UNIT_ATTABLE, DRUM_SIZE) },
};

REG drum_reg[] = {
        { DRDATA (DRUM_AUTO_SKIP_ZERO_ADDRESS, drum_auto_skip_zero_address, 8), PV_LEFT },
        { DRDATA (DRUM_MAP_CHECK, drum_map_check, 8), PV_LEFT },
        { DRDATA (DRUM_READ_DATA_DUMP, drum_read_data_dump, 8), PV_LEFT },
        { DRDATA (DRUM_WRITE_DATA_DUMP, drum_write_data_dump, 8), PV_LEFT },
        { DRDATA (LOG_DRUM_0_MAP, log_drum_map_array[0], 16), PV_LEFT },
        { DRDATA (LOG_DRUM_1_MAP, log_drum_map_array[1], 16), PV_LEFT },
        { DRDATA (LOG_DRUM_2_MAP, log_drum_map_array[2], 16), PV_LEFT },
        { DRDATA (LOG_DRUM_3_MAP, log_drum_map_array[3], 16), PV_LEFT },
        { DRDATA (DRUM_0_ACCESS_MODE, drum_access_mode_array[0], 16), PV_LEFT },
        { DRDATA (DRUM_1_ACCESS_MODE, drum_access_mode_array[1], 16), PV_LEFT },
        { DRDATA (DRUM_2_ACCESS_MODE, drum_access_mode_array[2], 16), PV_LEFT },
        { DRDATA (DRUM_3_ACCESS_MODE, drum_access_mode_array[3], 16), PV_LEFT },
	{ NULL }
};

MTAB drum_mod[] = {
	{ 0 }
};


DEVICE drum_dev = {
	"DRUM", drum_unit, drum_reg, drum_mod,
	3, 8, 12, 1, 8, 45,
	NULL, NULL, &drum_reset, NULL,
        &drum_attach, &drum_detach, NULL,
	DEV_DISABLE | DEV_DEBUG
};


/* external references (CPU module) */

extern void mosu_store (int addr, t_value val);
extern t_value mosu_load (int addr);

extern t_value  cyclic_checksum( t_value x, t_value y);


/* internal data */
static  t_value  temp_drum_buf[DRUM_SIZE+1];



/* storage for print and punch */

t_value  msu_drum_print_buf[MSU_DRUM_PRINT_BUF_SIZE] = {0};
int  msu_drum_print_last_pos = 0;


t_stat put_code_into_cbuf_reg( t_value ncode)
{
    //msu_drum_print_buf[msu_drum_print_last_pos] = ncode;
    msu_drum_print_buf[msu_drum_print_last_pos] |= ncode;
    msu_drum_print_last_pos++;
    if (msu_drum_print_last_pos >= MSU_DRUM_PRINT_BUF_SIZE) msu_drum_print_last_pos=0;

    return SCPE_OK;
}




/* Drum storage routines */



/*
 * Event: drum i/o operation completed.
 */
t_stat drum_svc (UNIT *u)
{
    if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: drum_svc(..)\n");

    return SCPE_OK;
}


/*
 * Reset routine
 */
t_stat drum_reset (DEVICE *dptr)
{
    uint32 i;

    if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: drum_reset(..)\n");

    ext_io_op = 07777;
    for (i = 0; i < dptr->numunits; i++)
         sim_cancel (dptr->units + i);

    return SCPE_OK;
}



/*
 *  Device attach routine
 */
t_stat drum_attach (UNIT *uptr, char *cptr)
{
    t_stat s;

    sim_cancel(uptr);				           /* cancel current IO */
   
    s = attach_unit (uptr, cptr);

    if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: drum_attach(..), name='%s' res=%d\n", cptr, s);

    return s;
}



/*
 *  Device detach routine
 */
t_stat drum_detach (UNIT *uptr)
{

    if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: drum_detach(..)\n");

    sim_cancel(uptr);

    return detach_unit (uptr);
}




/*
 * Запись на барабан.
 * Если параметр sum ненулевой, посчитываем и кладём туда контрольную
 * сумму массива. Также запмсываем сумму в слово last+1 на барабане.
 */
t_stat drum_write (int drum_no, int addr, int first, int last, t_value *sum,int * ocodes,int no_mosu_access,
                   int disable_control)
{
    int nwords, i, res, chksum_word;
    size_t count;
    t_value chksum;

    if (sim_deb && drum_dev.dctrl) 
        fprintf (sim_deb, "drm: drum_write(%d,%05o,%04o,%04o,..)\n", drum_no, addr, first, last);

    if (drum_auto_skip_zero_address && (first==0) && (last>0)) first++;
    nwords = last - first + 1;

    /* Wrong length for drum writing? */
    chksum_word = 0;
    if (!disable_control && sum) chksum_word = 1;
    if (nwords <= 0 || ((nwords+addr+chksum_word) > DRUM_SIZE)) return STOP_BADWLEN;

    if (sim_deb && drum_dev.dctrl)
        fprintf (sim_deb, "drm: writing MD %05o mem_region %04o-%04o\n", addr, first, last);

    if (sim_deb && drum_dev.dctrl) {
      fprintf (sim_deb, "drm: write: no_mosu_access=%d, disable_control=%d\n", no_mosu_access, disable_control );
    }

    for( i=0; i<nwords; i++ ) {
       //if (sim_deb && drum_dev.dctrl);
       //    fprintf (sim_deb, "drm: mosu_load[%04o]=%015llo\n", first+i, mosu_load(first+i) );
       if (no_mosu_access)  temp_drum_buf[first+i] = 0;
       else  temp_drum_buf[first+i] = mosu_load(first+i);
    }

    if (sim_deb && drum_dev.dctrl) {
      if (drum_write_data_dump && nwords) {
        for( i=0; i<nwords; i++ ) fprintf (sim_deb, "drm: write_value=%015llo\n", temp_drum_buf[first+i]);
      }
    }

    if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: seek file_pos=%llu\n", addr*sizeof(t_value));
    res = fseek (drum_unit[drum_no].fileref, addr*sizeof(t_value), SEEK_SET);
    if (res) return SCPE_IOERR;

    if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: nwords=%04o\n", nwords);

    count = fxwrite (&temp_drum_buf[first], sizeof(t_value), nwords, drum_unit[drum_no].fileref);
    if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: write_count=%04o\n", count);
    if (ocodes && (count <= nwords)) *ocodes = (int)count;
    if (ferror (drum_unit[drum_no].fileref)) return SCPE_IOERR;
    if (count != nwords) return SCPE_IOERR;

    if (sum) {
	/* Compute and write checksum */
        chksum = 0;
	for (i=first; i<=last; ++i) {
            //if (sim_deb && drum_dev.dctrl)
	    //  fprintf (sim_deb, "drm: temp_buf[%04o]=%015llo\n", i, temp_drum_buf[i] );
            chksum = cyclic_checksum (chksum, temp_drum_buf[i]);
        }
        if (sim_deb && drum_dev.dctrl) {
          if (drum_write_data_dump) fprintf (sim_deb, "drm: write_value=%015llo\n", chksum);
        }
        if (!disable_control) {
          count = fxwrite (&chksum, sizeof(t_value), 1, drum_unit[drum_no].fileref);
          if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: write_count=%04o\n", count);
          if (ferror (drum_unit[drum_no].fileref)) return SCPE_IOERR;
          if (count != 1) return SCPE_IOERR;
          if (ocodes) *ocodes += (int)count;
        }
        else if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: write_count=0\n");
        if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: chksum=%015llo (0x%016llX)\n", chksum,chksum);
        if (sum) *sum = chksum; 
    }

    if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: writing_done\n");

    return SCPE_OK;
}



/*
 *  Magnetic drum reading
 */
t_stat drum_read (int drum_no, int addr, int first, int last, t_value *sum,int * ocodes,int no_mosu_access,
                  int disable_control)
{
    int nwords, i, res, chksum_word;
    t_value old_sum;
    size_t count;
    t_value chksum;

    if (sim_deb && drum_dev.dctrl)
	fprintf (sim_deb, "drm: drum_read(%d,%05o,%04o,%04o,..)\n", drum_no, addr, first, last);

    if (drum_auto_skip_zero_address && (first==0) && (last>0)) first++;
    nwords = last - first + 1;

    /* Wrong lengh during drum reading */
    chksum_word = 0;
    if (!disable_control && sum) chksum_word = 1;
    if (nwords <= 0 || ((nwords+addr+chksum_word) > DRUM_SIZE)) return STOP_BADRLEN;

    if (sim_deb && drum_dev.dctrl) 
        fprintf (sim_deb, "drm: reading MD %05o mem_region %04o-%04o\n", addr, first, last);

    if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: seek file_pos=%llu\n", addr*sizeof(t_value));
    res = fseek (drum_unit[drum_no].fileref, addr*8, SEEK_SET);
    if (res) return SCPE_IOERR;

    count = (int)fxread (&temp_drum_buf[first], sizeof(t_value), nwords, drum_unit[drum_no].fileref);
    if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: read_count=%04o\n", count);
    if (ocodes && (count <= nwords)) *ocodes = (int)count;
    if (ferror (drum_unit[drum_no].fileref)) return SCPE_IOERR;

    if (sim_deb && drum_dev.dctrl) {
      if (drum_read_data_dump && (count <= nwords)) {
        for( i=0; i<count; i++ ) fprintf (sim_deb, "drm: read_value=%015llo\n", temp_drum_buf[first+i]);
      }
    }

    if (sim_deb && drum_dev.dctrl) {
      fprintf (sim_deb, "drm: read: no_mosu_access=%d, disable_control=%d\n", no_mosu_access, disable_control );
    }

    if (count <= nwords) {
        for( i=0; i<count; i++ ) {
            if (!no_mosu_access)  mosu_store(first+i,temp_drum_buf[first+i]);
        }
    }

    /* Reading uninitialized drum storage */
    if (count != nwords) return STOP_DRUMINVDATA;

    if (sum) {
	/* Read and test checksum  */
	old_sum = 0;
	count = fxread (&old_sum, sizeof(t_value), 1, drum_unit[drum_no].fileref);
        if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: read_count=%04o\n", count);
        if (!disable_control) {
          if (ferror (drum_unit[drum_no].fileref)) return SCPE_IOERR;
          if (count != 1) return SCPE_IOERR;
        }
        if (ocodes) *ocodes += (int)count;
        if (sim_deb && drum_dev.dctrl) {
          if (drum_read_data_dump) fprintf (sim_deb, "drm: read_value=%015llo\n", old_sum);
        }
	chksum = 0;
        for (i=first; i<=last; ++i) {
            chksum = cyclic_checksum (chksum, temp_drum_buf[i]);
        }   
        if (sim_deb && drum_dev.dctrl) 
            fprintf (sim_deb, "drm: old_sum=%015llo chksum=%015llo\n", old_sum, chksum);
        if (sum) *sum = chksum; 
	if (!disable_control && (old_sum != *sum)) return STOP_READERR;
    }

    if (sim_deb && drum_dev.dctrl) fprintf (sim_deb, "drm: reading_done\n");

    return SCPE_OK;
}



/*
 * Execute drum i/o operationВыполнение.
 * All parameters are containg in operands CW, A_MSU, S_MOSU, E_MOSU.
 */
t_stat drum_io(t_value *sum, int * ocodes)
{
    int user_drum_no, drum_no, i, j, drum_chk;

    /* test logical drum number */

    user_drum_no = (ext_io_op & EXT_UNIT);
    if (sim_deb && drum_dev.dctrl) {
	fprintf (sim_deb, "drm: drum_io(..), user_drum_no=%d\n", user_drum_no );
	for( i=0;i<MAX_LOG_DRUM_COUNT;i++) fprintf (sim_deb, "drm: drum_io(..), log_drum_map_array[%d]=%d\n", 
                                                    i, log_drum_map_array[i] );
    }

    if (user_drum_no < 0) return STOP_EXTINVAL;
    if (user_drum_no >= MAX_LOG_DRUM_COUNT) return STOP_EXTINVAL;

    /* make a map of logical drum number to physical drum number */

    drum_no = log_drum_map_array[user_drum_no];
    if (sim_deb && drum_dev.dctrl)
	fprintf (sim_deb, "drm: drum_io(..), drum_no=%d user_drum_no=%d\n", drum_no, user_drum_no);
	
    if (drum_no < MIN_PHYS_DRUM_NUM) return STOP_EXTINVAL;
    if (drum_no > MAX_PHYS_DRUM_NUM) return STOP_EXTINVAL;

    /* check drum mapping: cannot be 2 or more logical drums to be mapped to one physical drum */
    if (drum_map_check) {
      for( i=0; i<MAX_LOG_DRUM_COUNT; i++ ) {
        drum_chk=log_drum_map_array[i];
        for( j=i+1;j<MAX_LOG_DRUM_COUNT; j++ ) {
          if ( i!=j ) {
            if (drum_chk == log_drum_map_array[j]) {
             if (sim_deb && drum_dev.dctrl) 
                 fprintf (sim_deb, "drm: drum_io(..), switch error: i=%d, j=%d\n", i,j );
              return STOP_DRUM_MAP_ERROR;
            }
          }
        }/*for*/
      }/*for*/
    }


    drum_no = drum_no - 1;
    if (sim_deb && drum_dev.dctrl)
	fprintf (sim_deb, "drm: drum_io(..), drum_no=%d user_drum_no=%d\n", drum_no, user_drum_no);

    /* check access mode for selected physical drum */

    if (sim_deb && drum_dev.dctrl) {
	for( i=0;i<=MAX_PHYS_DRUM_COUNT;i++) fprintf (sim_deb, "drm: drum_io(..), drum_access_mode_array[%d]=%d\n", 
                                                      i, drum_access_mode_array[i] );
    }

    if ((ext_io_op & EXT_WRITE) && (!(drum_access_mode_array[drum_no] & DRUM_WRITE_MODE))) {
      return STOP_DRUM_NOT_IN_WRITE_MODE;
    }
    else {
      if (!(drum_access_mode_array[drum_no] & DRUM_READ_MODE)) {
        return STOP_DRUM_NOT_IN_READ_MODE;
      }
    }

    if ((drum_dev.flags & DEV_DIS) || !drum_unit[drum_no].fileref) {
	/* Device not attached. */
	return SCPE_UNATT;
    }

    if (ext_io_op & EXT_WRITE) {
	return drum_write (drum_no, ext_io_dev_zone_addr, ext_io_ram_start, ext_io_ram_end,
			  //(ext_io_op & EXT_DIS_CHECK) ? 0 : sum, ocodes, ext_io_op & EXT_DIS_RAM ? 1 : 0);
                          sum, ocodes, ext_io_op & EXT_DIS_RAM ? 1 : 0, (ext_io_op & EXT_DIS_CHECK) ? 1 : 0);
    } else {
	return drum_read (drum_no, ext_io_dev_zone_addr, ext_io_ram_start, ext_io_ram_end,
		         //(ext_io_op & EXT_DIS_CHECK) ? 0 : sum, ocodes, ext_io_op & EXT_DIS_RAM ? 1 : 0);
                         sum, ocodes, ext_io_op & EXT_DIS_RAM ? 1 : 0, (ext_io_op & EXT_DIS_CHECK) ? 1 : 0);
    }

}


