#ifndef JPEG_H
#define JPEG_H

//#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/iptc.h"
#include "Metadata/xmp.h"
#include "Metadata/metareport.h"

#include <iostream>
#include <bitset>
#include "Utilities/bit.h"

class Jpeg : public QObject
{
    Q_OBJECT

public:
    Jpeg();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               IPTC *iptc,
               Exif *exif);
    
    void initSegCodeHash();
    void getJpgSegments(MetadataParameters &p, ImageMetadata &m);
    bool getDimensions(MetadataParameters &p, ImageMetadata &m);

    QHash<QString, quint32> segmentHash;

private:
    void readAppSegments(MetadataParameters &p);
    void parseFrameHeader(MetadataParameters &p, uint marker, quint16 len);
    void parseHuffmanTable(MetadataParameters &p, quint16 len);
    void parseQuantizationTable(MetadataParameters &p, quint16 len);
    void parseSOSHeader(MetadataParameters &p, quint16 len);
    void decodeScan(MetadataParameters &p);

    int  huff2Signed(uint val, uint bits);

    void buildMask();
    void bufAppend(uint &buf, quint8 byte, uint &consumed);
    uint bufExtract(uint &buf, uint nBits, uint &consumed);
    uint bufPeek(uint &buf, uint nBits);

    void rptHuff();
    void rptMCU();

    enum ColorModel{CMYK, YCBCR};
    int colorModel;
    enum SubFormat{Baseline_DCT, Extended_DCT, Progressive_DCT, Lossless, Arithmetric_DCT,
                   ArithmetricProgressive_DCT, ArithmetricLossless, UnknownJPEGSubformat};
    int mcu[3][64];
    int zzmcu[64] = { 0, 1, 8,16, 9, 2, 3,10,
                     17,24,32,25,18,11, 4, 5,
                     12,19,26,33,40,48,41,34,
                     27,20,13, 6, 7,14,21,28,
                     35,42,49,56,57,50,43,36,
                     29,22,15,23,30,37,44,51,
                     58,59,52,45,38,31,39,46,
                     53,60,61,54,47,55,62,63};
    /*
    Sequential,Zigzag
     0,0	 1,1     2,5     3,6     4,14	 5,15	 6,27	 7,28
     8,2	 9,4	10,7	11,13	12,16	13,26	14,29	15,42
    16,3	17,8	18,12	19,17	20,25	21,30	22,41	23,43
    24,9	25,11	26,18	27,24	28,31	29,40	30,44	31,53
    32,10	33,19	34,23	35,32	36,39	37,45	38,52	39,54
    40,20	41,22	42,33	43,38	44,46	45,51	46,55	47,60
    48,21	49,34	50,37	51,47	52,50	53,56	54,59	55,61
    56,35	57,36	58,48	59,49	60,57	61,58	62,62	63,63
    */

    // SOF
    int subFormat;
    bool losslessFormat;
    uint precision;
    quint32 lines;
    quint32 samplesPerLine;
    int componentsInFrame;
    struct Component {
        int Id;
        int horSampleFactor;
        int verSampleFactor;
        int QTableSel;
    };
    QVector<Component> components;

    // DHT Define Huffman Table

    struct DHT {
        int tableID;                   // Specifies one of component: 0 for luminance and 1 for chrominance
        int classID;                   // Specifies is it DC element or AC element of table. 0-DC element 1-AC element
//        QVector<uint> codes;
//        QVector<uint> codeLengths;
    };

    /* huffCode   = binary string       ie 11001 = 25
       huffLength = length of code      ie 11001 length = 5
       huffVal    = lookup value for code (usually used to define next #bits)
       huffResult = value of next huffVal bits
       huffSignedResult = Twos complement of huffResult
    */

    QVector<QString> dhtClassDescription{"DC Table", "AC Table"};

    // [DHT Type (Class/TblID)] [Code Length] [Code, CodeValue (length of bits to read)]
    QMap<int, QMap<uint, QMap<uint, uint>>> dhtMap;    // code, value

    // Quantization table segment
    QMap<int, QVector<int>> dqt;
    QVector<QString> dqtDescription{"Luminance", "Chrominance"};

    // Frame header segment
    QVector<QString> componentDescription{"Lum: Y   ", "Chrom: Cb", "Chrom: Cr"};

    // Define restart interval
    quint16 restartInterval;

    // Start of scan SOS
    quint32 scanDataOffset;             // start of scan data after SOS header
    int sosComponentCount;
    int huffTblToUse[4];                // component, huff table

    // decode scan data
    unsigned mask[32];

    QHash<quint32, QString> segCodeHash;
    quint32 order;
    QString err;
    bool isXmp;

};

#endif // JPEG_H

/*

signed CimgDecode::HuffmanDc2Signed(unsigned nVal,unsigned nBits)
{
    if (nVal >= (unsigned)(1<<(nBits-1))) {
        return (signed)(nVal);
    } else {
        return (signed)( nVal - ((1<<nBits)-1) );
    }
}

void CimgDecode::GenLookupHuffMask()
{
    unsigned int nMask;
    for (unsigned nLen=0;nLen<32;nLen++)
    {
        nMask = (1 << (nLen))-1;
        nMask <<= 32-nLen;
        m_anHuffMaskLookup[nLen] = nMask;
    }
}

// Extract a specified number of bits from a 32-bit holding register
//
// INPUT:
// - nWord				= The 32-bit holding register
// - nBits				= Number of bits (leftmost) to extract from the holding register
// PRE:
// - m_anHuffMaskLookup[]
// RETURN:
// - The subset of bits extracted from the holding register
// NOTE:
// - This routine is inlined for speed
//
inline unsigned CimgDecode::ExtractBits(unsigned nWord,unsigned nBits)
{
    unsigned nVal;
    nVal = (nWord & m_anHuffMaskLookup[nBits]) >> (32-nBits);
    return nVal;
}


*/
