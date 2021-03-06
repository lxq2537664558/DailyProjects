#include "pch.h"

#include <stdint.h>

#include <queue>

#include "stream.h"
#include "huffman.h"
#include "bitStream.h"

struct HuffmanNode {
    uint32_t byte : 8;
    uint32_t leftID : 10;
    uint32_t rightID : 10;
};

static const int HUFFMAN_NODE_NULL_ID = 0;
static const int HUFFMAN_NODE_FIRST_ID = 1;
static const int HUFFMAN_NODE_COUNT = 512;
static const int HUFFMAN_COMPRESSION_OVERHEAD = 512;
static const int BYTE_COUNT = 256;

struct ByteCodeBits {
    int bitCount;
    int bits[3];
};

namespace HuffmanCompressionAlgo {

    typedef pair<int, int> IntPair;
    struct HuffmanNodeIDFreqGreater {
        bool operator () (const IntPair &l, const IntPair &r) const {
            return l.second > r.second;
        }
    };
    static int _buildHuffmanTree(HuffmanNode nodes[HUFFMAN_NODE_COUNT], int byteFreq[BYTE_COUNT]) {
        int nextNodeID = HUFFMAN_NODE_FIRST_ID;
        for (int i = 0; i < BYTE_COUNT; ++i) {
            if (byteFreq[i] > 0) {
                auto &node = nodes[nextNodeID++];
                node.byte = i;
                node.leftID = node.rightID = HUFFMAN_NODE_NULL_ID;
            }
        }

        priority_queue<IntPair, vector<IntPair>, HuffmanNodeIDFreqGreater> q;
        for (int i = HUFFMAN_NODE_FIRST_ID; i < nextNodeID; ++i) {
            q.push(IntPair(i, byteFreq[nodes[i].byte]));
        }

        while (q.size() > 1) {
            auto l = q.top(); q.pop();
            auto r = q.top(); q.pop();
            int id = nextNodeID++;
            auto &node = nodes[id];
            assert(nextNodeID <= HUFFMAN_NODE_COUNT);
            node.leftID = l.first;
            node.rightID = r.first;
            q.push(IntPair(id, l.second + r.second));
        }

        return q.top().first;
    }

    static void _recurisveGenerateCodeStr(string &curS, int nodeID, HuffmanNode nodes[HUFFMAN_NODE_COUNT], string codeStrs[BYTE_COUNT]) {
        auto &node = nodes[nodeID];
        if (node.leftID == HUFFMAN_NODE_NULL_ID) {
            assert(node.rightID == HUFFMAN_NODE_NULL_ID);
            codeStrs[node.byte] = curS.empty() ? "0" : curS;
        } else {
            assert(node.rightID != HUFFMAN_NODE_NULL_ID);
            curS += '0';
            _recurisveGenerateCodeStr(curS, node.leftID, nodes, codeStrs);
            curS.pop_back();
            curS += '1';
            _recurisveGenerateCodeStr(curS, node.rightID, nodes, codeStrs);
            curS.pop_back();
        }
    }
    static void _generateCodeBitsWithHuffmanTree(ByteCodeBits codebits[BYTE_COUNT], int rootID, HuffmanNode nodes[HUFFMAN_NODE_COUNT]) {
        string codeStrs[BYTE_COUNT];
        {
            string s;
            _recurisveGenerateCodeStr(s, rootID, nodes, codeStrs);
        }
        for (int i = 0; i < BYTE_COUNT; ++i) {
            if (codeStrs[i].empty()) {
                codebits[i].bitCount = 0;
                continue;
            } 

            auto &code = codebits[i];
            OutputBitStream bs(&code.bits, sizeof(code.bits) * 8);
            for (char c : codeStrs[i]) bs.writeBool(c == '1' ? 1 : 0);
            code.bitCount = bs.pos();
        }
    }

    static void _recursiveSerializeHuffmanNode(OutputBitStream *bs, int nodeID, HuffmanNode nodes[HUFFMAN_NODE_COUNT]) {
        auto &node = nodes[nodeID];
        if (node.leftID == HUFFMAN_NODE_NULL_ID) {
            bs->writeBool(true);
            bs->writeInt(uint8_t(node.byte));
        } else {
            bs->writeBool(false);
            _recursiveSerializeHuffmanNode(bs, node.leftID, nodes);
            _recursiveSerializeHuffmanNode(bs, node.rightID, nodes);
        }
    }
    static void _serializeHuffmanTree(OutputBitStream *bs, int rootID, HuffmanNode nodes[HUFFMAN_NODE_COUNT]) {
        _recursiveSerializeHuffmanNode(bs, rootID, nodes);
    }

