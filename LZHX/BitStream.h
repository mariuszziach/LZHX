/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

#ifndef LZHX_BITSTREAM_H
#define LZHX_BITSTREAM_H

#include "Types.h"

namespace LZHX {

// easy bit reading/writing used in huffman compression
class BitStream {
private:
    Byte *buf;
	int bit_pos, byte_pos;
public:
	BitStream();
	void assignBuffer(Byte *buf);
    // pos
	int  getBitPos();
	int  getBytePos();
	void setBitPos(int bit_pos);
	void setBytePos(int byte_pos);
	void resetPos();
    // write
	void writeBit(int bit);
	void writeBits(int bits, int count);
    // read
	int readBit();
	int readBits(int count);
};

} // namespace

#endif // LZHX_BITSTREAM_H
