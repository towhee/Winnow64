#ifndef JPGDECODER_H
#define JPGDECODER_H

#include <QtWidgets>
#include <cmath>
#include <fstream>

class BitReader
{
public:
    BitReader(const std::string& filename);
    ~BitReader();

    // quint8 nextByte = 0;
    // quint8 nextBit = 0;
    std::ifstream inFile;
    bool hasBits();
    quint8 readByte();
    uint readWord();
    uint readBit();
    uint readBits(const uint length);
    void align();

// private:
    // Restart interval Markers
    const quint8 RST0 = 0xD0;
    const quint8 RST1 = 0xD1;
    const quint8 RST2 = 0xD2;
    const quint8 RST3 = 0xD3;
    const quint8 RST4 = 0xD4;
    const quint8 RST5 = 0xD5;
    const quint8 RST6 = 0xD6;
    const quint8 RST7 = 0xD7;

private:
    QBuffer buffer;
    quint8 nextByte = 0;
    quint8 nextBit = 0;
};

class JpgDecoder : public QObject
{
    Q_OBJECT

public:
    JpgDecoder();
    bool decode(QString fPath);

private:
    QElapsedTimer t;
    bool isDebug  = false;

    typedef unsigned char quint8;
    typedef unsigned int uint;

    // Start of Frame markers, non-differential, Huffman coding
    const quint8 SOF0 = 0xC0; // Baseline DCT
    const quint8 SOF1 = 0xC1; // Extended sequential DCT
    const quint8 SOF2 = 0xC2; // Progressive DCT
    const quint8 SOF3 = 0xC3; // Lossless (sequential)

    // Start of Frame markers, differential, Huffman coding
    const quint8 SOF5 = 0xC5; // Differential sequential DCT
    const quint8 SOF6 = 0xC6; // Differential progressive DCT
    const quint8 SOF7 = 0xC7; // Differential lossless (sequential)

    // Start of Frame markers, non-differential, arithmetic coding
    const quint8 SOF9 = 0xC9; // Extended sequential DCT
    const quint8 SOF10 = 0xCA; // Progressive DCT
    const quint8 SOF11 = 0xCB; // Lossless (sequential)

    // Start of Frame markers, differential, arithmetic coding
    const quint8 SOF13 = 0xCD; // Differential sequential DCT
    const quint8 SOF14 = 0xCE; // Differential progressive DCT
    const quint8 SOF15 = 0xCF; // Differential lossless (sequential)

    // Define Huffman Table(s)
    const quint8 DHT = 0xC4;

    // JPEG extensions
    const quint8 JPG = 0xC8;

    // Define Arithmetic Coding Conditioning(s)
    const quint8 DAC = 0xCC;

    // Restart interval Markers
    const quint8 RST0 = 0xD0;
    const quint8 RST1 = 0xD1;
    const quint8 RST2 = 0xD2;
    const quint8 RST3 = 0xD3;
    const quint8 RST4 = 0xD4;
    const quint8 RST5 = 0xD5;
    const quint8 RST6 = 0xD6;
    const quint8 RST7 = 0xD7;

    // Other Markers
    const quint8 SOI = 0xD8; // Start of Image
    const quint8 EOI = 0xD9; // End of Image
    const quint8 SOS = 0xDA; // Start of Scan
    const quint8 DQT = 0xDB; // Define Quantization Table(s)
    const quint8 DNL = 0xDC; // Define Number of Lines
    const quint8 DRI = 0xDD; // Define Restart Interval
    const quint8 DHP = 0xDE; // Define Hierarchical Progression
    const quint8 EXP = 0xDF; // Expand Reference Component(s)

    // APPN Markers
    const quint8 APP0 = 0xE0;
    const quint8 APP1 = 0xE1;
    const quint8 APP2 = 0xE2;
    const quint8 APP3 = 0xE3;
    const quint8 APP4 = 0xE4;
    const quint8 APP5 = 0xE5;
    const quint8 APP6 = 0xE6;
    const quint8 APP7 = 0xE7;
    const quint8 APP8 = 0xE8;
    const quint8 APP9 = 0xE9;
    const quint8 APP10 = 0xEA;
    const quint8 APP11 = 0xEB;
    const quint8 APP12 = 0xEC;
    const quint8 APP13 = 0xED;
    const quint8 APP14 = 0xEE;
    const quint8 APP15 = 0xEF;