    static void _compressBufferWithCodeBits(OutputBitStream *bs, const uint8_t *src, int srcsize, ByteCodeBits codebits[BYTE_COUNT]) {
        for (int i = 0; i < srcsize; ++i) {
            auto &code = codebits[src[i]];
            assert(code.bitCount > 0);
            bs->writeBits(&code.bits, code.bitCount);
        }
    }

    static int compress(const uint8_t *src, int srcsize, uint8_t *dest, int destsize) {
        assert(src != nullptr && srcsize > 0);
        assert(dest != nullptr && destsize >= srcsize + HUFFMAN_COMPRESSION_OVERHEAD);

        int byteFreq[BYTE_COUNT] = {0};
        for (int i = 0; i < srcsize; ++i) ++byteFreq[src[i]];

        HuffmanNode nodes[HUFFMAN_NODE_COUNT] = {0};
        int rootID = _buildHuffmanTree(nodes, byteFreq);

        ByteCodeBits codebits[BYTE_COUNT];
        _generateCodeBitsWithHuffmanTree(codebits, rootID, nodes);

        OutputBitStream bs(dest, destsize * 8);
        _serializeHuffmanTree(&bs, rootID, nodes);
        _compressBufferWithCodeBits(&bs, src, srcsize, codebits);

        return (bs.pos() + 7) / 8;
    }
}

namespace HuffmanUncompressionAlgo {

    static int _recursiveDeserializeHuffmanNode(InputBitStream *bs, int nodeID, HuffmanNode nodes[HUFFMAN_NODE_COUNT]) {
        auto &node = nodes[nodeID];
        if (bs->readBool()) {
            node.byte = bs->readInt<uint8_t>();
            node.leftID = node.rightID = HUFFMAN_NODE_NULL_ID;
            return nodeID + 1;
        } else {
            node.leftID = nodeID + 1;
            node.rightID = _recursiveDeserializeHuffmanNode(bs, node.leftID, nodes);
            return _recursiveDeserializeHuffmanNode(bs, node.rightID, nodes);
        }
    }

    static int _deserializeHuffmanTree(InputBitStream *bs, HuffmanNode nodes[HUFFMAN_NODE_COUNT]) {
        int rootID = HUFFMAN_NODE_FIRST_ID;
        _recursiveDeserializeHuffmanNode(bs, rootID, nodes);
        return rootID;
    }

    static int _leafCount(int nodeID, HuffmanNode nodes[HUFFMAN_NODE_COUNT]) {
        auto &node = nodes[nodeID];
        if (node.leftID == HUFFMAN_NODE_NULL_ID) return 1;
        else return _leafCount(node.leftID, nodes) + _leafCount(node.rightID, nodes);
    }

