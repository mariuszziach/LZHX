/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

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
int LZDictionaryBuffer::convPos(bool abs_to_rel, int pos) {
    if (abs_to_rel) {
        if (this->pos >= pos) return  this->pos - pos;
        else                  return (this->cap - pos) + this->pos;
    } else {
        if (this->pos >= pos) return this->pos -  pos;
        else                  return this->cap - (pos - this->pos);
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
    this->hash_map   = new LZDictionaryNode[cdc_sttgs->byte_lkp_cap ];  
    this->cyclic_buf = new LZDictionaryNode[cdc_sttgs->byte_mtch_pos];
    this->best_match = new LZMatch; 
    this->cdc_sttgs  = cdc_sttgs;
    this->cyclic_iterator = 0;
    for (int i = 0; i < (int)cdc_sttgs->byte_lkp_cap;  i++) hash_map  [i].clear();
    for (int i = 0; i < (int)cdc_sttgs->byte_mtch_pos; i++) cyclic_buf[i].clear();
}
LZMatchFinder::~LZMatchFinder() {
    delete[] this->hash_map;
    delete[] this->cyclic_buf;
    delete this->best_match;
}
void LZMatchFinder::assignBuffer(Byte *buf, int buf_size, LZDictionaryBuffer *lz_buf) {
    this->buf = buf;
    this->buf_size = buf_size;
    this->lz_buf = lz_buf;
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
    if (cyclic_iterator >= int(cdc_sttgs->byte_mtch_pos))
        cyclic_iterator = 0;
    LZDictionaryNode *current = hash_map   + hash(buf + pos);
    LZDictionaryNode *empty   = cyclic_buf + cyclic_iterator++;
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
    current = hash_map + hash(buf + pos);
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
void LZ::initStream(CodecStream *codec_stream) {
    this->codec_stream = codec_stream;
    this->total_in     = 0;
    this->total_out    = 0;
}

// compress block
int LZ::compressBlock() {
    int o[4], in_size(0), out_size(0), i(0);
    CodecBuffer *cb_in, *cb_out[4];
    Byte *in, *out[4];
   
    // find raw buffer to compress
    cb_in = codec_stream->find(CBT_RAW);
    in    = cb_in->mem;

    // find 4 empty buffers for output
    // 0 -> instructions; 1 -> pos; 2 -> len; 3 ->literal;
    for (int i = 0; i < 4; i++) {
        o[i]            = 0;
        cb_out[i]       = codec_stream->find(CBT_EMPTY); 
        cb_out[i]->type = CBT_LZ;
        out[i]          = cb_out[i]->mem;

        // first byte of buffer is buffer type
        out[i][o[i]++]  = i;
    }

    // input stream after compression will be empty
    cb_in->type = CBT_EMPTY;
    in_size     = cb_in->size;

    // assign buffer to match finder, write input size int 1 stream
    lz_mf->assignBuffer(in, in_size, lz_buf);
    o[1] += write32To8Buf(out[1] + o[1], in_size);

    while (i < in_size) {

        // search for best match
        if (i + int(cdc_sttgs->byte_lkp_hsh) > in_size) lz_match->len = 0;
        else lz_match = lz_mf->find(i);


        if ((lz_match->len > 3) &&
            (lz_match->len + lz_match->pos + int(cdc_sttgs->byte_lkp_hsh)) < in_size) {

            // write match to stream
            lz_match->pos = lz_buf->convPos(true, lz_match->pos);
            out[0][o[0]++] = (Byte)(1);
            out[2][o[2]++] = (Byte)(lz_match->len);
            o[1]          += write16To8Buf(out[1] + o[1], lz_match->pos);

            // add skipped bytes into dictionary
            while (lz_match->len--) {
                lz_mf->insert(i);
                lz_buf->putByte(in[i++]);
            }
        }
        else {
            // write literal
            out[0][o[0]++] = (Byte)(0);
            out[3][o[3]++] = (Byte)(in[i]);

            // add byte into dictionary
            lz_mf->insert(i);
            lz_buf->putByte(in[i++]);
        }
    }

    // set output buffer sizes
    for (int i = 0; i < 4; i++) {
        out_size       += o[i];
        cb_out[i]->size = o[i];
    }

    // update processed bytes length
    total_in  += in_size;
    total_out += out_size;
    return out_size;
}

// decompress block
int LZ::decompressBlock() {
    int i[4], dec_size(0);
    CodecBuffer *cb_out, *cb_in[4];
    Byte *out, *temp_in[4], *in[4];

    // find empty buffer for output
    cb_out = codec_stream->find(CBT_EMPTY);
    out = cb_out->mem;

    // find 4 lz input buffers
    for (int j = 0; j < 4; j++) {
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
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) 
            if (temp_in[i][0] == j)
                in[j] = temp_in[i];

    // read uncompressed size
    dec_size = read32From8Buf(in[1] + i[1]);
    i[1] += sizeof(DWord);

    for (int o = 0; o < dec_size; ) {

        // what to do?
        if (in[0][i[0]++]) {
            int pos, len;

            // read match
            pos = read16From8Buf(in[1] + i[1]); i[1] += sizeof(Word);
            pos = lz_buf->convPos(false, pos);
            len = in[2][i[2]++];

            // copy match
            for (int j = 0; j < len; j++) {
                Byte b       = lz_buf->getByte(pos + j);
                tmp_btebf[j] = b;
                out[o++]     = b;
            }

            // insert processed bytes into dictionary
            for (int j = 0; j < len; j++)  lz_buf->putByte(tmp_btebf[j]);
            
        }
        else {
            // read uncompressed byte
            lz_buf->putByte(in[3][i[3]]);
            out[o++] = in[3][i[3]++];
        }
    }

    // update info
    total_in    += i[0] + i[1] + i[2] + i[3];
    total_out   += dec_size;
    cb_out->size = dec_size;
    return dec_size;
}
