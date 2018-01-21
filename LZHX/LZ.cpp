/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

#include <iostream>
#include <memory>
#include <cstdlib>

// LHZX
#include "LZ.h"
#include "Utils.h"

using namespace LZHX;

// dictionary buffer
LZDictionaryBuffer::~LZDictionaryBuffer() { if (arr) delete[] arr; }
LZDictionaryBuffer::LZDictionaryBuffer(int cap) {
    this->cap = cap;
    arr = new Byte[cap];
    pos = size = 0;
    for (int i = 0; i < cap; i++) arr[i] = 0;
}
// insert byte into ring buffer
Byte *LZDictionaryBuffer::putByte(Byte val) {
    if (pos >= cap) pos = 0;
    if (size < cap) size++;
    arr[pos] = val;
    return &arr[pos++];
}
// convert position absolute to relative and conversely
int LZDictionaryBuffer::convPos(bool abs_to_rel, int p) {
    if (abs_to_rel) {
        if (this->pos >= p) return  this->pos - p;
        else                return (this->cap - p) + this->pos;
    } else {
        if (this->pos >= p) return this->pos -  p;
        else                return this->cap - (p - this->pos);
    }
}
Byte LZDictionaryBuffer::getByte(int p) { return arr[p]; }
int  LZDictionaryBuffer::getPos()       { return pos; }

// lz match
LZMatch::LZMatch()    { clear(); }
void LZMatch::clear() {pos = len = 0; }
void LZMatch::copy(LZMatch *lzm) {
    pos = lzm->pos;
    len = lzm->len;
}

// lz dictionary node - item of lookup table - first item of linked list
LZDictionaryNode::LZDictionaryNode() {
    next = prev = nullptr; clear();
}
void LZDictionaryNode::clear() {
    if (next != nullptr) next->prev = nullptr;
    if (prev != nullptr) prev->next = nullptr;
    next = prev = nullptr;
    valid = false;
    pos = 0;
}

// match finder
LZMatchFinder::LZMatchFinder(CodecSettings *cdc_sttgs) {
    this->lz_buf     = nullptr;
    this->buf        = nullptr;
    this->buf_size   = 0;
    this->lkp_tab    = new LZDictionaryNode[cdc_sttgs->byte_lkp_cap ];  
    this->dict_tab   = new LZDictionaryNode[cdc_sttgs->byte_mtch_pos];
    this->best_match = new LZMatch; 
    this->cdc_sttgs  = cdc_sttgs;
    this->dict_i = 0;
    for (int i = 0; i < (int)cdc_sttgs->byte_lkp_cap;  i++) lkp_tab  [i].clear();
    for (int i = 0; i < (int)cdc_sttgs->byte_mtch_pos; i++) dict_tab[i].clear();
}
LZMatchFinder::~LZMatchFinder() {
    delete[] this->lkp_tab;
    delete[] this->dict_tab;
    delete this->best_match;
}
void LZMatchFinder::assignBuffer(Byte *b, int bs, LZDictionaryBuffer *lzb) {
    this->buf      = b;
    this->buf_size = bs;
    this->lz_buf   = lzb;
}

// FNV hash
int LZMatchFinder::hash(Byte *in) {
    DWord hash = 0x811C9DC5;
    for (int i = 0; i < int(cdc_sttgs->byte_lkp_hsh); i++) {
        hash ^= in[i];
        hash *= 0x1000193;
    }
    return hash & cdc_sttgs->mask_lkp_cap;
}

// insert new item into dictionary
void LZMatchFinder::insert(int pos) {
    if (buf_size - pos  <= int(cdc_sttgs->byte_lkp_hsh )) return;
    if (dict_i >= int(cdc_sttgs->byte_mtch_pos))
        dict_i = 0;
    LZDictionaryNode *current = lkp_tab   + hash(buf + pos);
    LZDictionaryNode *empty   = dict_tab + dict_i++;
    empty->clear();
    empty->pos     = current->pos;
    empty->next    = current->next;
    if (empty->next) empty->next->prev = empty;
    empty->prev    = current;
    empty->valid   = true;
    current->pos   = lz_buf->getPos();
    current->next  = empty;
    current->prev  = nullptr;
    current->valid = true;
}

