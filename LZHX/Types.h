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

// basic unsigned var types
typedef uint8_t  Byte;
typedef uint16_t Word;
typedef uint32_t DWord;
typedef uint64_t QWord;

// enums
enum CodecType       { CT_LZ  = 0x1, CT_HF  = 0x2 };
enum CodecBufferType { CBT_LZ = 0x1, CBT_HF = 0x2, CBT_RAW = 0x4, CBT_EMPTY = 0x8 };
enum ArchiveFlags    { AF_ENCRYPT = 0x1 };
enum FileFlags       { FF_DIR     = 0x1 };

// byte buffer with size, cap and type
struct CodecBuffer {
    Byte *mem;
    int   size, cap;
    CodecBufferType type;
};

// codec callback interface for monitoring compression progress
class CodecCallbackInterface {
public:
    virtual void init() = 0;
    virtual bool   compressCallback(int in_size, int out_size, int stream_size, const char *f_name) = 0;
    virtual bool decompressCallback(int in_size, int out_size, int stream_size, const char *f_name) = 0;
};

// codec stream is ussualy file stream where stream_size is file size
// buf_cont is number of working buffer in buf_stack
// find() is for finding specific type buffer
class CodecStream {
public:
    int buf_count;
    int stream_size;
	CodecBuffer *buf_stack;
    CodecBuffer *find(CodecBufferType type);
};

// interface of compression algorithm
class CodecInterface {
public:
    virtual CodecType getCodecType()         = 0;
	virtual void initStream(CodecStream *cs) = 0;
	virtual int compressBlock  ()            = 0;
    virtual int decompressBlock()            = 0;
    virtual int getTotalIn()                 = 0;
    virtual int getTotalOut()                = 0;
};

// codec settings used in LZ compressor
class CodecSettings {
public:
    void Set(DWord bit_blk_cap, DWord bit_lkp_cap,
        DWord bit_lkp_hsh, DWord bit_mtch_len,
        DWord bit_mtch_pos, DWord bit_bffr_cnt,
        DWord bit_runs);
    // settings values in bits
    DWord bit_blk_cap;  // file chunk size 
    DWord bit_lkp_cap;  // lookup table size 
    DWord bit_lkp_hsh;  // number of bytes to count hash for lookup table 
    DWord bit_mtch_len; // max match lenght size
    DWord bit_mtch_pos; // max match position size
    DWord bit_bffr_cnt; // working buffer count
    DWord bit_runs;
    // in bytes
    DWord byte_blk_cap, byte_lkp_cap, byte_lkp_hsh,
        byte_mtch_len, byte_mtch_pos, byte_bffr_cnt,
        byte_runs;
    // masks
    DWord mask_blk_cap, mask_lkp_cap, mask_lkp_hsh,
        mask_mtch_len, mask_mtch_pos, mask_bffr_cnt,
        mask_runs;
};

// archive header
struct ArchiveHeader {
    Byte  a_sig[4];   // LZHX
    DWord a_sig2;     // 0xFFFFFFFB
    DWord a_fcnt;      // num of files
    DWord a_flgs;     // flags
    QWord a_unc_size; // archive uncompressed size
    QWord a_cmp_size; // archive compressed size
};

// file in archive header
struct FileHeader {
    Byte  f_flags;    // flags
    DWord f_cmp_size; // compressed and decompressed sizes
    DWord f_dcm_size;
    QWord f_cr_time;  // creation, last acces and write times
    QWord f_la_time;
    QWord f_lw_time;
    DWord f_attr;     // file attributes
    DWord f_nm_cnt;   // file name bytes count
    DWord f_cnt_hsh;  // FNV hash
};

} // namespace

#endif // LZHX_TYPES_H
