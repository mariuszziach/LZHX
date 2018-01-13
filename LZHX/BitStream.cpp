/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

#include "BitStream.h"

using namespace LZHX;

BitStream::BitStream() { buf = nullptr; resetPos(); }
void BitStream::assignBuffer(Byte *buf) { this->buf = buf; resetPos(); }
int  BitStream::getBitPos() { return bit_pos; }
int  BitStream::getBytePos() { return byte_pos; }
void BitStream::setBitPos(int bit_pos) { this->bit_pos = bit_pos; }
void BitStream::setBytePos(int byte_pos) { this->byte_pos = byte_pos; }
void BitStream::resetPos() { bit_pos = byte_pos = 0; }
void BitStream::writeBit(int bit) {
	if (bit_pos == 0) { buf[byte_pos] = bit & 1; bit_pos++; }
    else { buf[byte_pos] |= ((bit & 1) << bit_pos++); }
	if (bit_pos > 7) { bit_pos = 0; byte_pos++; }
}
void BitStream::writeBits(int bits, int count) {
    if (count == 8 && bit_pos == 0) { buf[byte_pos++] = bits; }
    else { for (int i = 0; i < count; i++) { writeBit(bits >> i); } }
}
int BitStream::readBit() {
	int ret = (buf[byte_pos] >> bit_pos++) & 1;
	if (bit_pos > 7) { bit_pos = 0; byte_pos++; }
	return ret;
}
int BitStream::readBits(int count) {
	int bits = 0;
	if (count == 8 && bit_pos == 0) { bits = buf[byte_pos++]; }
	else { for (int i = 0; i < count; i++) { bits |= readBit() << i; } }
	return bits;
}