// find item in dictionary
LZMatch *LZMatchFinder::find(int pos) {
    int runs, i;
    LZDictionaryNode *current;
    best_match->clear();
    if (buf_size - pos <= (int)(cdc_sttgs->byte_lkp_hsh) || pos == 0) return best_match;
    current = lkp_tab + hash(buf + pos);
    runs    = 0;
    while (current && (runs++ < int(cdc_sttgs->byte_runs))) {
        if (current->valid) {
            i = 0;
            while (buf[pos + i] == lz_buf->getByte(current->pos + i)) {
                if (pos + i >= buf_size - 1 || current->pos + i >= lz_buf->size - 1) break;
                if (i >= int(cdc_sttgs->mask_mtch_len)) break;
                i++;
            }
            if (best_match->len < i) {
                best_match->pos = current->pos;
                best_match->len = i;
            }
        }
        current = current->next;
    }
    return best_match;
}

// lz algorith main class
LZ::LZ(CodecSettings *cdc_sttgs) {
    bit_stream = new BitStream; 
    lz_mf      = new LZMatchFinder(cdc_sttgs);
    lz_buf     = new LZDictionaryBuffer(cdc_sttgs->byte_mtch_pos);
    tmp_btebf  = new Byte[cdc_sttgs->byte_mtch_len];
    lz_match   = nullptr;
    rolz_match = nullptr;
    this->cdc_sttgs = cdc_sttgs;
}
LZ::~LZ() {
    if (bit_stream) delete bit_stream;
    if (lz_mf)      delete lz_mf;
    if (lz_buf)     delete lz_buf;
    if (tmp_btebf)  delete [] tmp_btebf;
}

// info
CodecType LZ::getCodecType() { return CT_LZ; }
int LZ::getTotalIn()  { return total_in;  }
int LZ::getTotalOut() { return total_out; }

// init
void LZ::initStream(CodecStream *cs) {
    this->codec_stream = cs;
    this->total_in     = 0;
    this->total_out    = 0;
}

// compress block
int LZ::compressBlock() {
    int o[4], in_size(0), out_size(0), i(0);
    CodecBuffer *cb_in, *cb_out[ILZSN];
    Byte *in, *out[ILZSN];
   
    // find raw buffer to compress
    cb_in = codec_stream->find(CBT_RAW);
    in    = cb_in->mem;

    // find 4 empty buffers for output
    // 0 -> instructions; 1 -> pos; 2 -> len; 3 ->literal;
    for (int j = 0; j < ILZSN; j++) {
        o     [j]       = 0;
        cb_out[j]       = codec_stream->find(CBT_EMPTY); 
        cb_out[j]->type = CBT_LZ;
        out   [j]       = cb_out[j]->mem;

        // first byte of buffer is buffer type
        out[j][o[j]++]  = Byte(j);
    }

    // input stream after compression will be empty
    cb_in->type = CBT_EMPTY;
    in_size     = cb_in->size;

    // assign buffer to match finder, write input size int 1 stream
    lz_mf->assignBuffer(in, in_size, lz_buf);
    o[ILZMPS] += write32To8Buf(out[ILZMPS] + o[ILZMPS], in_size);
   
    match_empty.clear();
    lz_match = &match_empty;

    while (i < in_size) {

        // search for best match
        if (i + int(cdc_sttgs->byte_lkp_hsh) > in_size) {
            lz_match->len   = 0;
        } else {
            lz_match   = lz_mf->find(i);
        }

        if ((lz_match->len > ILZMINML) &&
            (lz_match->len + lz_match->pos + int(cdc_sttgs->byte_lkp_hsh)) < in_size) {

            // write match to stream
            lz_match->pos = lz_buf->convPos(true, lz_match->pos);
            if (lz_match->pos < 256) {
                out[ILZIS] [o[ILZIS] ++] = (Byte)(ILZIM1);
                out[ILZMLS][o[ILZMLS]++] = (Byte)(lz_match->len);
                out[ILZMPS][o[ILZMPS]++] = (Byte)(lz_match->pos);
            } else {
                out[ILZIS] [o[ILZIS] ++] = (Byte)(ILZIM2);
                out[ILZMLS][o[ILZMLS]++] = (Byte)(lz_match->len);
                o[ILZMPS] += write16To8Buf(out[ILZMPS] + o[ILZMPS], Word(lz_match->pos));
            }
            // add skipped bytes into dictionary
            while (lz_match->len--) {
                lz_mf->insert(i);
                lz_buf->putByte(in[i++]);
            }
        } else {
            if (in[i] != ILZIM1 && in[i] != ILZIM2 && in[i] != ILZIL) {
                out[ILZIS][o[ILZIS]++] = in[i];
            } else {
                // write literal
                out[ILZIS][o[ILZIS]++] = (Byte)(ILZIL);
                out[ILZLS][o[ILZLS]++] = (Byte)(in[i]);
            }
            // add byte into dictionary
            lz_mf->insert(i);
            lz_buf->putByte(in[i++]);
        }
    }

    // set output buffer sizes
    for (int j = 0; j < ILZSN; j++) {
        out_size       += o[j];
        cb_out[j]->size = o[j];
    }

    // update processed bytes length
    total_in  += in_size;
    total_out += out_size;
    return out_size;
}

