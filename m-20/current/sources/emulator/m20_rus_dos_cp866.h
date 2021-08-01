/*
 * File:     m20_rus_dos_cp866.h
 * Purpose:  M-20 simulator text for DOS CP-866 (russian encoding)
 *
 * Copyright (c) 2014, Dmitry Stefankov
 *
 * $Id$
 *
 * Revision History.
 *
 *  15-Dec-2014  DVS  Initial Implemementation
 *  29-Jun-2021  DVS  Changed sim_stop_messages definitions
 *
 */


#include "m20_defs.h"


//const char *sim_stop_messages[] = {
const char *sim_stop_messages[SCPE_BASE] = {
	"�������⭠� �訡��",				/* Unknown error */
	"��⠭��",					/* STOP */
	"��窠 ��⠭���",				/* Breakpoint */
	"��窠 ��⠭��� (���饭�� � �����)",		/* Breakpoint (memory access) */
	"��窠 ��⠭��� (���饭�� � ������樨)",	/* Breakpoint (instruction access) */
	"������ ��� �������",				/* Invalid instruction */
	"��室 �� �।��� �����",			/* Run out end of memory */
	"��९������� �� ᫮�����",			/* Addition overflow */
	"��९������� �� ᫮����� ���浪��",		/* Exponent overflow */
	"��९������� �� 㬭������",			/* Multiplication overflow */
	"��९������� �� �������",			/* Division overflow */
	"��९������� ������� �� �������",		/* Division mantissa overflow */
	"��७� �� ����⥫쭮�� �᫠",		/* SQRT from negative number */
	"�訡�� ���᫥��� ����",			/* SQRT error */
	"�訡�� �⥭�� ��ࠡ���",			/* Drum read error */
	"����ୠ� ����� �⥭�� ��ࠡ���",		/* Invalid drum read length */
	"����ୠ� ����� ����� ��ࠡ���",		/* Invalid drum write length */
	"�訡�� ����� ��ࠡ���",			/* Drum write error */
	"����୮� �� ��� ���饭�� � ��ࠡ���", 	/* Invalid drum control word */
	"�⥭�� �����樠����஢������ ��ࠡ���", 	/* Reading uninialized drum data */
	"����୮� �� ��� ���饭�� � ����",		/* Invalid tape control word */
	"����୮� �� ��� ࠧ��⪨ �����",		/* Invalid tape format word */
	"����� � �����⭮� ���⮩ �� ॠ�������",	/* Tape not implemented */
	"�����⪠ �����⭮� ����� �� ॠ��������",	/* Tape formatting not implemented */
	"�뢮� �� ���䮪���� �� ॠ�������",		/* Punch not implemented */
	"���� � ���䮪��� �� ॠ�������",		/* Punch reader not implemented */
	"����୮� ��",					/* Invalid control word */
	"������ ��㬥�� �������",			/* Invalid argument of instruction */
	"��⠭�� �� ��ᮢ�������",			/* Assertion failed */
	"������� �� �� ࠡ�⠥� ��� ��",		/* MB instruction without MA */
	"���譨� ���ன�⢠ �� �����ন������",         /* External devices i/o not implemented */
        "���뢠�饥 ���ன�⢮ ����",                 /* Card reader empty */
	"������� �� �� ࠡ�⠥� ��� ���ᮢ",           /* CR instruction without CA */
        "������ �ଠ� ����� ��� ��",	                /* invalid card reader format word */
        "��ᮢ������� ����஫��� �㬬 �� ����� � ��", /* card reader mismatch cyclic sum */
        "��⠭���� �����/�뢮�� �ய�饭�",             /* i/o setup was missing */
        "����� � ������� ��ࠡ���� �� ॠ�������",    /* drum not implemented */
        "�뢮� �� ����� �� ॠ�������",                /* print not implemented */
        "�뢮� �� ���䮪���� � �����묨 ��.ࠧ�鸞��",/* output to punch has wrong control bit */
        "�뢮� �� ����� � �����묨 ��.ࠧ�鸞��",    /* output to printer has wrong control bits */
        "������ ��.ࠧ��� ��� ����樨 � ��",       /* tape i/o has wrong control word */
        "������ ��.ࠧ��� ��� ����樨 � ��",       /* drum i/o has wrong control word */
        "������ ��.ࠧ��� ��� ࠧ��⪨ ��",         /* tape format has wrong control word */
	"����ୠ� ����� �⥭�� �����⭮� �����",	/* Invalid tape read length */
	"����ୠ� ����� ����� �����⭮� �����",	/* Invalid tape write length */
	"����ୠ� ����� ࠧ��⪨ �����⭮� �����",	/* Invalid tape format length */
        "����ୠ� ���� ��� �����⭮� �����",            /* invalid tape zone number */
        "��� ����� ��� �� �����⭮� ����",            /* end of tape detected */
        "�⥭�� �����樠����஢���� ��",                /* reading uninialized tape data */
        "��������� ���� �� �����⭮� ���� �� �������", /* matching tape zone not found */
        "������ ���� �� �� �������筮 ��� �����",    /* too large data for tape zone */
        "������ ���� �� �� �ࠩ�� ����� ��� �⥭��",    /* too small user buffer for data from tape zone */
        "�訡�� �⥭�� �����⭮� �����",                /* tape read error */
        "����⪠ ����� � �祩�� (⮫쪮-���-�⥭��)",  /* writing attempt to read-only memory location */
        "�� ��⮢���� �������",                    /* not ready punch */
        "�� ��⮢���� �����饣� ���ன�⢠",         /* not ready print */
        "��� ����� ��� �⥭�� ᫥���饩 ��",           /* card read out of memory */
        "�����㦥�� ������ ࠧ��� � �祩�� �����",  /* detected memory garbase per memory location */
        "�� ���⠢��� ०�� ����� ��� ��ࠡ���",       /* drum not in write mode */
        "�� ���⠢��� ०�� �⥭�� ��� ��ࠡ���",       /* drum not in read mode */
        "�訡�� �������� ��ࠡ����",                  /* one more logical drums mapped to one physical drum */
	"�� ���⠢��� ०�� ࠧ��⪨ ��� �����⭮� �����",/* tape not in format mode */
	"�� ���⠢��� ०�� ����� ��� �����⭮� �����",/* tape not in write mode */
	"�� ���⠢��� ०�� �⥭�� ��� �����⭮� �����",/* tape not in read mode */
	"�訡�� �������� ����",                       /* one more logical tapes mapped to one physical tape */
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

char  reg_rk_desc[]    = "ॣ���� ������";
char  reg_rop_desc[]   = "ॣ���� ����権";
char  reg_ra_desc[]    = "ॣ���� ����";
char  reg_kra_desc[]   = "�������� ॣ���� ����";
char  reg_sma_desc[]   = "�㬬��� ����";
char  trg_sw_desc[]    = "�ਣ��� ᨣ���� w";
char  reg_rr_desc[]    = "ॣ���� १����";
char  reg_rpu1_desc[]  = "ॣ����-1 ���� �ࠢ�����";
char  reg_rpu2_desc[]  = "ॣ����-2 ���� �ࠢ�����";
char  reg_rpu3_desc[]  = "ॣ����-3 ���� �ࠢ�����";
char  reg_rpu4_desc[]  = "ॣ����-4 ���� �ࠢ�����";



const char *m20_opname [M20_SYM_OPCODE_TABLE_SIZE] = {
     /* 00-07 */
     "����뫪�",          "᫮�_��",                      "����_��",                "����_���_��", 
     "�����_�",            "㬭��_��",                     "᫮�_�प_�_���",         "横�_᫮�",
     /* 10-17 */
     "����_��_��⠭��",   "���_�᫨_�_����_a1_�_w1",     "���_�᫨_�_���_a1",      "᫮�_������",  
     "ᤢ��_����_��_���",  "�ࠢ�",                        "�����_���_��_�_����",   "��⠭��_017",
     /* 20-27 */
     "�롮ઠ_�_��",	   "᫮�_�",                       "����_�",                 "����_���_�",  
     "�����",              "㬭��_�",                      "᫮�_�प_�_�प",        "横�_����",           
     /* 30-37 */
     "����_��",	   "���_�᫨_�_��_����_a1_�_w1",  "���_�᫨_�_��_����_a1",  "����_������",   
     "ᤢ��_����_��_�प", "�ࠢ�_��⠭��",                "��_���_��_��_w1",       "��⠭��_037",
     /* 40-47 */
     "���_040",	           "᫮�_�",                       "����_�",                 "����_���_�",  
     "����_��_����_�",    "㬭��_�",                      "���_���_��_�प",         "�뢮�_��_ࠧ�冷�",
     /* 50-57 */
     "����_�।_050",	   "���_�᫨_�_���_a1_�_w0",      "���_�_��_���",           "᫮�_���",  
     "ᤢ��_����_��_���",  "���_㬭��",                    "�����_���_��",          "��⠭��_057",
     /* 60-67 */
     "���_060",	           "᫮�",	                   "����",	              "����_���",    
     "����_��_����",      "㬭��",                        "����_�प_��_�प",      "横�_ᤢ��",
     /* 70-77 */
     "����_��_070",	   "���_�_��_���_a1_�_w1",        "���_�_��_����",          "���_���",   
     "ᤢ��_����_��_�प", "���_᫮�",                     "��_���_��_��_w0",       "��⠭��_077"
};


const char *m20_short_opname [M20_SYM_OPCODE_TABLE_SIZE] = {
	"�",   "�",    "�",   "��",   "�",    "�",   "ᯠ",   "��",
	"��",  "���",  "��",  "�",   "᤬�", "�",   "��",    "�⮯",
	"���", "�",   "��",  "���",  "��",   "�",  "ᯯ",   "�",           
	"���", "���",  "��",  "��",   "᤬�", "��",  "��",    "�⮯",
	"��",  "�",   "��",  "���",  "�",    "�",  "���",   "��",
	"��",  "��",  "�",  "�",   "��", "�",   "��",    "�⮯",
	"��",  "ᮭ",  "���", "����", "��",   "㮭", "���",   "横�_ᤢ��",
	"��",  "��",  "��",  "��",   "��", "���", "��",    "�⮯"
};



const unsigned char m20_to_cyr_win1251[MAX_PRINTER_SYMBOLS_NUM] = {
/* 000-007 */	'0',   '1',   '2',   '3',   '4',   '5',   '6',   '7',
/* 010-017 */	'8',   '9',   '+',   '-',   '/',   ',',   '.',   ' ',
/* 020-027 */	't',   '^',   '!',   '(',   ')',   'x',   '=',   ';',
/* 030-037 */	'[',   ']',   '*',   '\'',  '\'',  'n',   '<',   '>', 
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

