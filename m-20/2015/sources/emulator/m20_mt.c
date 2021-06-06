/*
 * File:     m20_mt.c
 * Purpose:  M-20 simulator magnetic tape
 *
 * Copyright (c) 2014, Dmitry Stefankov
 *
 * $Id$
 *
 * Revision History.
 *
 *  10-Nov-2014  DVS  Initial Implemementation
 *  14-Nov-2014  DVS  Added tape format
 *  17-Nov-2014  DVS  Cleanup and more debugging
 *  18-Nov-2014  DVS  cyclic_sum
 *  20-Nov-2014  DVS  Fixed tape format routine
 *  23-Nov-2014  DVS  Minor changes
 *  20-Dec-2014  DVS  Added more strong checks for r/w ops
 *  16-Jan-2015  DVS  Added tape mapping, access modes
 *  02-Feb-2015  DVS  Added missing file seek to format function
 *  26-Feb-2015  DVS  Added auto skip zero address on read/write ops
 *  27-Feb-2015  DVS  Move auto skip zero address option into MT module
 *  08-Mar-2015  DVS  Added more checksum control logic
 *                    Added tape read/write data dump debugging option
 *
 */


#include "m20_defs.h"




/*
 * Параметры обмена с внешним устройством.
 */
int ext_io_op;			/* УЧ - условное число */
int ext_io_dev_zone_addr;	/* А_МЗУ - начальный адрес на барабане/ленте/буфере_печ. */
int ext_io_ram_start;		/* Н_МОЗУ - начальный адрес памяти */
int ext_io_ram_end;		/* К_МОЗУ - конечный адрес памяти */
int ext_io_ram_jump;            /* П_МОЗУ - адрес памяти передачи упрваления если нет совпадения кс */
int ext_io_ram_chksum;          /* КС_МОЗУ - адрес памяти для запис КС */


t_stat mt_svc (UNIT *uptr);
t_stat mt_reset (DEVICE *dptr);
t_stat mt_attach (UNIT *uptr, char *cptr);
t_stat mt_detach (UNIT *uptr);

static int tape_auto_skip_zero_address = 1;
static int tape_map_check = 1;

static int tape_read_data_dump = 0;
static int tape_write_data_dump = 0;
static int tape_format_data_dump = 0;

static int log_tape_map_array[MAX_TAPES_COUNT] = { 0,1,2,3 };

static int tape_access_mode_array[MAX_TAPES_COUNT] = {TAPE_READ_MODE|TAPE_WRITE_MODE|TAPE_FORMAT_MODE,
                                                      TAPE_READ_MODE|TAPE_WRITE_MODE|TAPE_FORMAT_MODE,
                                                      TAPE_READ_MODE|TAPE_WRITE_MODE|TAPE_FORMAT_MODE,
                                                      TAPE_READ_MODE|TAPE_WRITE_MODE|TAPE_FORMAT_MODE};


/* 
   MT data structures

   mt_dev       MT device descriptor
   mt_unit      MT unit list
   mt_reg       MT register list
   mt_mod       MT modifier list
*/

UNIT mt_unit[] = {
	{ UDATA (&mt_svc, UNIT_FIX+UNIT_ATTABLE, MAX_TAPE_SIZE) },
	{ UDATA (&mt_svc, UNIT_FIX+UNIT_ATTABLE, MAX_TAPE_SIZE) },
	{ UDATA (&mt_svc, UNIT_FIX+UNIT_ATTABLE, MAX_TAPE_SIZE) },
	{ UDATA (&mt_svc, UNIT_FIX+UNIT_ATTABLE, MAX_TAPE_SIZE) },
};