    // Misc Markers
    const quint8 JPG0 = 0xF0;
    const quint8 JPG1 = 0xF1;
    const quint8 JPG2 = 0xF2;
    const quint8 JPG3 = 0xF3;
    const quint8 JPG4 = 0xF4;
    const quint8 JPG5 = 0xF5;
    const quint8 JPG6 = 0xF6;
    const quint8 JPG7 = 0xF7;
    const quint8 JPG8 = 0xF8;
    const quint8 JPG9 = 0xF9;
    const quint8 JPG10 = 0xFA;
    const quint8 JPG11 = 0xFB;
    const quint8 JPG12 = 0xFC;
    const quint8 JPG13 = 0xFD;
    const quint8 COM = 0xFE;
    const quint8 TEM = 0x01;

    struct QuantizationTable {
        uint table[64] = { 0 };
        bool set = false;

        // Default constructor
        QuantizationTable() = default;

        // Constructor
        QuantizationTable(const uint (&initTable)[64], bool initSet)
            : set(initSet) {
            std::copy(std::begin(initTable), std::end(initTable), std::begin(table));
        }
    };

    struct HuffmanTable {
        quint8 offsets[17] = { 0 };
        quint8 symbols[176] = { 0 };
        uint codes[176] = { 0 };
        bool set = false;

        // Default constructor
        HuffmanTable() = default;

        // Constructor
        HuffmanTable(const quint8 (&initOffsets)[17],
                     const quint8 (&initSymbols)[176],
                     const uint (&initCodes)[176],
                     bool initSet)
            : set(initSet) {
            std::copy(std::begin(initOffsets), std::end(initOffsets), std::begin(offsets));
            std::copy(std::begin(initSymbols), std::end(initSymbols), std::begin(symbols));
            std::copy(std::begin(initCodes), std::end(initCodes), std::begin(codes));
        }
    };

    struct ColorComponent {
        quint8 horizontalSamplingFactor = 0;
        quint8 verticalSamplingFactor = 0;
        quint8 quantizationTableID = 0;
        quint8 huffmanDCTableID = 0;
        quint8 huffmanACTableID = 0;
        bool usedInFrame = false;
        bool usedInScan = false;

        // Default constructor
        ColorComponent() = default;
    };

    struct Block {
        union {
            int y[64] = { 0 };
            int r[64];
        };
        union {
            int cb[64] = { 0 };
            int g [64];
        };
        union {
            int cr[64] = { 0 };
            int b [64];
        };
        int* operator[](uint i) {
            switch (i) {
            case 0:
                return y;
            case 1:
                return cb;
            case 2:
                return cr;
            default:
                return nullptr;
            }
        }
    };

    struct JPGImage {
        QuantizationTable quantizationTables[4];
        HuffmanTable huffmanDCTables[4];
        HuffmanTable huffmanACTables[4];
        ColorComponent colorComponents[3];

        quint8 frameType = 0;
        uint height = 0;
        uint width = 0;
        quint8 numComponents = 0;
        bool zeroBased = false;

        quint8 componentsInScan = 0;
        quint8 startOfSelection = 0;
        quint8 endOfSelection = 0;
        quint8 successiveApproximationHigh = 0;
        quint8 successiveApproximationLow = 0;

        uint restartInterval = 0;

        Block* blocks = nullptr;

        bool valid = true;

        uint blockHeight = 0;
        uint blockWidth = 0;
        uint blockHeightReal = 0;
        uint blockWidthReal = 0;

        quint8 horizontalSamplingFactor = 0;
        quint8 verticalSamplingFactor = 0;

        // Default constructor
        JPGImage() = default;
    };

    struct BMPImage {
        uint height = 0;
        uint width = 0;

        Block* blocks = nullptr;

        uint blockHeight = 0;
        uint blockWidth = 0;
    };

    const quint8 zigZagMap[64] = {
        0,   1,  8, 16,  9,  2,  3, 10,
        17, 24, 32, 25, 18, 11,  4,  5,
        12, 19, 26, 33, 40, 48, 41, 34,
        27, 20, 13,  6,  7, 14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36,
        29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46,
        53, 60, 61, 54, 47, 55, 62, 63
    };

