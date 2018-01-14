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

#define DUMP(a) std::cout << #a " = " << (a) << std::endl; 

namespace LZHX {

extern char const ext[5];

int   getFSize (std::ifstream &ifs);
int   write32To8Buf (Byte *buf, DWord i);
int   write16To8Buf (Byte *buf, Word  i);
DWord read32From8Buf(Byte *buf);
Word  read16From8Buf(Byte *buf);

// windows console
void setConsoleTextRed();
void setConsoleTextNormal();
void setConsoleTitle(char const *tit);
void consoleWait();
DWord getFileAttributes(char const *f_name);
bool setFileAttributes (char const *f_name, DWord attr);
void getFileTime(char const *f_name, QWord *fcr, QWord *fla, QWord *lwr, bool dir = false);
void setFileTime(char const *f_name, QWord  fcr, QWord  fla, QWord  lwr, bool dir = false);
bool isFileExtAssoc();


} // namespace

#endif // LZHX_UTILS_H
