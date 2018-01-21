/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

#ifndef LZHX_UTILS_H
#define LZHX_UTILS_H

// stl
#include <fstream>
#include <string>

// LZHX
#include "Types.h"
#include "Globals.h"

#define DUMP(a) std::cout << #a " = " << (a) << std::endl; 

namespace LZHX {

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
void consoleEndLine();
void consoleWriteEndLine(char const *msg);
void consoleAskPassword1(std::string &pass);
void consoleAskPassword2(std::string &pass);
void consoleCompWrite  (const char *f_name1, const char *f_name2);
void consoleDecompWrite(const char *f_name1, const char *f_name2);
void consoleListWrite  (const char *f_name1, const char *f_name2);
void consolePrintProgress(const char *f_name, int pr,
    float sec, int in_s, int out_s);
void consoleSummaryWrite(QWord tot_in, QWord tot_out,
    float sec, bool cmp);

// file list output
void fileListWriteHeader(std::ofstream &ofs, DWord a_cnt,
    QWord a_unc, const char *a_name);
void fileListWriteFile(std::ofstream &ofs, const char *f_name,
    FileHeader *fh);

// file attributes
DWord getFileAttributes(char const *f_name);
bool setFileAttributes (char const *f_name, DWord attr);

// file time
void getFileTime(char const *f_name, QWord *fcr, QWord *fla, QWord *lwr, bool dir = false);
void setFileTime(char const *f_name, QWord  fcr, QWord  fla, QWord  lwr, bool dir = false);

// suffix gen
std::string suffixGen(int &i);

} // namespace

#endif // LZHX_UTILS_H
