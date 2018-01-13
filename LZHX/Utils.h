/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

#ifndef LZHX_UTILS_H
#define LZHX_UTILS_H

#include <fstream>
#include "Types.h"

#define DUMP(varname) fprintf(stderr,  "%s\t= %i\n", #varname, varname);
#define DUMPS(varname) fprintf(stderr, "%s\t= %s\n", #varname, varname);

namespace LZHX {

int   getFSize (std::ifstream &ifs);
int   write32To8Buf (Byte *buf, DWord i);
int   write16To8Buf (Byte *buf, Word  i);
DWord read32From8Buf(Byte *buf);
Word  read16From8Buf(Byte *buf);

} // namespace

#endif // LZHX_UTILS_H
