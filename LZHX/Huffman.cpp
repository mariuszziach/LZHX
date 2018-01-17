/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

#include "Huffman.h"

using namespace LZHX;

// huffman tree node
HuffmanTree::HuffmanTree() { clear(); }
void HuffmanTree::clear() {
    symbol = freq = 0;
    left = right = nullptr;
}
bool HuffmanTree::isLeaf() {
	if (left == nullptr && right == nullptr) { return true; }
	return false;
}

// huffman code
void HuffmanCode::clear()  { code = 0; bit_count = 0; }
HuffmanCode::HuffmanCode() { clear(); }

// huffman symbol pair
HuffmanSymbolPair::HuffmanSymbolPair() { first = second = nullptr; }

// huffman main class
void Huffman::reset() {
	for (int i = 0; i < alphabet_size * 2; i++) {
		nodes[i].clear();
		nodes[i].symbol = i;
	}
	for (int i = 0; i < alphabet_size; i++) codes[i].clear();
	root = nullptr;
	parents = nodes + alphabet_size;
}

// create histogram
void Huffman::countFrequencies(Byte *buf, int in_size) {
	for (int i = 0; i < in_size; i++) nodes[buf[i]].freq++;
}

// sorting histogram
void Huffman::findLowestFreqSymbolPair(HuffmanSymbolPair &sp) {
	sp.first = sp.second = nullptr;
	int i = 0;
	while (i < nodes_array_size) {
		HuffmanTree *currentNode = nodes + i;
		if (currentNode->freq > 0) {
			if (sp.first == nullptr)       { sp.first  = currentNode; }
			else if (sp.second == nullptr) { sp.second = currentNode; }
			else if (sp.first->freq  > currentNode->freq) { sp.first  = currentNode; }
			else if (sp.second->freq > currentNode->freq) { sp.second = currentNode; }
		}
		i++;
	}
}

// tree building
void Huffman::buildTree() {
	while (true) {
		HuffmanSymbolPair sp;
		findLowestFreqSymbolPair(sp);
		if (sp.second == nullptr) {
			root = sp.first;
			break;
		} else {
			parents->freq   = sp.first->freq + sp.second->freq;
			parents->right  = sp.first;
			parents->left   = sp.second;
			sp.first ->freq = 0;
			sp.second->freq = 0;
			parents++;
		}
	}
}

// make huffan codes
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

// writing tree to stream
void Huffman::writeTree(HuffmanTree *node) {
	if (node->isLeaf()) {
		bit_stream->writeBit(1);
		bit_stream->writeBits(node->symbol, 8);
	} else { bit_stream->writeBit(0); }
	if (node->right != nullptr) { writeTree(node->right); }
	if (node->left  != nullptr) { writeTree(node->left);  }
}

// reading tree from stream
HuffmanTree *Huffman::readTree(HuffmanTree *node) {
	int bit = bit_stream->readBit();
	if (bit == 1) {
		node->symbol = bit_stream->readBits(8);
		node->right  = node->left  = nullptr;
		node->freq   = 0;
		return node;
	} else if (bit == 0) {
		node->right = node + 1;
		node->left  = readTree(node->right) + 1;
		return readTree(node->left);
	}
	return nullptr;
}

// reading symbol from stream using huffman tree
int Huffman::decodeSymbol(HuffmanTree *node) {
	if (node->isLeaf()) { return node->symbol; }
	int bit = bit_stream->readBit();
	if      (bit == 1) { return decodeSymbol(node->right); }
	else if (bit == 0) { return decodeSymbol(node->left);  }
	else                 return 0;
}

// constructors/destructors
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

// info
CodecType Huffman::getCodecType() { return CT_HF; }
int Huffman::getTotalIn()         { return total_in; }
int Huffman::getTotalOut()        { return total_out; }

// init
void Huffman::initStream(CodecStream *codec_stream) {
	this->codec_stream = codec_stream;
	this->total_in = this->total_out = 0;
}

// compress block
int Huffman::compressBlock() {
    int in_size;
    CodecBuffer *cb_in, *cb_out;
    Byte *in, *out;

    // find one LZ compressed buffer in buffer stack and compress it
    cb_in        = codec_stream->find(CBT_LZ);
	cb_out       = codec_stream->find(CBT_EMPTY); 
    cb_out->type = CBT_HF;
    in           = cb_in ->mem;
	out          = cb_out->mem;
	cb_in->type  = CBT_EMPTY;
	in_size      = cb_in->size;

	reset();
    
    // write input size to stream
	bit_stream->assignBuffer(out);
	bit_stream->writeBits(in_size, 32);

    // build and write tree
	countFrequencies(in, in_size);
	buildTree();
	makeCodes(root, 0, 0);
	writeTree(root);

    // for each byte write assigned code to output
	for (int i = 0; i < in_size; i++) {
		HuffmanCode *currentCode = codes + in[i];
		bit_stream->writeBits(currentCode->code, currentCode->bit_count);
	}

    // close last byte with 0 bits
	while (bit_stream->getBitPos() > 0) bit_stream->writeBit(0);

    // update info
	total_in    += in_size;
	total_out   += bit_stream->getBytePos();
	cb_out->size = bit_stream->getBytePos();

	return cb_out->size;
}

// decompress block
int Huffman::decompressBlock() {
    int dec_size;
    CodecBuffer *cb_in, *cb_out;
    Byte *in, *out;

    // find one huffman buffer and one empty
	cb_in        = codec_stream->find(CBT_HF);
	cb_out       = codec_stream->find(CBT_EMPTY); 
    cb_out->type = CBT_LZ;
    in           = cb_in ->mem;
	out          = cb_out->mem;
	cb_in->type  = CBT_EMPTY;

	reset();

    // read decompressed size and tree
	bit_stream->assignBuffer(in);
    dec_size = bit_stream->readBits(32);
	readTree(nodes);

    // decode each symbol
	for (int o = 0; o < dec_size; o++)
        out[o] = decodeSymbol(nodes);

    // update info
	total_in    += cb_in->size;
	total_out   += dec_size;
	cb_out->size = dec_size;

	return dec_size;
}
