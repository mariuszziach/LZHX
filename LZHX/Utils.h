/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

#ifndef LZHX_UTILS_H
#define LZHX_UTILS_H

// stl
#include <fstream>
#include "Types.h"
#include "Globals.h"

#define DUMP(a) std::cout << #a " = " << (a) << std::endl; 

namespace LZHX {

// extern char const ext[5];

int   getFSize (std::ifstream &ifs);

// reading/writing integers from/to byte stream
int   write32To8Buf (Byte *buf, DWord i);
int   write16To8Buf (Byte *buf, Word  i);
DWord read32From8Buf(Byte *buf);
Word  read16From8Buf(Byte *buf);

// console
void setConsoleTextRed();
void setConsoleTextNormal();
void setConsoleTitle(char const *tit);
void consoleWait();

// file attributes
DWord getFileAttributes(char const *f_name);
bool setFileAttributes (char const *f_name, DWord attr);

// file time
void getFileTime(char const *f_name, QWord *fcr, QWord *fla, QWord *lwr, bool dir = false);
void setFileTime(char const *f_name, QWord  fcr, QWord  fla, QWord  lwr, bool dir = false);


} // namespace

#endif // LZHX_UTILS_H