// decompress block
int LZ::decompressBlock() {
    int i[ILZSN], dec_size(0);
    CodecBuffer *cb_out, *cb_in[ILZSN];
    Byte *out, *temp_in[ILZSN], *in[ILZSN];

    // find empty buffer for output
    cb_out = codec_stream->find(CBT_EMPTY);
    out = cb_out->mem;

    // find 4 lz input buffers
    for (int j = 0; j < ILZSN; j++) {
        cb_in  [j]       = codec_stream->find(CBT_LZ);
        cb_in  [j]->type = CBT_EMPTY;
        temp_in[j]       = cb_in[j]->mem;
        in     [j]       = nullptr;
        i      [j]       = 1;
    }

    // output will be raw data
    cb_out->type = CBT_RAW;

    // sort buffers by their function
    // 0 -> instructions; 1 -> pos; 2 -> len; 3 ->literal;
    for (int k = 0; k < ILZSN; k++)
        for (int j = 0; j < ILZSN; j++)
            if (temp_in[k][0] == j)
                in[j] = temp_in[k];

    // read uncompressed size
    dec_size = read32From8Buf(in[ILZMPS] + i[ILZMPS]);
    i[ILZMPS] += sizeof(DWord);

    for (int o = 0; o < dec_size; ) {
        Byte c = in[ILZIS][i[ILZIS]++];

        // what to do?
        if (c == ILZIM1 || c == ILZIM2) {
            int pos(0), len(0);
            
            // read match
            if (c == ILZIM2) {
                pos = read16From8Buf(in[ILZMPS] + i[ILZMPS]);
                i[ILZMPS] += sizeof(Word);
            } else if (c == ILZIM1) {
                pos = in[ILZMPS][i[ILZMPS]++];
            }
            pos = lz_buf->convPos(false, pos);
            len = in[ILZMLS][i[ILZMLS]++];

            // copy match
            for (int j = 0; j < len; j++) {
                Byte b       = lz_buf->getByte(pos + j);
                tmp_btebf[j] = b;
                out[o++]     = b;
            }

            // insert processed bytes into dictionary
            for (int j = 0; j < len; j++)  lz_buf->putByte(tmp_btebf[j]);
            
        } else if (c == ILZIL) {
            // read uncompressed byte
            lz_buf->putByte(in[ILZLS][i[ILZLS]]);
            out[o++] = in[ILZLS][i[ILZLS]++];
        } else {
            lz_buf->putByte(c);
            out[o++] = c;
        }
    }

    // update info
    for (int j = 0; j < ILZSN; j++) {
        total_in += i[j];
    }

    total_out   += dec_size;
    cb_out->size = dec_size;
    return dec_size;
}