REG mt_reg[] = {
        { DRDATA (TAPE_AUTO_SKIP_ZERO_ADDRESS, tape_auto_skip_zero_address, 8), PV_LEFT },
        { DRDATA (TAPE_MAP_CHECK, tape_map_check, 8), PV_LEFT },
        { DRDATA (TAPE_READ_DATA_DUMP, tape_read_data_dump, 8), PV_LEFT },
        { DRDATA (TAPE_WRITE_DATA_DUMP, tape_write_data_dump, 8), PV_LEFT },
        { DRDATA (TAPE_FORMAT_DATA_DUMP, tape_format_data_dump, 8), PV_LEFT },
        { DRDATA (LOG_TAPE_0_MAP, log_tape_map_array[0], 16), PV_LEFT },
        { DRDATA (LOG_TAPE_1_MAP, log_tape_map_array[1], 16), PV_LEFT },
        { DRDATA (LOG_TAPE_2_MAP, log_tape_map_array[2], 16), PV_LEFT },
        { DRDATA (LOG_TAPE_3_MAP, log_tape_map_array[3], 16), PV_LEFT },
        { DRDATA (TAPE_0_ACCESS_MODE, tape_access_mode_array[0], 16), PV_LEFT },
        { DRDATA (TAPE_1_ACCESS_MODE, tape_access_mode_array[1], 16), PV_LEFT },
        { DRDATA (TAPE_2_ACCESS_MODE, tape_access_mode_array[2], 16), PV_LEFT },
        { DRDATA (TAPE_3_ACCESS_MODE, tape_access_mode_array[3], 16), PV_LEFT },
	{ NULL }
};

MTAB mt_mod[] = {
	{ 0 }
};

DEVICE mt_dev = {
	"MT", mt_unit, mt_reg, mt_mod,
	4, 8, 12, 1, 8, 45,
	NULL, NULL, &mt_reset, NULL,
        &mt_attach, &mt_detach, NULL,
	DEV_DISABLE | DEV_DEBUG
};


/* external references (CPU module) */

extern t_value mosu_load (int addr);
extern void mosu_store (int addr, t_value val);

extern t_value  cyclic_checksum( t_value x, t_value y);


/* internal data */

static t_value  temp_zone_buf[MAX_TAPE_ZONE_SIZE+1];


/*
 *  Событие: закончен обмен с МЛ.
 */
t_stat mt_svc (UNIT *u)
{
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_svc(..)\n");

    return SCPE_OK;
}


/*
 *  Reset routine
 */
t_stat mt_reset (DEVICE *dptr)
{
    uint32 i;

    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_reset(..)\n");

    ext_io_op = 07777;
    for (i = 0; i < dptr->numunits; i++)
         sim_cancel (dptr->units + i);

    return SCPE_OK;
}



/*
 *  Device attach routine
 */
t_stat mt_attach (UNIT *uptr, char *cptr)
{
    t_stat s;

    sim_cancel(uptr);				           /* cancel current IO */
   
    s = attach_unit (uptr, cptr);

    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_attach(..), name='%s' res=%d\n", cptr, s);

    return s;
}



/*
 *  Device detach routine
 */
t_stat mt_detach (UNIT *uptr)
{

    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_detach(..)\n");

    sim_cancel(uptr);

    return detach_unit (uptr);
}




/*
 *  Разметка магнитной ленты (МЛ) или форматирование
 */