    // IDCT scaling factors
    const float m0 = 2.0 * std::cos(1.0 / 16.0 * 2.0 * M_PI);
    const float m1 = 2.0 * std::cos(2.0 / 16.0 * 2.0 * M_PI);
    const float m3 = 2.0 * std::cos(2.0 / 16.0 * 2.0 * M_PI);
    const float m5 = 2.0 * std::cos(3.0 / 16.0 * 2.0 * M_PI);
    const float m2 = m0 - m5;
    const float m4 = m0 + m5;

    const float s0 = std::cos(0.0 / 16.0 * M_PI) / std::sqrt(8);
    const float s1 = std::cos(1.0 / 16.0 * M_PI) / 2.0;
    const float s2 = std::cos(2.0 / 16.0 * M_PI) / 2.0;
    const float s3 = std::cos(3.0 / 16.0 * M_PI) / 2.0;
    const float s4 = std::cos(4.0 / 16.0 * M_PI) / 2.0;
    const float s5 = std::cos(5.0 / 16.0 * M_PI) / 2.0;
    const float s6 = std::cos(6.0 / 16.0 * M_PI) / 2.0;
    const float s7 = std::cos(7.0 / 16.0 * M_PI) / 2.0;

    // standard tables

    const QuantizationTable qTableY50 = {
        {
            16,  11,  10,  16,  24,  40,  51,  61,
            12,  12,  14,  19,  26,  58,  60,  55,
            14,  13,  16,  24,  40,  57,  69,  56,
            14,  17,  22,  29,  51,  87,  80,  62,
            18,  22,  37,  56,  68, 109, 103,  77,
            24,  35,  55,  64,  81, 104, 113,  92,
            49,  64,  78,  87, 103, 121, 120, 101,
            72,  92,  95,  98, 112, 100, 103,  99
        },
        true
    };

    const QuantizationTable qTableCbCr50 = {
        {
            17, 18, 24, 47, 99, 99, 99, 99,
            18, 21, 26, 66, 99, 99, 99, 99,
            24, 26, 56, 99, 99, 99, 99, 99,
            47, 66, 99, 99, 99, 99, 99, 99,
            99, 99, 99, 99, 99, 99, 99, 99,
            99, 99, 99, 99, 99, 99, 99, 99,
            99, 99, 99, 99, 99, 99, 99, 99,
            99, 99, 99, 99, 99, 99, 99, 99
        },
        true
    };

    const QuantizationTable qTableY75 = {
        {
            16/2,  11/2,  10/2,  16/2,  24/2,  40/2,  51/2,  61/2,
            12/2,  12/2,  14/2,  19/2,  26/2,  58/2,  60/2,  55/2,
            14/2,  13/2,  16/2,  24/2,  40/2,  57/2,  69/2,  56/2,
            14/2,  17/2,  22/2,  29/2,  51/2,  87/2,  80/2,  62/2,
            18/2,  22/2,  37/2,  56/2,  68/2, 109/2, 103/2,  77/2,
            24/2,  35/2,  55/2,  64/2,  81/2, 104/2, 113/2,  92/2,
            49/2,  64/2,  78/2,  87/2, 103/2, 121/2, 120/2, 101/2,
            72/2,  92/2,  95/2,  98/2, 112/2, 100/2, 103/2,  99/2
        },
        true
    };

    const QuantizationTable qTableCbCr75 = {
        {
            17/2, 18/2, 24/2, 47/2, 99/2, 99/2, 99/2, 99/2,
            18/2, 21/2, 26/2, 66/2, 99/2, 99/2, 99/2, 99/2,
            24/2, 26/2, 56/2, 99/2, 99/2, 99/2, 99/2, 99/2,
            47/2, 66/2, 99/2, 99/2, 99/2, 99/2, 99/2, 99/2,
            99/2, 99/2, 99/2, 99/2, 99/2, 99/2, 99/2, 99/2,
            99/2, 99/2, 99/2, 99/2, 99/2, 99/2, 99/2, 99/2,
            99/2, 99/2, 99/2, 99/2, 99/2, 99/2, 99/2, 99/2,
            99/2, 99/2, 99/2, 99/2, 99/2, 99/2, 99/2, 99/2
        },
        true
    };