    template<typename IntT>
    static void _recursiveBuildQuickLookupTableWithHuffmanNode(int depth, IntT code, int nodeID, HuffmanNode nodes[HUFFMAN_NODE_COUNT], uint16_t lookupTable[1 << (sizeof(IntT) * 8)]) {
        if (depth > sizeof(IntT) * 8) return;

        auto &node = nodes[nodeID];
        if (node.leftID == HUFFMAN_NODE_NULL_ID) {
            int bitCount = depth == 0 ? 1 : depth;
            IntT step = IntT(1 << bitCount);
            for (int i = 0; i < 1 << (sizeof(IntT) * 8 - bitCount); ++i, code += step) {
                lookupTable[code] = node.byte | uint16_t(bitCount << 8);
            }
        } else {
            _recursiveBuildQuickLookupTableWithHuffmanNode<IntT>(depth + 1, code | (0 << depth), node.leftID, nodes, lookupTable);
            _recursiveBuildQuickLookupTableWithHuffmanNode<IntT>(depth + 1, code | (1 << depth), node.rightID, nodes, lookupTable);
        }
    }
    template<typename IntT>
    static void _buildQuickLookupTable(int rootID, HuffmanNode nodes[HUFFMAN_NODE_COUNT], uint16_t lookupTable[1 << (sizeof(IntT) * 8)]) {
        _recursiveBuildQuickLookupTableWithHuffmanNode<IntT>(0, 0, rootID, nodes, lookupTable);
    }
    template<typename IntT>
    static int _quickLookup(InputBitStream *bs, uint16_t lookupTable[1 << (sizeof(IntT) * 8)]) {
        uint16_t v = lookupTable[bs->fetchInt<IntT>()];
        if (v >> 8) {
            bs->advance(v >> 8);
            return v & 0xff;
        } else {
            return -1;
        }
    }
    static int _lookup(InputBitStream *bs, int rootID, HuffmanNode nodes[HUFFMAN_NODE_COUNT]) {
        auto node = &nodes[rootID];
        while (node->leftID != HUFFMAN_NODE_NULL_ID) {
            node = &nodes[bs->readBool() ? node->rightID : node->leftID];
        }
        return node->byte;
    }
    static void _uncompressBufferWithHuffmanTree(InputBitStream *bs, int rootID, HuffmanNode nodes[HUFFMAN_NODE_COUNT], uint8_t *dest, int destsize) {
        int i = 0;

        if (_leafCount(rootID, nodes) > 2) {
            typedef uint8_t QuickLookupType;
            uint16_t lookupTable[1 << (sizeof(QuickLookupType) * 8)] = {0};
            _buildQuickLookupTable<QuickLookupType>(rootID, nodes, lookupTable);

            for (; i + (int)sizeof(QuickLookupType) * 8 <= destsize; ++i) {
                int byte;
                if ((byte = _quickLookup<QuickLookupType>(bs, lookupTable)) == -1) {
                    byte = _lookup(bs, rootID, nodes);
                }
                dest[i] = byte;
            }
        }

        for (; i < destsize; ++i) {
            dest[i] = _lookup(bs, rootID, nodes);
        }
    }

    static void uncompress(const uint8_t *src, int srcsize, uint8_t *dest, int destsize) {
        InputBitStream bs(src, srcsize * 8);

        HuffmanNode nodes[HUFFMAN_NODE_COUNT] = {0};
        int rootID = _deserializeHuffmanTree(&bs, nodes);

        _uncompressBufferWithHuffmanTree(&bs, rootID, nodes, dest, destsize);
    }
}

HuffmanCompressor::HuffmanCompressor(int chunkSize): mChunkSize(chunkSize) {
}

void HuffmanCompressor::compress(IInputStream *si, IOutputStream *so) {
    vector<uint8_t> readBuf(mChunkSize), writeBuf(mChunkSize + HUFFMAN_COMPRESSION_OVERHEAD + 8);

    for (int i = 0, size = si->size(); i < size; i += mChunkSize) {
        int srcsize = min(size - i, mChunkSize);
        if (si->read(&readBuf[0], srcsize) != srcsize) assert(0);

        uint32_t *dest = (uint32_t*)&writeBuf[0];
        dest[0] = srcsize;
        dest[1] = HuffmanCompressionAlgo::compress(&readBuf[0], srcsize, &writeBuf[0] + 8, (int)writeBuf.size() - 8);;
        assert(dest[1] + 8 < writeBuf.size());
        if (so->write(dest, dest[1] + 8) != (int)dest[1] + 8) assert(0);
    }
}

void HuffmanCompressor::uncompress(IInputStream *si, IOutputStream *so) {
    vector<uint8_t> readBuf, writeBuf;

    for (int i = 0, size = si->size(); i < size; ) {
        uint32_t sizes[2];
        if (si->read(sizes, sizeof(sizes)) != sizeof(sizes)) assert(0);
        readBuf.resize(sizes[1]);
        writeBuf.resize(sizes[0]);

        if (si->read(&readBuf[0], (int)readBuf.size()) != (int)readBuf.size()) assert(0);

        HuffmanUncompressionAlgo::uncompress(&readBuf[0], (int)readBuf.size(), &writeBuf[0], (int)writeBuf.size());
        if (so->write(&writeBuf[0], (int)writeBuf.size()) != (int)writeBuf.size()) assert(0);

        i += (int)readBuf.size() + 8;
    }
}
