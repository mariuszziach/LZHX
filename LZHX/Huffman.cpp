/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

#include "Huffman.h"
#include <cassert>
#include <iostream>

using namespace std;
using namespace LZHX;

HuffmanTree::HuffmanTree() { clear(); }
void HuffmanTree::clear() {
    symbol = 0;
    freq = 0;
    left = nullptr;
    right = nullptr;
}
bool HuffmanTree::isLeaf() {
	if (left == nullptr && right == nullptr) { return true; }
	return false;
}
void HuffmanCode::clear()  { code = 0; bit_count = 0; }
HuffmanCode::HuffmanCode() { clear(); }
HuffmanSymbolPair::HuffmanSymbolPair() { first = second = nullptr; }
void Huffman::reset() {
	for (int i = 0; i < alphabet_size * 2; i++) {
		nodes[i].clear();
		nodes[i].symbol = i;
	}
	for (int i = 0; i < alphabet_size; i++) codes[i].clear();
	root = nullptr;
	parents = nodes + alphabet_size;
}
void Huffman::countFrequencies(Byte *buf, int in_size) {
	for (int i = 0; i < in_size; i++) nodes[buf[i]].freq++;
}
void Huffman::findLowestFreqSymbolPair(HuffmanSymbolPair &sp) {
	sp.first = sp.second = nullptr;
	int i = 0;
	while (i < nodes_array_size) {
		HuffmanTree *currentNode = nodes + i;
		if (currentNode->freq > 0) {
			if (sp.first == nullptr) { sp.first = currentNode; }
			else if (sp.second == nullptr) { sp.second = currentNode; }
			else if (sp.first->freq  > currentNode->freq) { sp.first = currentNode; }
			else if (sp.second->freq > currentNode->freq) { sp.second = currentNode; }
		}
		i++;
	}
}
void Huffman::buildTree() {
	while (true) {
		HuffmanSymbolPair sp;
		findLowestFreqSymbolPair(sp);
		if (sp.second == nullptr) {
			root = sp.first;
			break;
		} else {
			parents->freq = sp.first->freq + sp.second->freq;
			parents->right = sp.first;
			parents->left = sp.second;
			sp.first->freq = 0;
			sp.second->freq = 0;
			parents++;
		}
	}
}
void Huffman::makeCodes(HuffmanTree *node, int code, int bit_count) {
	if (node->isLeaf()) {
		codes[node->symbol].code = code;
		codes[node->symbol].bit_count = bit_count;
	}
	if (node->right != nullptr)
		makeCodes(node->right, code | (1 << bit_count), bit_count + 1);
	if (node->left != nullptr)
		makeCodes(node->left, code, bit_count + 1);
}
void Huffman::writeTree(HuffmanTree *node) {
	if (node->isLeaf()) {
		bit_stream->writeBit(1);
		bit_stream->writeBits(node->symbol, 8);
	} else { bit_stream->writeBit(0); }
	if (node->right != nullptr) { writeTree(node->right); }
	if (node->left != nullptr) { writeTree(node->left); }
}
HuffmanTree *Huffman::readTree(HuffmanTree *node) {
	int bit = bit_stream->readBit();
	if (bit == 1) {
		node->symbol = bit_stream->readBits(8);
		node->right = nullptr;
		node->left = nullptr;
		node->freq = 0;
		return node;
	} else if (bit == 0) {
		node->right = node + 1;
		node->left = readTree(node->right) + 1;
		return readTree(node->left);
	}
	return nullptr;
}
int Huffman::decodeSymbol(HuffmanTree *node) {
	if (node->isLeaf()) { return node->symbol; }
	int bit = bit_stream->readBit();
	if      (bit == 1) { return decodeSymbol(node->right); }
	else if (bit == 0) { return decodeSymbol(node->left);  }
	else                 return 0;
}
Huffman::Huffman(int alphabet_size) {
	this->alphabet_size = alphabet_size;
	nodes_array_size = alphabet_size * 2;
	nodes = new HuffmanTree[nodes_array_size];
	codes = new HuffmanCode[alphabet_size];
	bit_stream = new BitStream;
}
Huffman::~Huffman() {
	delete[] nodes;
	delete[] codes;
	delete bit_stream;
}
CodecType Huffman::getCodecType() { return CT_HF; }
int Huffman::getTotalIn()  { return total_in; }
int Huffman::getTotalOut() { return total_out; }
void Huffman::initStream(CodecStream *codec_stream) {
	this->codec_stream = codec_stream;
	this->total_in = this->total_out = 0;
}
int Huffman::compressBlock() {
    CodecBuffer *cb_in, *cb_out;
    Byte *in, *out;
    cb_in        = codec_stream->find(CBT_LZ);
	cb_out       = codec_stream->find(CBT_EMPTY); 
    cb_out->type = CBT_HF;
    in           = cb_in->mem;
	out          = cb_out->mem;
	cb_in->type  = CBT_EMPTY;
	int in_size  = cb_in->size;
	reset();
    
	bit_stream->assignBuffer(out);
	bit_stream->writeBits(in_size, 32);
	countFrequencies(in, in_size);
	buildTree();
	makeCodes(root, 0, 0);
	writeTree(root);

	for (int i = 0; i < in_size; i++) {
		HuffmanCode *currentCode = codes + in[i];
		bit_stream->writeBits(currentCode->code, currentCode->bit_count);
	}

	while (bit_stream->getBitPos() > 0) bit_stream->writeBit(0);

	total_in  += in_size;
	total_out += bit_stream->getBytePos();
	cb_out->size = bit_stream->getBytePos();

	return cb_out->size;
}

int Huffman::decompressBlock() {
    CodecBuffer *cb_in, *cb_out;
    Byte *in, *out;

	cb_in = codec_stream->find(CBT_HF);
	cb_out = codec_stream->find(CBT_EMPTY); 
    cb_out->type = CBT_LZ;
    in           = cb_in->mem;
	out          = cb_out->mem;
	cb_in->type = CBT_EMPTY;

	int in_size = cb_in->size;

	reset();

	bit_stream->assignBuffer(in);
	int dec_size = bit_stream->readBits(32);
	readTree(nodes);

	for (int o = 0; o < dec_size; o++)
        out[o] = decodeSymbol(nodes);

	total_in += in_size;
	total_out += dec_size;
	
	cb_out->size = dec_size;

	return dec_size;
}