    const QuantizationTable qTableY100 = {
        {
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1
        },
        true
    };

    const QuantizationTable qTableCbCr100 = {
        {
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1
        },
        true
    };

    const QuantizationTable* const qTables50[3] {  &qTableY50,  &qTableCbCr50,  &qTableCbCr50 };
    const QuantizationTable* const qTables75[3] {  &qTableY75,  &qTableCbCr75,  &qTableCbCr75 };
    const QuantizationTable* const qTables100[3] { &qTableY100, &qTableCbCr100, &qTableCbCr100 };

    HuffmanTable hDCTableY = {
        { 0, 0, 1, 6, 7, 8, 9, 10, 11, 12, 12, 12, 12, 12, 12, 12, 12 },
        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b },
        {},
        false
    };

    HuffmanTable hDCTableCbCr = {
        { 0, 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 12, 12, 12, 12 },
        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b },
        {},
        false
    };

    HuffmanTable hACTableY = {
        { 0, 0, 2, 3, 6, 9, 11, 15, 18, 23, 28, 32, 36, 36, 36, 37, 162 },
        {
            0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
            0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
            0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
            0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
            0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
            0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
            0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
            0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
            0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
            0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
            0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
            0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
            0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
            0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
            0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
            0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
            0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
            0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
            0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
            0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
            0xf9, 0xfa
        },
        {},
        false
    };

    HuffmanTable hACTableCbCr = {
        { 0, 0, 2, 3, 5, 9, 13, 16, 20, 27, 32, 36, 40, 40, 41, 43, 162 },
        {
            0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
            0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
            0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
            0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
            0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
            0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
            0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
            0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
            0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
            0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
            0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
            0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
            0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
            0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
            0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
            0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
            0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
            0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
            0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
            0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
            0xf9, 0xfa
        },
        {},
        false
    };

    HuffmanTable* const dcTables[3] { &hDCTableY, &hDCTableCbCr, &hDCTableCbCr };
    HuffmanTable* const acTables[3] { &hACTableY, &hACTableCbCr, &hACTableCbCr };

    void readStartOfFrame(BitReader& bitReader, JPGImage* const image);
    void readQuantizationTable(BitReader& bitReader, JPGImage* const image);
    void generateCodes(HuffmanTable& hTable);
    void readHuffmanTable(BitReader& bitReader, JPGImage* const image);
    void readStartOfScan(BitReader& bitReader, JPGImage* const image);
    void readRestartInterval(BitReader& bitReader, JPGImage* const image);
    void readAPPN(BitReader& bitReader, JPGImage* const image);
    void readComment(BitReader& bitReader, JPGImage* const image);
    void printFrameInfo(const JPGImage* const image);
    void printScanInfo(const JPGImage* const image);
    void readFrameHeader(BitReader& bitReader, JPGImage* const image);
    void decodeHuffmanData(BitReader& bitReader, JPGImage* const image);
    void dequantizeBlockComponent(const QuantizationTable& qTable, int* const component);
    void dequantize(const JPGImage* const image);
    void readScans(BitReader& bitReader, JPGImage* const image);
    JPGImage* readJPG(const std::string& filename);
    quint8 getNextSymbol(BitReader& bitReader, const HuffmanTable& hTable);
    bool decodeBlockComponent(
        const JPGImage* const image,
        BitReader& bitReader,
        int* const component,
        int& previousDC,
        uint& skips,
        const HuffmanTable& dcTable,
        const HuffmanTable& acTable
        );
    void inverseDCTBlockComponent(int* const component);
    void inverseDCT(const JPGImage* const image);
    void YCbCrToRGBBlock(Block& yBlock, const Block& cbcrBlock,
                         const uint vSamp, const uint hSamp,
                         const uint v, const uint h);
    void YCbCrToRGB(const JPGImage* const image);
    void putInt(quint8*& bufferPos, const uint v);
    void putShort(quint8*& bufferPos, const uint v);
    void writeBMP(const JPGImage* const image, const std::string& filename);

};
#endif // JPGDECODER_H
