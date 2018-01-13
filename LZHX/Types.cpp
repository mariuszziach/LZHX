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
void CodecSettings::Set(DWord bit_blk_cap, DWord bit_lkp_cap,
    DWord bit_lkp_hsh, DWord bit_mtch_len,
    DWord bit_mtch_pos, DWord bit_bffr_cnt, 
    DWord bit_runs) {
    this->bit_blk_cap = bit_blk_cap;
    byte_blk_cap = 1 << bit_blk_cap;
    mask_blk_cap = byte_blk_cap - 1;

    this->bit_lkp_cap = bit_lkp_cap;
    byte_lkp_cap = 1 << bit_lkp_cap;
    mask_lkp_cap = byte_lkp_cap - 1;

    this->bit_lkp_hsh = bit_lkp_hsh;
    byte_lkp_hsh = 1 << bit_lkp_hsh;
    mask_lkp_hsh = byte_lkp_hsh - 1;

    this->bit_mtch_len = bit_mtch_len;
    byte_mtch_len = 1 << bit_mtch_len;
    mask_mtch_len = byte_mtch_len - 1;

    this->bit_mtch_pos = bit_mtch_pos;
    byte_mtch_pos = 1 << bit_mtch_pos;
    mask_mtch_pos = byte_mtch_pos - 1;

    this->bit_bffr_cnt = bit_bffr_cnt;
    byte_bffr_cnt = 1 << bit_bffr_cnt;
    mask_bffr_cnt = byte_bffr_cnt - 1;

    this->bit_runs = bit_runs;
    byte_runs = 1 << bit_runs;
    mask_runs = byte_runs - 1;

}