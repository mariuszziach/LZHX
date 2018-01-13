/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

// c
#include <cstdint>

#ifndef LZHX_TYPES_H
#define LZHX_TYPES_H

namespace LZHX {

typedef uint8_t  Byte;
typedef uint16_t Word;
typedef uint32_t DWord;
typedef uint64_t QWord;

enum CodecType       { CT_LZ, CT_HF };
enum CodecBufferType { CBT_LZ, CBT_HF, CBT_RAW, CBT_EMPTY };

struct CodecBuffer {
    Byte *mem;
    int   size, cap;
    CodecBufferType type;
};

class CodecCallbackInterface {
public:
    virtual bool   compressCallback(int in_size, int out_size, int stream_size) = 0;
    virtual bool decompressCallback(int in_size, int out_size, int stream_size) = 0;
};

class CodecStream {
public:
    int buf_count, stream_size;
	CodecBuffer *buf_stack;
    CodecBuffer *find(CodecBufferType type);
};

class CodecInterface {
public:
    virtual CodecType getCodecType()         = 0;
	virtual void initStream(CodecStream *cs) = 0;
	virtual int compressBlock  ()            = 0;
    virtual int decompressBlock()            = 0;
    virtual int getTotalIn()                 = 0;
    virtual int getTotalOut()                = 0;
};

class CodecSettings {
public:
    void Set(DWord bit_blk_cap, DWord bit_lkp_cap,
        DWord bit_lkp_hsh, DWord bit_mtch_len,
        DWord bit_mtch_pos, DWord bit_bffr_cnt);
    DWord bit_blk_cap;  // file chunk size in BITS
    DWord bit_lkp_cap;  // lookup table size in BITS
    DWord bit_lkp_hsh;  // number of bytes to count hash for lookup table in BITS
    DWord bit_mtch_len; // max match lenght size in BITS
    DWord bit_mtch_pos; // max match position size in BITS
    DWord bit_bffr_cnt; // working buffer count
    DWord byte_blk_cap, byte_lkp_cap, byte_lkp_hsh,
        byte_mtch_len, byte_mtch_pos, byte_bffr_cnt;
    DWord mask_blk_cap, mask_lkp_cap, mask_lkp_hsh,
        mask_mtch_len, mask_mtch_pos, mask_bffr_cnt;
};

} // namespace

#endif // LZHX_TYPES_H
