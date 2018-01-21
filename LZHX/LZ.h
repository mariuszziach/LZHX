/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////


#ifndef LZHX_LZ_H
#define LZHX_LZ_H

// LZHX
#include "Types.h"
#include "BitStream.h"

// number of LZ streams
#define ILZSN  4

// IDs
#define ILZIS  0 // instruction stream
#define ILZMPS 1 // match pos stream
#define ILZMLS 2 // match len stream
#define ILZLS  3 // literal stream

// LZ instructions
#define ILZIM1 0 // match 1 
#define ILZIM2 1 // match 2
#define ILZIL  2 // literal

// others
#define ILZMINML 4 // minimum match len

namespace LZHX {

// dictionary buffer
class LZDictionaryBuffer {
public:
    Byte *arr;
    int pos, size, cap;
    LZDictionaryBuffer(int cap);
    ~LZDictionaryBuffer();
    Byte *putByte(Byte val);
    Byte  getByte(int p);
    int   getPos();
    int   convPos(bool absToRel, int pos);
};

// lz match 
class LZMatch {
public:
    int pos, len;
    LZMatch();
    void clear();
    void copy(LZMatch *lzm);
};

// lz lookup table node
class LZDictionaryNode {
public:
    int pos;
    bool valid;
    LZDictionaryNode *next, *prev;
    LZDictionaryNode();
    void clear();
};

// lz match finder
class LZMatchFinder {
private:
    int dict_i, buf_size;
    Byte               *buf;
    CodecSettings      *cdc_sttgs;
    LZDictionaryBuffer *lz_buf;
    LZDictionaryNode   *lkp_tab, *dict_tab;
    LZMatch            *best_match;
public:
    LZMatchFinder(CodecSettings *cdc_sttgs);
    ~LZMatchFinder();
    void assignBuffer(Byte *buf, int buf_size, LZDictionaryBuffer *lz_buf);
    int  hash(Byte *in);
    void insert(int pos);
    LZMatch *find(int pos);
};

// lz algorithm main class
class LZ : public CodecInterface {
private:
    int total_in, total_out, stream_size;
    CodecStream        *codec_stream;
    CodecSettings      *cdc_sttgs;
    BitStream          *bit_stream;
    LZMatchFinder      *lz_mf;
    LZMatch             match_empty;
    LZMatch            *lz_match;
    LZMatch            *rolz_match;
    LZDictionaryBuffer *lz_buf;
    Byte               *tmp_btebf;
public:
    LZ(CodecSettings *cdc_sttgs);
    ~LZ();
    CodecType getCodecType();
    int  getTotalIn();
    int  getTotalOut();
    void initStream(CodecStream *codec_stream);
    int  compressBlock();
    int  decompressBlock();
};

} // namespace

#endif // LZHX_LZ_H
