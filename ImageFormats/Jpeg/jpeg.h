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