t_stat mt_format_tape (t_value *sum, int * ocodes, int first, int last)
{
    int  res;
    int  i;
    int  codes_group_size;
    int  zone_num;
    int  mt_no;
    int  last_fmt_pos;
    t_value  temp_value;
    t_value chksum;
    size_t count;
    int codes_num = 0;
    int no_mosu_access = 0;
    int user_mt_no, tape_chk, j;
    //int first, last;
    unsigned long int  tape_len;

    user_mt_no = (ext_io_op & EXT_UNIT);

    if (sim_deb && mt_dev.dctrl) 
	fprintf (sim_deb, "mt: format_tape(..), user_mt_no=%d,user_first=%04o,user_last=%04o\n", user_mt_no,first,last);

    /* make a map of logical tape number to physical tape number */

    mt_no = log_tape_map_array[user_mt_no];
    if (sim_deb && mt_dev.dctrl)
	fprintf (sim_deb, "mt: format_tape(..), mt_no=%d user_mt_no=%d\n", mt_no, user_mt_no);

    /* check tape mapping: cannot be 2 or more logical tapes to be mapped to one physical tape */
    if (tape_map_check) {
      for( i=0; i<MAX_TAPES_COUNT; i++ ) {
        tape_chk=log_tape_map_array[i];
        for( j=i+1;j<MAX_TAPES_COUNT; j++ ) {
          if ( i!=j ) {
            if (tape_chk == log_tape_map_array[j]) {
             if (sim_deb && mt_dev.dctrl) 
                 fprintf (sim_deb, "mt: format_tape(..), switch error: i=%d, j=%d\n", i,j );
              return STOP_TAPE_MAP_ERROR;
            }
          }
        }/*for*/
      }/*for*/
    }

    /* check access mode for selected physical tape */

    if (sim_deb && mt_dev.dctrl) {
	for( i=0;i<=MAX_TAPES_COUNT;i++) fprintf (sim_deb, "mt: format_tape(..), tape_access_mode_array[%d]=%d\n", 
                                                  i, tape_access_mode_array[mt_no] );
    }

    if (!(tape_access_mode_array[mt_no] & TAPE_FORMAT_MODE)) {
      return STOP_TAPE_NOT_IN_FORMAT_MODE;
    }

    if (ext_io_op & EXT_DIS_RAM) no_mosu_access=1;
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: format_tape(): no_mosu_access=%d\n", no_mosu_access );

    //first = ext_io_ram_start;
    //last = ext_io_ram_end;
    if (tape_auto_skip_zero_address && (first==0) && (last>0)) first++;
    codes_group_size = last - first + 1;
    if (sim_deb && mt_dev.dctrl) 
        fprintf (sim_deb, "mt: format_tape(): codes_group_size=%04o\n", codes_group_size);

    if ((codes_group_size > MAX_TAPE_ZONE_SIZE) || (codes_group_size < MIN_TAPE_ZONE_SIZE)) {
       return STOP_TAPEFMTINVAL;
    }

    if (sim_deb && mt_dev.dctrl)
	fprintf (sim_deb, "mt: format_tape(): new codes_group_size=%04o\n", codes_group_size);

    zone_num = ext_io_dev_zone_addr;
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: format_tape(): zone_num=%04o\n", zone_num);

    if ((zone_num > MAX_TAPE_ZONE_NUM) || (zone_num < MIN_TAPE_ZONE_NUM)) return STOP_TAPEFMTINVAL;

    /* detect tape length */
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: format_tape(): get tape length\n");
    res = fseek (mt_unit[mt_no].fileref, 0, SEEK_END);
    if (res) return SCPE_IOERR;
    tape_len = ftell (mt_unit[mt_no].fileref);
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: format_tape(): tape_length=%lu\n", tape_len);

    last_fmt_pos = ftell(mt_unit[mt_no].fileref);
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: format_tape(): last_fmt_pos=%d\n", last_fmt_pos );

    if (sim_deb && mt_dev.dctrl) {
	fprintf (sim_deb, "mt: format_tape(): last_exp_pos=%d (max_mt_userdata_size=%d)\n", 
                 last_fmt_pos+(codes_group_size+2)*sizeof(t_value), MAX_TAPE_SIZE*sizeof(t_value) );
    }

    /* Wrong length for magnetic tape formatting */
    if ((last_fmt_pos+(codes_group_size+1+1)*sizeof(t_value)) > MAX_TAPE_SIZE*sizeof(t_value)) 
        return STOP_TAPEBADFLEN;

    /* 
       Write zone number and size.
       In real M-20 zone was written twice and no codes count was written.
    */
    temp_value = ((t_value)codes_group_size<<BITS_32) + zone_num;
    count = fxwrite (&temp_value, sizeof(t_value), 1, mt_unit[mt_no].fileref);
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: format_tape(): write_count=%04o\n", count);
    if (ferror (mt_unit[mt_no].fileref)) return SCPE_IOERR;
    if (count != 1) return SCPE_IOERR;
    codes_num = 1;
    if (ocodes) *ocodes = codes_num;

    /* Write zone (with zeros or with user data) */
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: format_tape(): write zone data or zeroes\n");
    chksum = 0;
    for( i=0; i<codes_group_size; i++ ) {
        temp_value = 0;
        if (!no_mosu_access) {
          if ((first+i) <= last) temp_value = mosu_load( (first+i) & MAX_ADDR_VALUE );
        }
        chksum = cyclic_checksum(chksum, temp_value);
        if (sim_deb && mt_dev.dctrl) {
          if (tape_format_data_dump) fprintf (sim_deb, "mt: format_value=%015llo\n",temp_value);
        }
        count = fxwrite (&temp_value, sizeof(t_value), 1, mt_unit[mt_no].fileref);
        //if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: write_count=%04o\n", count);
        if (ferror (mt_unit[mt_no].fileref)) return SCPE_IOERR;
        if (count != 1) return SCPE_IOERR;
        codes_num++;
        if (ocodes) *ocodes = codes_num;
    }

    /* Write last checksum (for whole zone) */
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: format_tape(): chksum=%015llo\n", chksum);
    temp_value = chksum;
    if (sim_deb && mt_dev.dctrl) {
      if (tape_format_data_dump) fprintf (sim_deb, "mt: format_value=%015llo\n", temp_value);
    }
    count = fxwrite (&temp_value, sizeof(t_value), 1, mt_unit[mt_no].fileref);
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: format_tape(): write_count=%04o\n", count);
    if (ferror (mt_unit[mt_no].fileref)) return SCPE_IOERR;
    if (count != 1) return SCPE_IOERR;
    codes_num++;
    if (ocodes) *ocodes = codes_num;
    if (sum) *sum = chksum;
	
    return SCPE_OK;
}




