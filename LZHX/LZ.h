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

namespace LZHX {

class LZDictionaryBuffer {
public:
    Byte *arr;
    int pos, size, cap;
    LZDictionaryBuffer(int cap = 0xFFFF);
    ~LZDictionaryBuffer();
    Byte *putByte(Byte val);
    Byte getByte(int p);
    int getPos();
    int convPos(bool absToRel, int pos);
};

class LZMatch {
public:
    int pos;
    int len;

    LZMatch();
    void clear();
    void copy(LZMatch *lzm);
};

class LZDictionaryNode {
public:
    int pos;
    LZDictionaryNode *next;
    LZDictionaryNode *prev;
    bool valid;

    LZDictionaryNode();
    void clear();
};

class LZMatchFinder {
private:
    CodecSettings *cdc_sttgs;
    LZDictionaryBuffer *lz_buf;
    Byte *buf;
    LZDictionaryNode *hash_map;
    LZDictionaryNode *cyclic_buf;
    int cyclic_iterator, buf_size;
    LZMatch *best_match;

public:
    LZMatchFinder(CodecSettings *cdc_sttgs);
    ~LZMatchFinder();
    void assignBuffer(Byte *buf, int buf_size, LZDictionaryBuffer *lz_buf);
    int hash(Byte *in);
    void insert(int pos);
    LZMatch *find(int pos);
};

class LZ : public CodecInterface {
private:
    int total_in;
    int total_out;
    int stream_size;
    CodecStream *codec_stream;
    CodecSettings *cdc_sttgs;
    BitStream    *bit_stream;
    LZMatchFinder      *lz_mf;
    LZMatch            *lz_match;
    LZDictionaryBuffer *lz_buf;

public:

    LZ(CodecSettings *cdc_sttgs);
    ~LZ();
    CodecType getCodecType();
    int getTotalIn();
    int getTotalOut();
    void initStream(CodecStream *codec_stream);
    int compressBlock();
    int decompressBlock();
};

} // namespace

#endif // LZHX_LZ_H
