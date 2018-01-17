/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

#ifndef LZHX_GLOBALS_H
#define LZHX_GLOBALS_H

// LZHX
#include "Types.h"

namespace LZHX {

// console strings
char const S_TITLE[] =    "LZHX File Archiver";
char const S_INF1 [] =    "\n LZHX\n";
char const S_INF2 [] =    " Info       : File archiver based on Lempel-Ziv and Huffman algorithms.\n"
                          "              To check the integrity  of  files, the program uses a FNV\n"
                          "              hashing algorithm and for encryption modified XOR cipher.\n\n"
                          " Author     : mariusz.ziach@gmail.com\n"
                          " Date       : 2018\n"
                          " Version    : 1.0\n";
char const S_USAGE1[] =   " Usage: LZHX.exe <file/folder/archive to compress/decompress>\n";
char const S_USAGE2[] =   "  The program will automatically recognize whether the given parameter\n"
                          "  is an archive  for  decompression or a file/folder  for  compression.\n"
                          "  It  will also prevent overwriting files by creating unique names for\n"
                          "  outputed files and folders if needed.\n ";
char const S_ERR_FOPN[] = " File error.\n";
char const S_ERR_EX  [] = " Exception: ";
char const S_ERR_UNEX[] = " Unknown exception.\n";
char const S_ERR_HASH[] = " Error - different file hashes.\n";
char const S_ERR_WPAS[] = " Wrong password.\n";
char const S_PASS1 []   = " Type password if you want to encrypt this archive or just press enter:";
char const S_PASS2 []   = " Archive is encrypted. Type password:";
char const S_COMP  []   = " Compress   : ";
char const S_DECOMP[]   = " Decompress : ";
char const S_LIST[]     = " Listing    : ";
char const S_LST_AR[]   = "Archive                   : ";
char const S_LST_FC[]   = "Files/folders in archive  : ";
char const S_LST_US[]   = "Uncompressed archive size : ";
char const S_LST_HD[]   = "  FNV Hash        Compressed       Uncompressed  Name";
char const S_CLOSE []   = " Press anything to close program...";
char const S_LSTEXT[]   = "txt";
char const S_LZEXT[]    = "lzhx";
char const S_ZERO []    = "0";
char const S_EMPTY[]    = "";
char const S_LISTC1     = 'l';
char const S_LISTC2     = 'L';

// archive signature
Byte  const sig[4] = { 'L','Z','H','X' };
DWord const sig2   = 0xFFFFFFFB;

// archive extension


};

#endif // LZHX_GLOBALS_H