/*
 * Запись на МЛ.
 * Если параметр sum ненулевой, посчитываем и кладём туда контрольную
 * сумму массива. Также запмсываем сумму в слово last+1 на МЛ.
 */
t_stat mt_write (int mt_no, int user_zone_num, int first, int last, t_value *sum, int * ocodes, 
                 int no_mosu_access, int disable_control)
{
    int  nwords, count, userwords, i, codes_num, res;
    int  cur_zone_num, cur_zone_size;
    t_value  temp_value, chksum;
    unsigned long int  tape_len, cur_tape_pos;

    if (sim_deb && mt_dev.dctrl)
	fprintf (sim_deb, "mt: mt_write(%d,%05o,%04o,%04o,..)\n", mt_no, user_zone_num, first, last);

    if (sim_deb && mt_dev.dctrl) {
      fprintf (sim_deb, "mt: write: no_mosu_access=%d, disable_control=%d\n", no_mosu_access, disable_control );
    }

    if ((user_zone_num > MAX_TAPE_ZONE_NUM) || (user_zone_num < MIN_TAPE_ZONE_NUM)) {
       /* wrong zone address */
       return STOP_TAPEINVZONE;
    }

    if (tape_auto_skip_zero_address && (first==0) && (last>0)) first++;
    userwords = last - first + 1;
    /* Неверная длина записи на МЛ (д.б. не более макс.длины зоны)*/
    if ((userwords < MIN_TAPE_ZONE_SIZE) || (userwords > MAX_TAPE_ZONE_SIZE)) return STOP_TAPEBADWLEN;

    /* detect tape length */
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_write(): get tape length\n");
    res = fseek (mt_unit[mt_no].fileref, 0, SEEK_END);
    if (res) return SCPE_IOERR;
    tape_len = ftell (mt_unit[mt_no].fileref);

    /* Rewind tape into the beginning */
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_write(): rewind tape to beginning\n");
    res = fseek (mt_unit[mt_no].fileref, 0, SEEK_SET);
    if (res) return SCPE_IOERR;

    cur_tape_pos = 0;
    cur_zone_num = 0;

    codes_num = 0;
    if (ocodes) *ocodes = codes_num;

    //while( cur_zone_num <= MAX_TAPE_ZONE_NUM) {
    while( cur_tape_pos < tape_len) {

        /* read zone number and length */
        temp_value = 0;
        count = (int)fxread (&temp_value, sizeof(t_value), 1, mt_unit[mt_no].fileref);
        if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_write(): read_zone_num_count=%d\n", count);
        if (ferror (mt_unit[mt_no].fileref)) return SCPE_IOERR;

        /* Чтение неинициализированной ленты? */
        if (count != 1) return STOP_TAPEINVDATA;

        codes_num++;
        if (ocodes) *ocodes = codes_num;

        /* extract zone number and length */
        cur_zone_num = temp_value & 0xFFFFFFF;
        cur_zone_size = temp_value >> BITS_32;
        if (sim_deb && mt_dev.dctrl)
	    fprintf (sim_deb, "mt: mt_write(): cur_zone_num=%d, cur_zone_size=%d\n", cur_zone_num,cur_zone_size);

	/* bad zone size? */
	if (cur_zone_num,cur_zone_size > MAX_TAPE_ZONE_SIZE) return STOP_TAPEBADRLEN;


	/* matching zone found! */
	if (cur_zone_num == user_zone_num) {
            if (sim_deb && mt_dev.dctrl)
	        fprintf (sim_deb, "mt: mt_write(): matching_zone_found (%d==%d)\n", user_zone_num, cur_zone_num );
	    /* Check zone size to write */
            if (sim_deb && mt_dev.dctrl)
	        fprintf (sim_deb, "mt: mt_write(): userwords=%d, cur_zone_size=%d\n", userwords, cur_zone_size );
	    /* User data cannot written to tape zone */
	    if (userwords > cur_zone_size) return STOP_TAPELARGEDATA;
	    /* write data into tape */
            cur_tape_pos = ftell (mt_unit[mt_no].fileref);
            if (sim_deb && mt_dev.dctrl)
	        fprintf (sim_deb, "mt: mt_write(): cur_tape_pos=%d, tape_len=%d\n", cur_tape_pos, tape_len );
            res = fseek (mt_unit[mt_no].fileref, cur_tape_pos, SEEK_SET);
            if (res) return SCPE_IOERR;
            if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_write(): write zone data or zeroes\n");
            chksum = 0;
            for( i=0; i<userwords; i++ ) {
                if (no_mosu_access) temp_value = 0;
                else temp_value = mosu_load(first+i);
                if (sim_deb && mt_dev.dctrl) {
                  if (tape_write_data_dump) fprintf (sim_deb, "mt: write_value=%015llo\n",temp_value);
                }
                chksum = cyclic_checksum (chksum, temp_value);
                //if (sim_deb && mt_dev.dctrl)
	        //    fprintf (sim_deb, "mt: mt_write(): mosu[%04o]=%015llo\n", first+i,temp_value);
                count = (int)fxwrite (&temp_value, sizeof(t_value), 1, mt_unit[mt_no].fileref);
                if (ferror (mt_unit[mt_no].fileref)) return SCPE_IOERR;
                if (count != 1) return SCPE_IOERR;
                codes_num++;
                if (ocodes) *ocodes = codes_num;
            }
            /* Write last checksum (for all user data) */
            //if (sum) {
            if (1) {
              if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_write(): sum=%015llo\n", chksum);
              temp_value = chksum;
              if (!disable_control) {
                if (sim_deb && mt_dev.dctrl) {
                  if (tape_write_data_dump) fprintf (sim_deb, "mt: write_value=%015llo\n", temp_value);
                }
                count = (int)fxwrite (&temp_value, sizeof(t_value), 1, mt_unit[mt_no].fileref);
                if (sim_deb && mt_dev.dctrl) 
                  fprintf (sim_deb, "mt: mt_write(): write_data_chksum_count=%d\n", count);
                if (ferror (mt_unit[mt_no].fileref)) return SCPE_IOERR;
                if (count != 1) return SCPE_IOERR;
                codes_num++;
              }
              /* store results */
              if (sum) *sum = chksum;
              if (ocodes) *ocodes = codes_num;
            }
            if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: writing_done\n");
            return SCPE_OK;
	}


        /* extract data from zone  */
	memset( temp_zone_buf, 0, sizeof(temp_zone_buf) );
	nwords = cur_zone_size;

        count = (int)fxread (temp_zone_buf, sizeof(t_value), nwords, mt_unit[mt_no].fileref);
        if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_write(): read_data_zone_count=%d\n", count);
        if (ferror (mt_unit[mt_no].fileref)) return SCPE_IOERR;

        /* Чтение неинициализированной ленты? */
        if (count != nwords) return STOP_TAPEINVDATA;

        codes_num += nwords;
        if (ocodes) *ocodes += nwords;

        /* read checksum */
        temp_value = 0;
        count = (int)fxread (&temp_value, sizeof(t_value), 1, mt_unit[mt_no].fileref);
        if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_write(): read_chksum_count=%d\n", count);
        if (ferror (mt_unit[mt_no].fileref)) return SCPE_IOERR;

        /* Чтение неинициализированной ленты? */
        if (count != 1) return STOP_TAPEINVDATA;

        codes_num++;
        if (ocodes) *ocodes = codes_num;

        chksum = temp_value;
        if (sim_deb && mt_dev.dctrl) fprintf (sim_deb,"mt: mt_write(): read_chksum_value=%015llo\n",chksum);

        cur_tape_pos = ftell (mt_unit[mt_no].fileref);
        if (sim_deb && mt_dev.dctrl)
	    fprintf (sim_deb, "mt: mt_write(): cur_tape_pos=%d, tape_len=%d\n", cur_tape_pos, tape_len );

	/* compare our zone with current zone on tape */
	if (cur_zone_num > user_zone_num) {
	    /* We not found match zone */
	    return STOP_NOTAPEZONE;
	}

        /* Continue to search? */

    } /*while*/


    return STOP_NOTAPE;
}





