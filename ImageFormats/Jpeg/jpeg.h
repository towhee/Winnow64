#ifndef JPEG_H
#define JPEG_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/iptc.h"
#include "Metadata/xmp.h"
#include "Metadata/metareport.h"

#include <iostream>
//#include "Utilities/bit.h"

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

    void buildIdctLookup();

    void rptHuff();
    void rptMCU();
    void rptIDCT();
    void rptRGB();

    enum ColorModel{CMYK, YCBCR};
    int colorModel;
    enum SubFormat{Baseline_DCT, Extended_DCT, Progressive_DCT, Lossless, Arithmetric_DCT,
                   ArithmetricProgressive_DCT, ArithmetricLossless, UnknownJPEGSubformat};
    // MCU for 3 channels
    int   mcu[3][8][8];
    float idct[3][8][8];
    int   rgb[3][8][8];
    int zzmcu[64] = { 0, 1, 8,16, 9, 2, 3,10,
                     17,24,32,25,18,11, 4, 5,
                     12,19,26,33,40,48,41,34,
                     27,20,13, 6, 7,14,21,28,
                     35,42,49,56,57,50,43,36,
                     29,22,15,23,30,37,44,51,
                     58,59,52,45,38,31,39,46,
                     53,60,61,54,47,55,62,63};

    const int zz[64][2] = {{0,0}, {0,1}, {1,0}, {2,0}, {1,1}, {0,2},{ 0,3}, {1,2},
                           {2,1}, {3,0}, {4,0}, {3,1}, {2,2}, {1,3}, {0,4}, {0,5},
                           {1,4}, {2,3}, {3,2}, {4,1}, {5,0}, {6,0}, {5,1}, {4,2},
                           {3,3}, {2,4}, {1,5}, {0,6}, {0,7}, {1,6}, {2,5}, {3,4},
                           {4,3}, {5,2}, {6,1}, {7,0}, {7,1}, {6,2}, {5,3}, {4,4},
                           {3,5}, {2,6}, {1,7}, {2,7}, {3,6}, {4,5}, {5,4}, {6,3},
                           {7,2}, {7,3}, {6,4}, {5,5}, {4,6}, {3,7}, {4,7}, {5,6},
                           {6,5}, {7,4}, {7,5}, {6,6}, {5,7}, {6,7}, {7,6}, {7,7}};


    /*
    const std::pair<const int, const int> zzOrder[64] =
    {
        {0,0},
        {0,1}, {1,0},
        {2,0}, {1,1}, {0,2},
        {0,3}, {1,2}, {2,1}, {3,0},
        {4,0}, {3,1}, {2,2}, {1,3}, {0,4},
        {0,5}, {1,4}, {2,3}, {3,2}, {4,1}, {5,0},
        {6,0}, {5,1}, {4,2}, {3,3}, {2,4}, {1,5}, {0,6},
        {0,7}, {1,6}, {2,5}, {3,4}, {4,3}, {5,2}, {6,1}, {7,0},
        {7,1}, {6,2}, {5,3}, {4,4}, {3,5}, {2,6}, {1,7},
        {2,7}, {3,6}, {4,5}, {5,4}, {6,3}, {7,2},
        {7,3}, {6,4}, {5,5}, {4,6}, {3,7},
        {4,7}, {5,6}, {6,5}, {7,4},
        {7,5}, {6,6}, {5,7},
        {6,7}, {7,6},
        {7,7}
    };
    */

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

    // Inverse Discrete Cosine Transform (IDCT)
    float fIdctLookup[3][8][8][8][8]; // c,y,x,v,u
    int   iIdctLookup[3][8][8][8][8];

    QHash<quint32, QString> segCodeHash;
    quint32 order;
    QString err;
    bool isXmp;

};

#endif // JPEG_H

/*
iDCT:
Applying these formulas directly is computationally expensive, especially
when there have been developed faster algorithms for implementing forward or
inverse DCT. A notable one called AA&N leaves only 5 multiplies and 29 adds
to be done in the DCT itself. More info and an implementation of it can be
found in the free software for JPEG encoders/decoders made by Independent JPEG
Group (IJG), their C source can be found at www.ijg.org.

Y = 0.299 R + 0.587 G + 0.114 B
Cb = - 0.1687 R - 0.3313 G + 0.5 B + 128
Cr = 0.5 R - 0.4187 G - 0.0813 B + 128

R = Y + 1.402 (Cr-128)
G = Y - 0.34414 (Cb-128) - 0.71414 (Cr-128)
B = Y + 1.772 (Cb-128)
*/
