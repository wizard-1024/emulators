/*
 * File:     m20_rus_win_cp1251.h
 * Purpose:  M-20 simulator text for Windows CP-1251 (russian encoding)
 *
 * Copyright (c) 2014, Dmitry Stefankov
 *
 * $Id$
 *
 * Revision History.
 *
 *  01-Nov-2014  DVS  Initial Implemementation
 *  12-Nov-2014  DVS  Updated
 *  14-Nov-2014  DVS  Updated
 *  17-Nov-2014  DVS  Updated
 *  21-Nov-2014  DVS  Added short form of symbolic instructions
 *                    Updated long form of symbolic instructions
 *  03-Dec-2014  DVS  Updated long form of symbolic instructions
 *  05-Dec-2014  DVS  Updated
 *  29-Jun-2021  DVS  Changed sim_stop_messages definitions
 *
 */


#include "m20_defs.h"



//const char *sim_stop_messages[] = {
const char *sim_stop_messages[SCPE_BASE] = {
	"����������� ������",				/* Unknown error */
	"�������",					/* STOP */
	"����� ��������",				/* Breakpoint */
	"����� �������� (��������� � ������)",		/* Breakpoint (memory access) */
	"����� �������� (��������� � ����������)",	/* Breakpoint (instruction access) */
	"�������� ��� �������",				/* Invalid instruction */
	"����� �� ������� ������",			/* Run out end of memory */
	"������������ ��� ��������",			/* Addition overflow */
	"������������ ��� �������� ��������",		/* Exponent overflow */
	"������������ ��� ���������",			/* Multiplication overflow */
	"������������ ��� �������",			/* Division overflow */
	"������������ �������� ��� �������",		/* Division mantissa overflow */
	"������� �� ����",                              /* Division by zero */
	"������ �� �������������� �����",		/* SQRT from negative number */
	"������ ���������� �����",			/* SQRT error */
	"������ ������ ��������",			/* Drum read error */
	"�������� ����� ������ ��������",		/* Invalid drum read length */
	"�������� ����� ������ ��������",		/* Invalid drum write length */
	"������ ������ ��������",			/* Drum write error */
	"�������� �� ��� ��������� � ��������", 	/* Invalid drum control word */
	"������ ��������������������� ��������", 	/* Reading uninialized drum data */
	"�������� �� ��� ��������� � �����",		/* Invalid tape control word */
	"�������� �� ��� �������� �����",		/* Invalid tape format word */
	"����� � ��������� ������ �� ����������",	/* Tape not implemented */
	"�������� ��������� ����� �� �����������",	/* Tape formatting not implemented */
	"����� �� ���������� �� ����������",		/* Punch not implemented */
	"���� � ��������� �� ����������",		/* Punch reader not implemented */
	"�������� ��",					/* Invalid control word */
	"�������� �������� �������",			/* Invalid argument of instruction */
	"������� �� ������������",			/* Assertion failed */
	"������� �� �� �������� ��� ��",		/* MB instruction without MA */
	"������� ���������� �� ��������������",         /* External devices i/o not implemented */
        "����������� ���������� �����",                 /* Card reader empty */
	"������� �� �� �������� ��� �������",           /* CR instruction without CA */
        "�������� ������ ����� ��� ��",	                /* invalid card reader format word */
        "������������ ����������� ���� ��� ����� � ��", /* card reader mismatch cyclic sum */
        "��������� �����/������ ���������",             /* i/o setup was missing */
        "����� � ��������� ��������� �� ����������",    /* drum not implemented */
        "����� �� ������ �� ����������",                /* print not implemented */
        "����� �� ���������� � ��������� ���.���������",/* output to punch has wrong control bit */
        "����� �� ������ � ��������� ���.���������",    /* output to printer has wrong control bits */
        "�������� ���.������� ��� �������� � ��",       /* tape i/o has wrong control word */
        "�������� ���.������� ��� �������� � ��",       /* drum i/o has wrong control word */
        "�������� ���.������� ��� �������� ��",         /* tape format has wrong control word */
	"�������� ����� ������ ��������� �����",	/* Invalid tape read length */
	"�������� ����� ������ ��������� �����",	/* Invalid tape write length */
	"�������� ����� �������� ��������� �����",	/* Invalid tape format length */
        "�������� ���� ��� ��������� �����",            /* invalid tape zone number */
        "��� ������ ��� �� ��������� �����",            /* end of tape detected */
        "������ ������������������� ��",                /* reading uninialized tape data */
        "��������� ���� �� ��������� ����� �� �������", /* matching tape zone not found */
        "������ ���� �� �� ������������ ��� ������",    /* too large data for tape zone */
        "������ ���� �� �� ������ ����� ��� ������",    /* too small user buffer for data from tape zone */
        "������ ������ ��������� �����",                /* tape read error */
        "������� ������ � ������ (������-���-������)",  /* writing attempt to read-only memory location */
        "�� ���������� �����������",                    /* not ready punch */
        "�� ���������� ����������� ����������",         /* not ready print */
        "��� ������ ��� ������ ��������� ��",           /* card read out of memory */
        "���������� �������� ������� � ������ ������",  /* detected memory garbase per memory location */
        "�� ��������� ����� ������ ��� ��������",       /* drum not in write mode */
        "�� ��������� ����� ������ ��� ��������",       /* drum not in read mode */
        "������ ���������� ���������",                  /* one more logical drums mapped to one physical drum */
	"�� ��������� ����� �������� ��� ��������� �����",/* tape not in format mode */
	"�� ��������� ����� ������ ��� ��������� �����",/* tape not in write mode */
	"�� ��������� ����� ������ ��� ��������� �����",/* tape not in read mode */
	"������ ���������� ����",                       /* one more logical tapes mapped to one physical tape */
    };