/*
 * Чтение с МЛ
 */
t_stat mt_read (int mt_no, int user_zone_num, int first, int last, t_value *sum, int * ocodes, 
                int no_mosu_access, int disable_control)
{
    int  nwords, count, userwords, i, codes_num, res;
    int  cur_zone_num, cur_zone_size;
    t_value  temp_value, chksum, calc_sum, user_chksum;
    unsigned long tape_len, cur_tape_pos;

    if (sim_deb && mt_dev.dctrl)
	fprintf (sim_deb, "mt: mt_read(%d,%05o,%04o,%04o,..)\n", mt_no, user_zone_num, first, last);

    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_read(): no_mosu_access=%d\n", no_mosu_access );

    /* wrong zone address? */
    if ((user_zone_num > MAX_TAPE_ZONE_NUM) || (user_zone_num < MIN_TAPE_ZONE_NUM)) return STOP_TAPEINVZONE;

    if (tape_auto_skip_zero_address && (first==0) && (last>0)) first++;
    userwords = last - first + 1;
    /* Неверная длина записи на МЛ (не более макс.длины зоны)*/
    if ((userwords < MIN_TAPE_ZONE_SIZE) || (userwords > MAX_TAPE_ZONE_SIZE)) return STOP_TAPEBADWLEN;

    if (sim_deb && mt_dev.dctrl) {
      fprintf (sim_deb, "mt: read: no_mosu_access=%d, disable_control=%d\n", no_mosu_access, disable_control );
    }

    /* detect tape length */
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_read(): get tape length\n");
    res = fseek (mt_unit[mt_no].fileref, 0, SEEK_END);
    if (res) return SCPE_IOERR;
    tape_len = ftell (mt_unit[mt_no].fileref);

    /* Rewind tape into the beginning */
    if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_read(): rewind tape to beginning\n");
    res = fseek (mt_unit[mt_no].fileref, 0, SEEK_SET);
    if (res) return SCPE_IOERR;

    cur_tape_pos = 0;
    cur_zone_num = 0;

    codes_num = 0;
    if (ocodes) *ocodes = codes_num;

    while( cur_tape_pos < tape_len) {

        /* read zone number and length */
        temp_value = 0;
        count = (int)fxread (&temp_value, sizeof(t_value), 1, mt_unit[mt_no].fileref);
        if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_read(): read_zone_num_count=%d\n", count);
        if (ferror (mt_unit[mt_no].fileref)) return SCPE_IOERR;

	/* Чтение неинициализированной ленты */
        if (count != 1) return STOP_TAPEINVDATA;

        codes_num++;
        if (ocodes) *ocodes = codes_num;

        /* extract zone number and length */
        cur_zone_num = temp_value & 0xFFFFFFF;
        cur_zone_size = temp_value >> BITS_32;
        if (sim_deb && mt_dev.dctrl)
	    fprintf (sim_deb, "mt: mt_read(): cur_zone_num=%d, cur_zone_size=%d\n", cur_zone_num,cur_zone_size);

	/* bad zone size */
	if (cur_zone_num,cur_zone_size > MAX_TAPE_ZONE_SIZE) return STOP_TAPEBADRLEN;

        /* extract data from zone  */
	memset( temp_zone_buf, 0, sizeof(temp_zone_buf) );
	nwords = cur_zone_size;

        count = (int)fxread (temp_zone_buf, sizeof(t_value), nwords, mt_unit[mt_no].fileref);
        if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_read(): read_userdata_zone_count=%d\n", count);
        if (ferror (mt_unit[mt_no].fileref)) return SCPE_IOERR;

	/* Чтение неинициализированной ленты */
        if (count != nwords) return STOP_TAPEINVDATA;

        codes_num += nwords;
        if (ocodes) *ocodes += nwords;

        if (sim_deb && mt_dev.dctrl) {
          if (tape_read_data_dump && (count <= nwords)) {
            for( i=0; i<count; i++ ) fprintf (sim_deb, "mt: read_value=%015llo\n", temp_zone_buf[i]);
          }
        }

        /* read checksum */
        temp_value = 0;
        count = (int)fxread (&temp_value, sizeof(t_value), 1, mt_unit[mt_no].fileref);
        if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: mt_read(): read_chksum_count=%d\n", count);
        if (ferror (mt_unit[mt_no].fileref)) return SCPE_IOERR;

	/* Чтение неинициализированной ленты */
        if (count != 1) return STOP_TAPEINVDATA;

        codes_num++;
        if (ocodes) *ocodes = codes_num;

        if (sim_deb && mt_dev.dctrl) {
          if (tape_read_data_dump) fprintf (sim_deb, "mt: read_value=%015llo\n", temp_value);
        }

        chksum = temp_value;
        if (sim_deb && mt_dev.dctrl) fprintf (sim_deb,"mt: mt_read(): read_chksum_value=%015llo\n",chksum );

        cur_tape_pos = ftell (mt_unit[mt_no].fileref);
        if (sim_deb && mt_dev.dctrl)
	    fprintf (sim_deb, "mt: mt_read(): cur_tape_pos=%d, tape_len=%d\n", cur_tape_pos, tape_len );


	/* matching zone found! */
	if (cur_zone_num == user_zone_num) {
            if (sim_deb && mt_dev.dctrl)
	        fprintf (sim_deb, "mt: mt_read(): matching_zone_found (%d==%d)\n", user_zone_num, cur_zone_num );
	    /* Check zone size to write */
            if (sim_deb && mt_dev.dctrl)
	        fprintf (sim_deb, "mt: mt_read(): userwords=%d, cur_zone_size=%d\n", userwords, cur_zone_size );
	    if (userwords > cur_zone_size) userwords = cur_zone_size;
            if (sim_deb && mt_dev.dctrl)
	        fprintf (sim_deb, "mt: mt_read(): [new] userwords=%d, cur_zone_size=%d\n", userwords, cur_zone_size );
	    if (userwords < cur_zone_size) {
                user_chksum =  temp_zone_buf[userwords];
                if (sim_deb && mt_dev.dctrl)
	            fprintf (sim_deb, "mt: mt_read(): user_chksum=%015llo\n", user_chksum );
                chksum = user_chksum;
                if (sim_deb && mt_dev.dctrl)
	            fprintf (sim_deb, "mt: mt_read(): new_real_chksum=%015llo\n", user_chksum );
	    }
	    /* Copy tape zone data */
            calc_sum = 0;
	    for( i=0; i<userwords; i++ ) {
	        temp_value = temp_zone_buf[i];
	        if (!no_mosu_access) mosu_store(first+i,temp_value);
                calc_sum = cyclic_checksum (calc_sum, temp_value);
	    }
            if (sim_deb && mt_dev.dctrl)
	      fprintf (sim_deb, "mt: mt_read(): read_chksum=%015llo calc_chksum=%015llo\n", chksum, calc_sum );
	    if (sum) {
	      if (sum) *sum = calc_sum;
              if (!disable_control && (calc_sum != chksum)) return STOP_TAPEREADERR;
            }
            if (sim_deb && mt_dev.dctrl) fprintf (sim_deb, "mt: reading_done\n");
	    return SCPE_OK;
	}


	/* compare our zone with current zone on tape */
	if (cur_zone_num > user_zone_num) {
	    /* We not found match zone */
	    return STOP_NOTAPEZONE;
	}


    } /*while*/


    return STOP_NOTAPE;
}






