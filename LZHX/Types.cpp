/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

// LHZX
#include "Types.h"

using namespace LZHX;

CodecBuffer *CodecStream::find(CodecBufferType type) {
    for (int i = 0; i < buf_count; i++) {
        if (buf_stack[i].type == type) {
            return buf_stack + i;
        }
    }
    return nullptr;
}

// convert bit values to byte values and masks
void CodecSettings::Set(DWord bbc, DWord blc,  DWord blh,
    DWord bml, DWord bmp, DWord bbcn,  DWord br) {

    // working buffers capacity
    this->bit_blk_cap = bbc;
    byte_blk_cap = 1 << bbc;
    mask_blk_cap = byte_blk_cap - 1;

    // lookup table capacity
    this->bit_lkp_cap = blc;
    byte_lkp_cap = 1 << blc;
    mask_lkp_cap = byte_lkp_cap - 1;

    // how many bytes are used to count hash for lookup table key
    this->bit_lkp_hsh = blh;
    byte_lkp_hsh = 1 << blh;
    mask_lkp_hsh = byte_lkp_hsh - 1;

    // max LZ match size
    this->bit_mtch_len = bml;
    byte_mtch_len = 1 << bml;
    mask_mtch_len = byte_mtch_len - 1;

    // max LZ match pos
    this->bit_mtch_pos = bmp;
    byte_mtch_pos = 1 << bmp;
    mask_mtch_pos = byte_mtch_pos - 1;

    // working buffer count
    this->bit_bffr_cnt = bbcn;
    byte_bffr_cnt = 1 << bbcn;
    mask_bffr_cnt = byte_bffr_cnt - 1;

    // how many times LZ searching procedure is trying to find best match
    this->bit_runs = br;
    byte_runs = 1 << br;
    mask_runs = byte_runs - 1;

}