char  reg_rk_name[]    = "RK";   //"��";    
char  reg_rop_name[]   = "ROP";  //"���";   
char  reg_ra_name[]    = "RA";   //"��";    
char  reg_kra_name[]   = "KRA";  //"���";   
char  reg_sma_name[]   = "SMA";  //"���";   
char  trg_sw_name[]    = "TSW";  //"��W";   
char  reg_rr_name[]    = "RR";   //"��"; 
char  reg_rpu1_name[]  = "RPU1";  //"���-1";
char  reg_rpu2_name[]  = "RPU2";  //"���-2";
char  reg_rpu3_name[]  = "RPU3";  //"���-3";
char  reg_rpu4_name[]  = "RPU4";  //"���-4";

char  reg_rk_desc[]    = "������� ������";
char  reg_rop_desc[]   = "������� ��������";
char  reg_ra_desc[]    = "������� ������";
char  reg_kra_desc[]   = "��������� ������� ������";
char  reg_sma_desc[]   = "�������� ������";
char  trg_sw_desc[]    = "������� ������� w";
char  reg_rr_desc[]    = "������� ����������";
char  reg_rpu1_desc[]  = "�������-1 ������ ����������";
char  reg_rpu2_desc[]  = "�������-2 ������ ����������";
char  reg_rpu3_desc[]  = "�������-3 ������ ����������";
char  reg_rpu4_desc[]  = "�������-4 ������ ����������";



const char *m20_opname [M20_SYM_OPCODE_TABLE_SIZE] = {
     /* 00-07 */
     "���������",          "����_��",                      "�����_��",                "�����_���_��", 
     "�����_�",            "�����_��",                     "����_����_�_���",         "����_����",
     /* 10-17 */
     "����_���_�������",   "���_����_��_����_a1_�_w1",     "���_����_��_���_a1",      "����_������",  
     "�����_����_��_���",  "�����",                        "������_���_���_�_����",   "�������_017",
     /* 20-27 */
     "�������_�_��",	   "����_�",                       "�����_�",                 "�����_���_�",  
     "�����",              "�����_�",                      "����_����_�_����",        "����_�����",           
     /* 30-37 */
     "����_���",	   "���_����_��_��_����_a1_�_w1",  "���_����_��_��_����_a1",  "�����_������",   
     "�����_����_��_����", "�����_�������",                "���_���_���_��_w1",       "�������_037",
     /* 40-47 */
     "���_040",	           "����_�",                       "�����_�",                 "�����_���_�",  
     "����_��_�����_�",   "�����_�",                       "���_���_��_����",         "�����_��_��������",
     /* 50-57 */
     "����_����_050",	   "���_����_��_���_a1_�_w0",      "���_��_��_���",           "����_���",  
     "�����_����_��_���",  "���_�����",                    "������_���_���",          "�������_057",
     /* 60-67 */
     "���_060",	           "����",	                   "�����",	              "�����_���",    
     "����_��_�����",      "�����",                        "�����_����_��_����",      "����_�����",
     /* 70-77 */
     "����_���_070",	   "���_��_��_���_a1_�_w1",        "���_��_��_����",          "���_���",   
     "�����_����_��_����", "���_����",                     "���_���_���_��_w0",       "�������_077"
};


const char *m20_short_opname [M20_SYM_OPCODE_TABLE_SIZE] = {
	"�",   "�",    "�",   "��",   "�",    "�",   "���",   "��",
	"��",  "���",  "��",  "��",   "����", "�",   "��",    "����",
	"���", "��",   "��",  "���",  "��",   "��",  "���",   "��",           
	"���", "���",  "��",  "��",   "����", "��",  "��",    "����",
	"��",  "��",   "��",  "���",  "�",    "��",  "���",   "���",
	"��",  "���",  "��",  "��",   "����", "�",   "��",    "����",
	"��",  "���",  "���", "����", "��",   "���", "���",   "����_�����",
	"��",  "���",  "��",  "��",   "����", "���", "��",    "����"
};



const unsigned char m20_to_cyr_win1251[MAX_PRINTER_SYMBOLS_NUM] = {
/* 000-007 */	'0',   '1',   '2',   '3',   '4',   '5',   '6',   '7',
/* 010-017 */	'8',   '9',   '+',   '-',   '/',   ',',   '.',   ' ',
/* 020-027 */	't',   '^',   '!',   '(',   ')',   'x',   '=',   ';',
/* 030-037 */	'[',   ']',   '*',   '\'', '\'',   'n',   '<',   '>', 
/* 040-047 */	':',   '�',   '�',   '�',   '�',   '�',   '�',   '�',
/* 050-057 */   '�',   '�',   '�',   '�',   '�',   '�',   '�',   '�',
/* 060-067 */   '�',   '�',   '�',   '�',   '�',   '�',   '�',   '�',
/* 070-077 */   '�',   '�',   '�',   '�',   '�',   '�',   '�',   '�',
/* 100-107 */   'D',   'F',   'G',   'I',   'J',   'L',   'N',   'Q',
/* 110-117 */   'R',   'S',   'U',   'V',   'W',   'Z',   '_',   ' ',
/* 120-127 */	  0,     0,     0,     0,     0,     0,     0,     0,
/* 140-147 */	  0,     0,     0,     0,     0,     0,     0,     0,
/* 150-157 */	  0,     0,     0,     0,     0,     0,     0,     0,
/* 160-167 */	  0,     0,     0,     0,     0,     0,     0,     0,
/* 170-177 */	  0,     0,     0,     0,     0,     0,     0,     0,
    };