/*
 * Выполнение обращения к МЛ.
 * Все параметры находятся в регистрах УЧ, А_МЗУ, Н_МОЗУ, К_МОЗУ.
 */
t_stat mt_tape_io(t_value *sum, int * ocodes)
{
    int user_mt_no, mt_no, tape_chk, i, j;
    t_stat err = SCPE_IOERR;

    user_mt_no = (ext_io_op & EXT_UNIT);

    if (sim_deb && mt_dev.dctrl)
	fprintf (sim_deb, "mt: mt_tape_io(..), user_mt_no=%d\n", user_mt_no);


    /* make a map of logical tape number to physical tape number */

    mt_no = log_tape_map_array[user_mt_no];
    if (sim_deb && mt_dev.dctrl)
	fprintf (sim_deb, "mt: mt_tape_io(..), mt_no=%d user_mt_no=%d\n", mt_no, user_mt_no);

    /* check tape mapping: cannot be 2 or more logical tapes to be mapped to one physical tape */
    if (tape_map_check) {
      for( i=0; i<MAX_TAPES_COUNT; i++ ) {
        tape_chk=log_tape_map_array[i];
        for( j=i+1;j<MAX_TAPES_COUNT; j++ ) {
          if ( i!=j ) {
            if (tape_chk == log_tape_map_array[j]) {
             if (sim_deb && mt_dev.dctrl) 
                 fprintf (sim_deb, "mt: mt_tape_io(..), switch error: i=%d, j=%d\n", i,j );
              return STOP_TAPE_MAP_ERROR;
            }
          }
        }/*for*/
      }/*for*/
    }

    /* check access mode for selected physical tape */

    if (sim_deb && mt_dev.dctrl) {
	for( i=0;i<=MAX_TAPES_COUNT;i++) fprintf (sim_deb, "mt: mt_tape_io(..), tape_access_mode_array[%d]=%d\n", 
                                                  i, tape_access_mode_array[mt_no] );
    }

    if ((ext_io_op & EXT_WRITE) && (!(tape_access_mode_array[mt_no] & TAPE_WRITE_MODE))) {
      return STOP_TAPE_NOT_IN_WRITE_MODE;
    }
    else {
      if (!(tape_access_mode_array[mt_no] & TAPE_READ_MODE)) {
        return STOP_TAPE_NOT_IN_READ_MODE;
      }
    }

    if ((mt_dev.flags & DEV_DIS) || ! mt_unit[mt_no].fileref) {
	/* Device not attached. */
	return SCPE_UNATT;
    }

    if (ext_io_op & EXT_WRITE) {
	err = mt_write (mt_no, ext_io_dev_zone_addr, ext_io_ram_start, ext_io_ram_end,
			 //(ext_io_op & EXT_DIS_CHECK) ? 0 : sum, ocodes, ext_io_op & EXT_DIS_RAM);
                         sum, ocodes, ext_io_op & EXT_DIS_RAM ? 1 : 0, (ext_io_op & EXT_DIS_CHECK) ? 1 : 0);
    } else {
	err = mt_read (mt_no, ext_io_dev_zone_addr, ext_io_ram_start, ext_io_ram_end,
		        //(ext_io_op & EXT_DIS_CHECK) ? 0 : sum, ocodes, ext_io_op & EXT_DIS_RAM);
                         sum, ocodes, ext_io_op & EXT_DIS_RAM ? 1 : 0, (ext_io_op & EXT_DIS_CHECK) ? 1 : 0);
    }

    if (sim_deb && mt_dev.dctrl)
	fprintf (sim_deb, "mt: mt_tape_io(..), err=%d\n", err);

    return err;
}
