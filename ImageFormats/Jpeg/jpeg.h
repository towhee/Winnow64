#ifndef JPEG_H
#define JPEG_H

#include <QtWidgets>
#include <QColor>
#include <QRgb>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/iptc.h"
#include "Metadata/gps.h"
#include "Metadata/xmp.h"
#include "Metadata/metareport.h"
#include "Metadata/ExifTool.h"

#include <iostream>
#include <stdio.h>
#include <algorithm>

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
               Exif *exif,
               GPS *gps);
    
    void initSegCodeHash();
    void getJpgSegments(MetadataParameters &p, ImageMetadata &m);
    bool getWidthHeight(MetadataParameters &p, int &w, int &h);
    bool getDimensions(MetadataParameters &p, ImageMetadata &m);
    bool getDimensions2(MetadataParameters &p, ImageMetadata &m);

    QHash<QString, quint32> segmentHash;

    void decodeScan(MetadataParameters &p);
    void decodeScan(QString fPath, QImage &image);
    void decodeScan(QFile &file, QImage &image);
    void decodeScan(QByteArray &ba, QImage &image);

    bool embedThumbnail(QString fPath);

private:
    Utilities u;
    void readAppSegments(MetadataParameters &p);
    void parseFrameHeader(MetadataParameters &p, uint marker, quint16 len);
    void parseHuffmanTable(MetadataParameters &p, quint16 len);
    void parseQuantizationTable(MetadataParameters &p, quint16 len);
    void parseSOSHeader(MetadataParameters &p, quint16 len);

    void processMCU(QByteArray &ba, quint32 &offset, uint &buf,
                    uint &consumed, int &bitsToResetMarker, bool &eos);
    void appendMCUScanlines2QImage(QImage &im, int &mcuRow);
    void idctTransformMCU();
    void rgbTransformMCU();
    void appendMCU2QImage();

    int  huff2Signed(uint val, uint bits);

    void buildMask();
    void bufAppend(uint &buf, quint8 byte, uint &consumed);
    uint bufExtract(uint &buf, uint nBits, uint &consumed);
    uint bufPeek(uint &buf, uint nBits);

    void buildIdctLookup();

    void rptHuff();
    void rptMCU(int col, int row);
    void rptIDCT(int col, int row);
    void rptRGB(int col, int row);

    struct JPG {
        int width;
        int height;
        int subFormat;
        uint precision;
        int componentsInFrame;
        int componentId;
        int horSampleFactor;
        int verSampleFactor;
        int QTableSel;
    };

    enum ColorModel{CMYK, YCBCR};
    int colorModel;
    enum SubFormat{Baseline_DCT, Extended_DCT, Progressive_DCT, Lossless, Arithmetric_DCT,
                   ArithmetricProgressive_DCT, ArithmetricLossless, UnknownJPEGSubformat};
    /* image scan mcu row from top left most mcu to bottom right most mcu equivalent
       to 8 scanlines joined together one after the other in format FFRRGGBBFFRRGGBB ... */
    typedef QVector<uint> scanMcuRow;
    scanMcuRow scanLine[8];

    // image width and height in pixels
    int iWidth, iHeight;

    // MCU for 3 channels
    int mcuRows, mcuCols;        // number of rows and columns of mcu in jpeg
    int mcu[3][8][8];
    double idct[3][8][8];
    uint rgb[3][8][8];
    int dcDiff[3];

    // zigzag transform coordinates
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


    // Endian
    bool isBigEndian;

    // DQT
    qint64 dqtOffset;
    quint16 dqtLength;
    int dqtPrecision;

    // SOF
    int subFormat;
    bool losslessFormat;
    uint precision;
    int lines;
    int samplesPerLine;
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

    /* huffCode   = binary string                   ie 11001 = 25
       huffLength = bit length of huffCodecode      ie 11001 length = 5
       huffVal    = lookup table value for huffCode (usually used to define next #bits)
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
    double fIdctLookup[3][8][8][8][8]; // c,y,x,v,u
    int   iIdctLookup[3][8][8][8][8];
    double cosine[106];

    QHash<quint32, QString> segCodeHash;
    quint32 order;
    QString err;
    bool isErr = false;
    bool isXmp;

};

#endif // JPEG_H

/*

void	onImage(int height, int width, int* data)
{
    QImage image = QImage( width, height, QImage::Format_ARGB32 );

    for (int i = 0; i < height; ++i)
        memcpy(image.scanLine(i), data + i * width, width * 4);
}


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

THE JPEG COMPRESSION and THE JPG FILE FORMAT

.............................................



Long ago, I've been looking on the net a good doc which could have explained to

me the JPEG compression, and particularly the JPG file format.

And recently I've found the ISO-ITU JPEG standard in a file called itu-1150.ps

(JPEG standard = ISO standard 10918-1 or CCITT standard recommendation T.81:

 "Information Technology - Digital compression and coding of continuous-tone

still images - Requirements and guidelines")

Though this standard is quite complete, it has a lot of not interesting parts

in its 186 pages, and I had to dig in it, and then write my own JPG viewer,

to get from this standard the main stuff a programmer needs :

    The Baseline Sequential DCT JPG compression.



First a note : Mainly because of the fact that the majority of the JPG files are

Baseline Sequential JPGS, this doc concerns only the Baseline Sequential JPG

compression and particularly the JFIF implementation of it.

It DOES NOT cover the JPG Progresive or Hierarchical compression.

(For more details about these read the itu-1150 standard.

 It can be found at www.wotsit.org or somewhere at www.jpeg.org/jpeg)



I've thought that it would be easier for the reader to understand the JPG

compression if I'll explain the steps of the JPG encoder.

(The decoder steps will be the inverse of the encoder's steps, but in reverse

order, of course)





THE JPEG ENCODER STEPS

----------------------



1) The afine transformation in colour space :  [R G B] -> [Y Cb Cr]

---------------------------------------------------------------------



(It is defined in the CCIR Recommendation 601)



(R,G,B are 8-bit unsigned values)



    | Y  |     |  0.299       0.587       0.114 |   | R |     | 0 |

    | Cb |  =  |- 0.1687    - 0.3313      0.5   | * | G |   + |128|

    | Cr |     |  0.5       - 0.4187    - 0.0813|   | B |     |128|



The new value Y = 0.299*R + 0.587*G + 0.114*B  is called the luminance.

It is the value used by the monochrome monitors to represent an RGB colour.

Physiologically, it represents the intensity of an RGB colour perceived by

the eye.

You see that the formula for Y it's like a weighted-filter with different weights

for each spectral component: the eye is most sensitive to the Green component

then it follows the Red component and the last is the Blue component.



The values Cb =  - 0.1687*R - 0.3313*G + 0.5   *B + 128

       Cr =    0.5   *R - 0.4187*G - 0.0813*B + 128

are called the chromimance values and represent 2 coordinates in a system

which measures the nuance and saturation of the colour ([Approximately], these

values indicate how much blue and how much red is in that colour).

These 2 coordinates are called shortly the chrominance.



[Y,Cb,Cr] to [R,G,B] Conversion (The inverse of the previous transform)

--------------------------------

RGB can be computed directly from YCbCr ( 8-bit unsigned values) as follows:



R = Y                    + 1.402  *(Cr-128)

G = Y - 0.34414*(Cb-128) - 0.71414*(Cr-128)

B = Y + 1.772  *(Cb-128)





A note relating Y,Cb,Cr to the human visual system

---------------------------------------------------

The eye, particulary the retina, has as visual analyzers two kind of cells :

Cells for night view which perceive only nuances of gray ranging from intense

white to the darkest black and cells for the day view which perceive the color

nuance.

The first cells, given an RGB colour, detect a gray level similar to that given

by the luminance value.

The second cells, responsible for the perception of the colour nuance, are the

cells which detects a value related to that of the chrominance.





2) Sampling

------------



The JPEG standard takes into account the fact that the eye seems to be more

sensitive at the luminance of a colour than at the nuance of that colour.

(the white-black view cells have more influence than the day view cells)



So, on most JPGS, luminance is taken in every pixel while the chrominance is

taken as a medium value for a 2x2 block of pixels.

Note that it is not neccessarily that the chrominance to be taken as a medium

value for a 2x2 block , it could be taken in every pixel, but good compression

results are achieved this way, with almost no loss in visual perception of the

new sampled image.



A note : The JPEG standard specifies that for every image component (like, for

example Y) must be defined 2 sampling coefficients: one for the horizontal

sampling and one for vertical sampling.

These sampling coefficients are defined in the JPG file as relative to the

maximum sampling coefficient (more on this later).



3) Level shift

--------------

All 8-bit unsigned values (Y,Cb,Cr) in the image are "level shifted": they are

converted to an 8-bit signed representation, by subtracting 128 from their value.



4) The 8x8 Discrete Cosine Transform (DCT)

------------------------------------------



The image is break into 8x8 blocks of pixels, then for each 8x8 block is

applied the DCT transform. Note that if the X dimension of the original image

is not divisible by 8, the encoder should make it divisible, by completing the

remaining right columns (until X becomes a multiple of 8) with the right-most

column of the original image.

Similar, if the Y dimension is not divisible by 8, the encoder should complete

the remaining lines with the bottom-most line of the original image.

The 8x8 blocks are processed from left to right and from top to bottom.



A note: Since a pixel in the 8x8 block has 3 components (Y,Cb,Cr) the DCT

is applied separately to 3 blocks 8x8:

  The first 8x8 block is the block which contains the luminance of the pixels

   in the original 8x8 block

  The second 8x8 block is the block which contains the Cb value of the pixels

   in the original 8x8 block

  And, similar, the third 8x8 block contains the Cr values.



The purpose of the DCT transform is that instead of processing the original

samples, you work with the spatial frequencies present in the original image.

These spatial frequencies are very related to the level of detail present in an

image. High spatial frequencies corresponds to high levels of detail, while

lower frequencies corresponds to lower levels of detail.



The DCT transform is very similar to the 2D Fourier transform which shifts from

the time domain (the original 8x8 block) to the frequency domain (the new 8x8=

64 coefficients which represents the amplitudes of the spatial frequencies

analyzed)



The mathematical definition of Forward DCT (FDCT) and Inverse DCT (IDCT) is :



FDCT:

       c(u,v)     7   7                 2*x+1                2*y+1

F(u,v) = --------- * sum sum f(x,y) * cos (------- *u*PI)* cos (------ *v*PI)

         4       x=0 y=0                 16                   16



 u,v = 0,1,...,7



      { 1/2 when u=v=0

 c(u,v) = {

      {  1 otherwise





IDCT:

       1     7   7                      2*x+1                2*y+1

f(x,y) =  --- * sum sum c(u,v)*F(u,v)*cos (------- *u*PI)* cos (------ *v*PI)

       4    u=0 v=0                      16                   16



 x,y=0,1...7



Applying these formulas directly is computationally expensive, especially

when there have been developed faster algorithms for implementing forward or

inverse DCT. A notable one called AA&N leaves only 5 multiplies and 29 adds

to be done in the DCT itself. More info and an implementation of it can be

found in the free software for JPEG encoders/decoders made by Independent JPEG

Group (IJG), their C source can be found at www.ijg.org.



5) The zig-zag reordering of the 64 DCT coefficients

-----------------------------------------------------



So, after we performed the DCT transform over a block of 8x8 values, we have

a new 8x8 block.

Then, this 8x8 block is traversed in zig-zag like this :



(The numbers in the 8x8 block indicate the order in which we traverse the

bidimensional 8x8 matrix)

          0, 1, 5, 6,14,15,27,28,

          2, 4, 7,13,16,26,29,42,

          3, 8,12,17,25,30,41,43,

          9,11,18,24,31,40,44,53,

         10,19,23,32,39,45,52,54,

         20,22,33,38,46,51,55,60,

         21,34,37,47,50,56,59,61,

         35,36,48,49,57,58,62,63



As you see , first is the upper-left corner (0,0), then the value at (0,1),

then (1,0) then (2,0), (1,1), (0,2), (0,3), (1,2),  (2,1), (3,0) etc.



After we are done with traversing in zig-zag the 8x8 matrix we have now a vector

with 64 coefficients (0..63)

The reason for this zig-zag traversing is that we traverse the 8x8 DCT coefficients

in the order of increasing the spatial frequencies. So, we get a vector sorted

by the criteria of the spatial frequency:  The first value in the vector (at

index 0) corresponds to the lowest spatial frequency present in the image -

It's called the DC term. As we increase the index in the vector, we get values

corresponding to higher frequencies (The value at index 63 corresponds to the

amplitude of the highest spatial frequency present in the 8x8 block).

The rest of the DCT coefficients are called AC terms.





6) Quantization

----------------



At this stage, we have a sorted vector with 64 values corresponding to the

amplitudes of the 64 spatial frequencies present in the 8x8 block.



These 64 values are quantized: Each value is divided by a dividend specified

in a vector with 64 values --- the quantization table , then it's rounded to

the nearest integer.



 for (i = 0 ; i<=63; i++ )

   vector[i] = (int) (vector[i] / quantization_table[i] + 0.5)



Here is the example of the quantization table for luminance(Y) given in an

annex of the JPEG standard.(It is given in a form of an 8x8 block; in order to

obtain a 64 vector it should be zig-zag reordered)

 16 11 10 16 24  40  51  61

 12 12 14 19 26  58  60  55

 14 13 16 24 40  57  69  56

 14 17 22 29 51  87  80  62

 18 22 37 56 68  109 103 77

 24 35 55 64 81  104 113 92

 49 64 78 87 103 121 120 101

 72 92 95 98 112 100 103 99

This table is based upon "psychovisual thresholding" , it has "been used with

good results on 8-bit per sample luminance and chrominance images".

Most existing encoders use simple multiples of this example, but the values are

not claimed to be optimal (An encoder can use ANY OTHER quantization table)

The table is specified in the JPEG file with the DQT(Define Quantization Table)

marker.Most commonly there is one table for Y, and another one for the

chrominance (Cb and Cr).



The quantization process has the key role in the JPEG compression.

It is the process which removes the high frequencies present in the original

image -- in consequence the high detail.

We do this because of the fact that the eye is much more sensitive to lower

spatial frequencies than to higher frequencies, so we can remove, with very

little visual loss, higher frequencies.

This is done by dividing values at high indexes in the vector (the amplitudes

of higher frequencies) with larger values than the values by which are divided

the amplitudes of lower frequencies.

The bigger the values in the quantization table are, the bigger is the error

(in consequence the visual error) introduced by this lossy process, and the

smaller is the visual quality.



Another important fact is that in most images the colour varies slow from one

pixel to another, so most images will have a small quantity of high detail

-> a small amount (small amplitudes) of high spatial frequencies - but they have

a lot of image information contained in the low spatial frequencies.



In consequence in the new quantized vector, at high spatial frequencies, we'll

have a lot of consecutive zeroes.





7)  The Zero Run Length Coding (RLC)

-------------------------------



Now we have the quantized vector with a lot of consecutive zeroes. We can exploit

this by run length coding the consecutive zeroes.

IMPORTANT: You'll see later why, but here we skip the encoding of the first

 coefficient of the vector (the DC coefficient) which is coded a bit different.

(I'll present its coding later on this doc)

Let's consider the original 64 vector a 63 vector (it's the 64 vector without

the first coefficient)





Say that we have 57,45,0,0,0,0,23,0,-30,-16,0,0,1,0,0,0, 0 , 0 ,0 , only 0,..,0



Here it is how the RLC JPEG compression is done for this example :



(0,57) ; (0,45) ; (4,23) ; (1,-30) ; (0,-16) ; (2,1) ; EOB



As you see, we encode for each value different by 0 the number of consecutive

zeroes PRECEDING that value, then we add the value.

Another note : EOB is the short form for End of Block, it's a special coded

value (a marker). If we've reached in a position in the vector from which

we have till the end of the vector only zeroes, we'll mark that position

with EOB and finish the RLC compression of the quantized vector.





[Note that if the quantized vector doesn't finishes with zeroes (has the last

element not 0) we'll not have the EOB marker.]



ACTUALLY, EOB has as an equivalent (0,0) and it will be (later) Huffman coded

like (0,0), so we'll encode :

 (0,57) ; (0,45) ; (4,23) ; (1,-30) ; (0,-16) ; (2,1) ; (0,0)





Another MAJOR thing: Say that somewhere in the quantized vector

 we have: 57, eighteeen zeroes, 3, 0,0 ,0,0 2, thirty-three zeroes, 895, EOB



The JPG Huffman coding makes the restriction (you'll see later why) that

the number of previous 0's to be coded as a 4-bit value, so it can't overpass

the value 15 (0xF).



So, the previous example would be coded as :

    (0,57) ; (15,0) (2,3) ; (4,2) ; (15,0) (15,0) (1,895) , (0,0)



(15,0) is a special coded value which indicates that there follows 16 consecutive

zeroes.Note : 16 zeroes not 15 zeroes.



8) The final step === Huffman coding

-------------------------------------



First an IMPORTANT note : Instead of storing the actual value , the JPEG standard

specifies that we store the minimum size in bits in which we can keep that value

(it's called the category of that value) and then a bit-coded representation

of that value like this:



         Values             Category        Bits for the value

        0                   0                   -

          -1,1                  1                  0,1

       -3,-2,2,3                2              00,01,10,11

     -7,-6,-5,-4,4,5,6,7            3    000,001,010,011,100,101,110,111

       -15,..,-8,8,..,15            4       0000,..,0111,1000,..,1111

      -31,..,-16,16,..,31           5     00000,..,01111,10000,..,11111

      -63,..,-32,32,..,63           6                   .

     -127,..,-64,64,..,127          7                   .

    -255,..,-128,128,..,255         8                   .

    -511,..,-256,256,..,511         9                   .

   -1023,..,-512,512,..,1023       10                   .

  -2047,..,-1024,1024,..,2047      11                   .

  -4095,..,-2048,2048,..,4095      12                   .

  -8191,..,-4096,4096,..,8191      13                   .

 -16383,..,-8192,8192,..,16383     14                   .

-32767,..,-16384,16384,..,32767    15                   .





In consequence for the previous example:

    (0,57) ; (0,45) ; (4,23) ; (1,-30) ; (0,-8) ; (2,1) ; (0,0)



let's encode ONLY the right value of these pairs, except the pairs that are

special markers like (0,0) or (if we would have) (15,0)





    57 is in the category 6 and it is bit-coded 111001 , so we'll encode it

like (6,111001)

    45 , similar, will be coded as (6,101101)

    23  ->  (5,10111)

   -30  ->  (5,00001)

    -8  ->  (4,0111)

     1  ->  (1,1)



And now , we'll write again the string of pairs:



   (0,6), 111001 ; (0,6), 101101 ; (4,5), 10111; (1,5), 00001; (0,4) , 0111 ;

       (2,1), 1 ; (0,0)



The pairs of 2 values enclosed in bracket paranthesis, can be represented on a

byte because of the fact that each of the 2 values can be represented on a nibble

(the counter of previous zeroes is always smaller than 15 and so it is the

category of the numbers [numbers encoded in a JPG file are in range -32767..32767]).

In this byte, the high nibble represents the number of previous 0s, and the

lower nibble is the category of the new value different by 0.



The FINAL step of the encoding consists in Huffman encoding this byte, and then

writing in the JPG file, as a stream of bits, the Huffman code of this byte,

followed by the remaining bit-representation of that number.



For example, let's say that for byte 6 ( the equivalent of (0,6) ) we have a

Huffman code = 111000;

    for byte 69 = (4,5) (for example) we have 1111111110011001

             21 = (1,5)    ---  11111110110

             4  = (0,4)    ---  1011

             33 = (2,1)    ---  11011

              0 = EOB = (0,0) ---  1010



The final stream of bits written in the JPG file on disk for the previous example

of 63 coefficients (remember that we've skipped the first coefficient ) is

      111000 111001  111000 101101  1111111110011001 10111   11111110110 00001

         1011 0111   11011 1   1010





The encoding of the DC coefficient

-----------------------------------

DC is the coefficient in the quantized vector corresponding to the lowest

frequency in the image (it's the 0 frequency) , and (before quantization) is

mathematically = (the sum of 8x8 image samples) / 8 .

(It's like an average value for that block of image samples).

It is said that it contains a lot of energy present in the original 8x8 image

block. (Usually it gets large values).

The authors of the JPEG standard noticed that there's a very close connection

between the DC coefficient of consecutive blocks, so they've decided to encode

in the JPG file the difference between the DCs of consecutive 8x8 blocks

(Note: consecutive 8x8 blocks of the SAME image component, like consecutive

8x8 blocks for Y , or consecutive blocks for Cb , or for Cr)



Diff = DC  - DC

         i     (i-1)

So DC of the current block (DC  ) will be equal to :  DC  = DC    + Diff

                              i                         i     i-1



And in JPG decoding you will start from 0 -- you consider that the first

DC coefficient = 0 ;  DC  = 0

                        0

And then you'll add to the current value the value decoded from the JPG

(the Diff value)



SO, in the JPG file , the first coefficient = the DC coefficient is actually

the difference, and it is Huffman encoded DIFFERENTLY from the encoding of AC coefficients.



Here it is how it's done:

(Remember that we now code the Diff value)



Diff corresponds as you've seen before to a representation made by category and

it's bit coded representation.

In the JPG file it will be Huffman encoded only the category value, like this:



Diff = (category, bit-coded representation)

Then Diff will be coded as (Huffman_code(category) , bit-coded representation)



For example, if Diff is equal to -511 , then Diff  corresponds to

                    (9, 000000000)

Say that 9 has a Huffman code = 1111110

(In the JPG file, there are 2 Huffman tables for an image component: one for DC

and one for AC)



In the JPG file, the bits corresponding to the DC coefficient will be:

           1111110 000000000

And,applied to this example of DC and to the previous example of ACs, for this

vector with 64 coefficients, THE FINAL STREAM OF BITS written in the JPG file

will be:



   1111110 000000000 111000 111001  111000 101101  1111111110011001 10111

       11111110110 00001 1011 0111   11011 1   1010



(In the JPG file , first it's encoded DC then ACs)





THE HUFFMAN DECODER (A brief summary) for the 64 coefficients (A Data Unit)

of an image component (For example Y)

-------------------------------------------------------------



So when you decode a stream of bits from the image in the JPG file, you'll do:



Init DC with 0.



1) First the DC coefficient decode :

     a) Fetch a valid Huffman code (you check if it exists in the Huffman

                                           DC table)

         b) See at what category this Huffman code corresponds

         c) Fetch N = category bits  , and determine what value is represented

           by (category, the N bits fetched) = Diff

         d) DC + = Diff

         e) write DC in the 64 vector :      " vector[0]=DC "



2) The 63 AC coefficients decode :



------- FOR every AC coefficient UNTIL (EOB_encountered OR AC_counter=64)



       a) Fetch a valid Huffman code (check in the AC Huffman table)

       b) Decode that Huffman code : The Huffman code corresponds to

                   (nr_of_previous_0,category)

[Remember: EOB_encountered = TRUE if (nr_of_previous_0,category) = (0,0) ]



       c) Fetch N = category bits, and determine what value is represented by

              (category,the N bits fetched) = AC_coefficient

       d) Write in the 64 vector, a number of zeroes = nr_of_previous_zero

       e) increment the AC_counter with nr_of_previous_0

       f) Write AC_coefficient in the vector:

                  " vector[AC_counter]=AC_coefficient "

-----------------



Next Steps

-----------

So, now we have a 64 elements vector.We'll do the reverse of the steps presented

in this doc:



1) Dequantize the 64 vector : "for (i=0;i<=63;i++) vector[i]*=quant[i]"

2) Re-order from zig-zag the 64 vector into an 8x8 block

3) Apply the Inverse DCT transform to the 8x8 block



Repeat the upper process [ Huffman decoder, steps 1), 2) and 3)]  for every

8x8 block of every image component (Y,Cb,Cr).



4) Up-sample if it's needed

5) Level shift samples (add 128 to the all 8-bit signed values in the 8x8 blocks

resulting from the IDCT transform)

6) Tranform YCbCr to RGB



7--- And VOILA ... the JPG image





The JPEG markers and/or how it's organized the image information in the JPG file

(The Byte level)

--------------------------------------------------------------------------------

NOTE: The JPEG/JFIF file format uses Motorola format for words, NOT Intel format,

i.e. : high byte first, low byte last -- (ex: the word FFA0 will be written in

the JPEG file in the order : FF at the low offset , A0 at the higher offset)



The JPG standard specifies that the JPEG file is composed mostly of pieces called

"segments".

A segment is a stream of bytes with length <= 65535.The segment beginning is

specified with a marker.

A marker = 2 bytes beginning with 0xFF ( the C hexadecimal notation for 255),

and ending with a byte different by 0 and 0xFF.

Ex: 'FFDA' , 'FFC4', 'FFC0'.

Each marker has a meaning: the second byte (different by 0 and 0xFF) specifies

what does that marker.

For example, there is a marker which specifies that you should start the decoding

process , this is called (the JPG standard's terminology):

        SOS=Start Of Scan = 'FFDA'



Another marker called DQT = Define Quantization Table = 0xFFDB does what this

name says: specifies that in the JPG file, after the marker (and after 3 bytes,

more on this later) it will follow 64 bytes = the coefficients of the quantization

table.



If, during the processing of the JPG file, you encounter an 0xFF, then again a

a byte different by 0 (I've told you that the second byte for a marker is not 0)

and this byte has no marker meaning (you cannot find a marker corresponding to

that byte) then the 0xFF byte you encountered must be ignored and skipped.

(In some JPGS, sequences of consecutive 0xFF are for some filling purposes and

must be skipped)



You see that whenever you encounter 0xFF , you check the next byte and see if

that 0xFF you encountered has a marker meaning or must be skipped.

What happens if we actually need to encode the 0xFF byte in the JPG file

as an *usual* byte (not a marker, or a filling byte) ?

(Say that we need to write a Huffman code which begins with 11111111 (8 bits of

1) at a byte alignment)

The standard says that we simply make the next byte 0 , and write the sequence

'FF00' in the JPG file.

So when your JPG decoder meets the 2 byte 'FF00' sequence, it should consider

just a byte: 0xFF as an usual byte.



Another thing: You realise that these markers are byte aligned in the JPG file.

What happens if during your Huffman encoding and inserting bits in the JPG file's

bytes you have not finished to insert bits in a byte, but you need to write a

marker which is byte aligned ?

For the byte alignment of the markers, you SET THE REMAINING BITS UNTIL THE

BEGINNING OF THE NEXT BYTE TO 1, then you write the marker at the next byte.



A short explanation of some important markers found in a JPG file.

-------------------------------------------------------------------



SOI = Start Of Image = 'FFD8'

 This marker must be present in any JPG file *once* at the beginning of the file.

(Any JPG file starts with the sequence FFD8.)

EOI = End Of Image = 'FFD9'

  Similar to EOI: any JPG file ends with FFD9.



RSTi = FFDi (where i is in range 0..7)  [ RST0 = FFD0, RST7=FFD7]

     = Restart Markers

These restart markers are used for resync. At regular intervals, they appear

in the JPG stream of bytes, during the decoding process (after SOS)

(They appear in the order: RST0 -- interval -- RST1 -- interval -- RST2 --...

                      ...-- RST6 -- interval -- RST7 -- interval -- RST0 --...

)

(Obs: A lot of JPGs don't have restart markers)



The problem with these markers is that they interrupt the normal bit order in

the JPG's Huffman encoded bitstream.

Remember that for the byte alignment of the markers the remaining bits are set

to 1, so your decoder has to skip at regular intervals the useless filling

bits (those set with 1) and the RST markers.



-------

Markers...

-------

At the end of this doc, I've included a very well written technical explanation

of the JPEG/JFIF file format, written by Oliver Fromme, the author of the QPEG

viewer.

There you'll find a pretty good and complete definition for the markers.



But, anyway, here is a list of markers you should check:



SOF0 = Start Of Frame 0 = FFC0

SOS  = Start Of Scan    = FFDA

APP0 = it's the marker used to identify a JPG file which uses the JFIF

    specification       = FFE0

COM  = Comment          = FFFE

DNL  = Define Number of Lines    = FFDC

DRI  = Define Restart Interval   = FFDD

DQT  = Define Quantization Table = FFDB

DHT  = Define Huffman Table      = FFC4



The Huffman table stored in a JPG file

---------------------------------------

Here it is how JPEG implements the Huffman tree: instead of a tree, it defines

a table in the JPG file after the DHT (Define Huffman Table) marker.

NOTE: The length of the Huffman codes is restricted to 16 bits.



Basically there are 2 types of Huffman tables in a JPG file : one for DC and

one for AC (actually there are 4 Huffman tables: 2 for DC,AC of luminance

       and 2 for DC,AC of chrominance)



They are stored in the JPG file in the same format which consist of:

1) 16 bytes :



byte i contains the number of Huffman codes of length i (length in bits)

 i ranges from 1 to 16

                                         16

2) A table with the length (in bytes) =  sum nr_codes_of_length_i

                                         i=1



which contains at location [k][j]  (k in 1..16, j in 0..(nr_codes_with_length_k-1))

the BYTE value associated to the j-th Huffman code of length k.

(For a fixed length k, the values are stored sorted by the value of the Huffman

code)



From this table you can find the actual Huffman code associated to a particular

byte.

Here it is an example of how the actual code values are generated:



Ex:  (Note: The number of codes for a given length are here for this particular

      example to figure it out, they can have any other values)

SAY that,



         For length 1 we have nr_codes[1]=0, we skip this length

         For length 2 we have 2 codes  00

                                       01

         For length 3 we have 3 codes  100

                                       101

                                       110

         For length  4 we have 1 code  1110

         For length  5 we have 1 code  11110

         For length  6 we have 1 code  111110

         For length  7 we have 0 codes  -- skip

 (if we had 1 code for length 7,

          we would have                1111110)

         For length  8 we have 1 code  11111100 (You see that the code is still

                                                 shifted to left though we skipped

                                                 the code value for 7)

         .....

         For length 16, .... (the same thing)



I've told you that in the Huffman table in the JPG file are stored the BYTE values

for a given code.



For this particular example of Huffman codes:

Say that in the Huffman table in the JPG file on disk we have (after that 16 bytes

which contains the nr of Huffman codes with a given length):

    45 57 29 17 23 25 34 28

These values corressponds , given that particular lengths I gave you before ,

to the Huffman codes like this :



    there's no value for  code of length 1

    for codes of length 2 : we have 45 57

    for codes of length 3 : 3 values (ex : 29,17,23)

    for codes of length 4 : only 1 value (ex: 25)

    for codes of length 5 : 1 value ( ex: 34)

    ..

    for code of length 7, again no value, skip to code with length 8

    for code of length 8 : 1 value 28



IMPORTANT note:

  For codes of length 2:

      the value 45 corresponds to code 00

                57             to code 01

  For codes of length 3:

      the value 29 corresponds to code  100

                17       ----||---      101

                23       ----||---      110



  ETC...

(I've told you that for a given length the byte values are stored in the order

of increasing the value of the Huffman code.)



Four Huffman tables corresponding to DC and AC tables of the luminance, and

DC and AC tables for the chrominance, are given in an annex of the JPEG

standard as a suggestion for the encoder.

 The standard says that these tables have been tested with good compression

results on a lot of images and reccommends them, but the encoder can use any

other Huffman table. A lot of JPG encoders use these tables. Some of them offer

you an option: entropy optimization - if it's enabled they'll use Huffman

tables optimized for that particular image.



The JFIF (Jpeg Format Interchange File) file

---------------------------------------------

    The JPEG standard (that in the itu-1150.ps file) is somehow very general,

the JFIF implementation is a particular case of this standard (and it is, of course,

compatible with the standard) .

      The JPEG standard specifies some markers reserved for applications

(by applications I mean particular cases of implementing the standard)

 Those markers are called APPn , where n ranges from 0 to 0xF ; APPn = FFEn

 The JFIF specification uses the APP0 marker (FFE0) to identify a JPG file which

uses this specification.

 You'll see in the JPEG standard that it refers to "image components".

These image components can be (Y,Cb,Cr) or (YIQ) or whatever.

 The JFIF implementations uses only (Y,Cb,Cr) for a truecolor JPG, or only Y for

a monochrome JPG.

 You can get the JFIF specification from www.jpeg.org



The sampling factors

--------------------



Note: The following explanation covers the encoding of truecolor (3 components)

JPGS; for gray-scaled JPGs there is one component (Y) which is usually no

down-sampled at all, and does not require any inverse transformation like the

inverse (Y,Cb,Cr) -> (R,G,B). In consequence, the gray-scaled JPGS are the

simplest and easiest to decode: for every 8x8 block in the image you do the

Huffman decoding of the RLC coded vector then you reorder it from zig-zag,

dequantize the 64 vector and finally you apply to it the inverse DCT and add

128 (level shift) to the new 8x8 values.



I've told you that image components are sampled. Usually Y is taken every pixel,

and Cb, Cr are taken for a block of 2x2 pixels.

But there are some JPGs in which  Cb , Cr are taken in every pixel, or some

JPGs where Cb, Cr are taken every 2 pixels (a horizontal sampling at 2 pixels,

and a vertical sampling in every pixel)

The sampling factors for an image component in a JPG file are defined in respect

(relative) to the highest sampling factor.



Here are the sampling factors for the most usual example:

         Y is taken every pixel , and Cb,Cr are taken for a block of 2x2 pixels

(The JFIF specification gives a formula for sampling factors which I think that

works only when the maximum sampling factor for each dimension X or Y is <=2)

The JPEG standard does not specify the sampling factors , it's more general).



You see that Y will have the highest sampling rate :

           Horizontal sampling factor = 2  = HY

               Vertical sampling factor   = 2  = VY

     For Cb ,  Horizontal sampling factor = 1  = HCb

           Vertical sampling factor   = 1  = VCb

     For Cr    Horizontal sampling factor = 1  = HCr

               Vertical sampling factor   = 1  = VCr

Actually this form of defining the sampling factors is quite useful.

The vector of 64 coefficients for an image component, Huffman encoded, is called

    DU = Data Unit (JPEG's standard terminology)



In the JPG file , the order of encoding Data Units is :

     1) encode Data Units for the first image component:

             for  (counter_y=1;counter_y<=VY;counter_y++)

                  for (counter_x=1;counter_x<=HY;counter_x++)

                     {  encode Data Unit for Y }



     2) encode Data Units for the second image component:

             for  (counter_y=1;counter_y<=VCb ;counter_y++)

                  for (counter_x=1;counter_x<=HCb;counter_x++)

                     {  encode Data Unit for Cb }



     3) finally, for the third component, similar:

             for  (counter_y=1;counter_y<=VCr;counter_y++)

                  for (counter_x=1;counter_x<=HCr;counter_x++)

                     {  encode Data Unit for Cr }



For the example I gave you (HY=2, VY=2 ; HCb=VCb =1, HCr,VCr=1)

here it is a figure ( I think it will clear out things for you) :

      YDU YDU    CbDU   CrDU

          YDU YDU

( YDU is a Data unit for Y , and similar CbDU a DU for Cb, CrDU = DU for Cr )

This usual combination of sampling factors is referred as 2:1:1 for both

vertical and horizontal sampling factors.

And, of course, in the JPG file the encoding order will be :

      YDU,YDU,YDU,YDU,CbDU,CrDU



You know that a DU (64 coefficients) defines a block of 8x8 values , so here

we specified the encoding order for a block of 16x16 image pixels

(An image pixel = an (Y,Cb,Cr) pixel [my notation]) :

  Four 8x8 blocks of Y values (4 YDUs), one 8x8 block of Cb values (1 CbDU)

and one 8x8 block of Cr values (1 CrDU)



(Hmax = the maximum horizontal sampling factor , Vmax = the maximum vertical

sampling factor)

In consequence for this example of sampling factors (Hmax = 2, Vmax=2), the

encoder should process SEPARATELY every 16x16 = (Hmax*8 x Vmax*8) image pixels

block in the order mentioned.



This block of image pixels with the dimensions (Hmax*8,Vmax*8) is called, in

the JPG's standard terminology, an MCU = Minimum Coded Unit

For the previous example : MCU = YDU,YDU,YDU,YDU,CbDU,CrDU



Another example of sampling factors :

      HY =1, VY=1

      HCb=1, VCb=1

      HCr=1, VCr=1

Figure/order :  YDU CbDU CrDU

You see that here is defined an 8x8 image pixel block (MCU) with 3 8x8 blocks:

     one for Y, one for Cb and one for Cr (There's no down-sampling at all)

Here (Hmax=1,Vmax=1) the MCU has the dimension (8,8), and MCU = YDU,CbDU,CrDU



For gray-scaled JPGs you don't have to worry about the order of encoding

data units in an MCU. For these JPGs, an MCU = 1 Data Unit (MCU = YDU)





In the JPG file, the sampling factors for every image component are defined

after the marker SOF0 = Start Of Frame 0 = FFC0



A brief scheme of decoding a JPG file

--------------------------------------

The decoder reads from the JPG file the sampling factors, it finds out the

dimensions of an MCU (Hmax*8,Vmax*8) => how many MCUs are in the whole image,

then decodes every MCU present in the original image (a loop for all these

blocks, or until the EOI marker is found [it should be found when the loop

finishes, otherwise you'll get an incomplete image]) - it decodes an MCU

by decoding every Data Unit in the MCU in the order mentioned before, and

finally, writes the decoded (Hmax*8 x Vmax*8) truecolor pixel block into the

(R,G,B) image buffer.





MPEG-1 video and JPEG

----------------------

The interesting part of the MPEG-1 specification (and probably MPEG-2) is that

it relies heavily on the JPEG specification.

It uses a lot of concepts presented here. The reason is that every 15 frames ,

or when it's needed, there's an independent frame called I-frame (Intra frame)

which is JPEG coded.

(By the way, that 16x16 image pixels block example I gave you, is called,in the

MPEG's standard terminology, a macroblock)

Except the algorithms for motion compensation, MPEG-1 video relies a lot on the

JPG specifications (the DCT transform , quantization, etc.)




JPEG/JFIF file format:

~~~~~~~~~~~~~~~~~~~~~~



  - header (2 bytes):  $ff, $d8 (SOI) (these two identify a JPEG/JFIF file)

  - for JFIF files, an APP0 segment is immediately following the SOI marker,

    see below

  - any number of "segments" (similar to IFF chunks), see below

  - trailer (2 bytes): $ff, $d9 (EOI)



Segment format:

~~~~~~~~~~~~~~~



  - header (4 bytes):

       $ff     identifies segment

        n      type of segment (one byte)

       sh, sl  size of the segment, including these two bytes, but not

               including the $ff and the type byte. Note, not Intel order:

               high byte first, low byte last!

  - contents of the segment, max. 65533 bytes.



 Notes:

  - There are parameterless segments (denoted with a '*' below) that DON'T

    have a size specification (and no contents), just $ff and the type byte.

  - Any number of $ff bytes between segments is legal and must be skipped.



Segment types:

~~~~~~~~~~~~~~



   *TEM   = $01   usually causes a decoding error, may be ignored



    SOF0  = $c0   Start Of Frame (baseline JPEG), for details see below

    SOF1  = $c1   dito

    SOF2  = $c2   usually unsupported

    SOF3  = $c3   usually unsupported



    SOF5  = $c5   usually unsupported

    SOF6  = $c6   usually unsupported

    SOF7  = $c7   usually unsupported



    SOF9  = $c9   for arithmetic coding, usually unsupported

    SOF10 = $ca   usually unsupported

    SOF11 = $cb   usually unsupported



    SOF13 = $cd   usually unsupported

    SOF14 = $ce   usually unsupported

    SOF14 = $ce   usually unsupported

    SOF15 = $cf   usually unsupported



    DHT   = $c4   Define Huffman Table, for details see below

    JPG   = $c8   undefined/reserved (causes decoding error)

    DAC   = $cc   Define Arithmetic Table, usually unsupported



   *RST0  = $d0   RSTn are used for resync, may be ignored

   *RST1  = $d1

   *RST2  = $d2

   *RST3  = $d3

   *RST4  = $d4

   *RST5  = $d5

   *RST6  = $d6

   *RST7  = $d7



    SOI   = $d8   Start Of Image

    EOI   = $d9   End Of Image

    SOS   = $da   Start Of Scan, for details see below

    DQT   = $db   Define Quantization Table, for details see below

    DNL   = $dc   usually unsupported, ignore



    SOI   = $d8   Start Of Image

    EOI   = $d9   End Of Image

    SOS   = $da   Start Of Scan, for details see below

    DQT   = $db   Define Quantization Table, for details see below

    DNL   = $dc   usually unsupported, ignore

    DRI   = $dd   Define Restart Interval, for details see below

    DHP   = $de   ignore (skip)

    EXP   = $df   ignore (skip)



    APP0  = $e0   JFIF APP0 segment marker, for details see below

    APP15 = $ef   ignore



    JPG0  = $f0   ignore (skip)

    JPG13 = $fd   ignore (skip)

    COM   = $fe   Comment, for details see below



 All other segment types are reserved and should be ignored (skipped).



SOF0: Start Of Frame 0:

~~~~~~~~~~~~~~~~~~~~~~~



  - $ff, $c0 (SOF0)

  - length (high byte, low byte), 8+components*3

  - data precision (1 byte) in bits/sample, usually 8 (12 and 16 not

    supported by most software)

  - image height (2 bytes, Hi-Lo), must be >0 if DNL not supported

  - image width (2 bytes, Hi-Lo), must be >0 if DNL not supported

  - number of components (1 byte), usually 1 = grey scaled, 3 = color YCbCr

    or YIQ, 4 = color CMYK)

  - for each component: 3 bytes

     - component id (1 = Y, 2 = Cb, 3 = Cr, 4 = I, 5 = Q)

     - sampling factors (bit 0-3 vert., 4-7 hor.)

     - quantization table number



 Remarks:

  - JFIF uses either 1 component (Y, greyscaled) or 3 components (YCbCr,

    sometimes called YUV, colour).



APP0: JFIF segment marker:

~~~~~~~~~~~~~~~~~~~~~~~~~~



  - $ff, $e0 (APP0)

  - length (high byte, low byte), must be >= 16

  - 'JFIF'#0 ($4a, $46, $49, $46, $00), identifies JFIF

  - major revision number, should be 1 (otherwise error)

  - minor revision number, should be 0..2 (otherwise try to decode anyway)

  - units for x/y densities:

     0 = no units, x/y-density specify the aspect ratio instead

     1 = x/y-density are dots/inch

     2 = x/y-density are dots/cm

  - x-density (high byte, low byte), should be <> 0

  - y-density (high byte, low byte), should be <> 0

  - thumbnail width (1 byte)

  - thumbnail height (1 byte)

  - n bytes for thumbnail (RGB 24 bit), n = width*height*3



 Remarks:

  - If there's no 'JFIF'#0, or the length is < 16, then it is probably not

    a JFIF segment and should be ignored.

  - Normally units=0, x-dens=1, y-dens=1, meaning that the aspect ratio is

    1:1 (evenly scaled).

  - JFIF files including thumbnails are very rare, the thumbnail can usually

    be ignored.  If there's no thumbnail, then width=0 and height=0.

  - If the length doesn't match the thumbnail size, a warning may be

    printed, then continue decoding.



DRI: Define Restart Interval:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



  - $ff, $dd (DRI)

  - length (high byte, low byte), must be = 4

  - restart interval (high byte, low byte) in units of MCU blocks,

    meaning that every n MCU blocks a RSTn marker can be found.

    The first marker will be RST0, then RST1 etc, after RST7

    repeating from RST0.



DQT: Define Quantization Table:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



  - $ff, $db (DQT)

  - length (high byte, low byte)

  - QT information (1 byte):

     bit 0..3: number of QT (0..3, otherwise error)

     bit 4..7: precision of QT, 0 = 8 bit, otherwise 16 bit

  - n bytes QT, n = 64*(precision+1)



 Remarks:

  - A single DQT segment may contain multiple QTs, each with its own

    information byte.

  - For precision=1 (16 bit), the order is high-low for each of the 64 words.



DAC: Define Arithmetic Table:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 Current software does not support arithmetic coding for legal reasons.

 JPEG files using arithmetic coding can not be processed.



DHT: Define Huffman Table:

~~~~~~~~~~~~~~~~~~~~~~~~~~



  - $ff, $c4 (DHT)

  - length (high byte, low byte)

  - HT information (1 byte):

     bit 0..3: number of HT (0..3, otherwise error)

     bit 4   : type of HT, 0 = DC table, 1 = AC table

     bit 5..7: not used, must be 0

  - 16 bytes: number of symbols with codes of length 1..16, the sum of these

    bytes is the total number of codes, which must be <= 256

  - n bytes: table containing the symbols in order of increasing code length

    (n = total number of codes)



 Remarks:

  - A single DHT segment may contain multiple HTs, each with its own

    information byte.



COM: Comment:

~~~~~~~~~~~~~



  - $ff, $fe (COM)

  - length (high byte, low byte) of the comment = L+2

  - The comment = a stream of bytes with the length = L



SOS: Start Of Scan:

~~~~~~~~~~~~~~~~~~~



  - $ff, $da (SOS)

  - length (high byte, low byte), must be 6+2*(number of components in scan)

  - number of components in scan (1 byte), must be >= 1 and <=4 (otherwise

    error), usually 1 or 3

  - for each component: 2 bytes

     - component id (1 = Y, 2 = Cb, 3 = Cr, 4 = I, 5 = Q), see SOF0

     - Huffman table to use:

    - bit 0..3: AC table (0..3)

    - bit 4..7: DC table (0..3)

  - 3 bytes to be ignored (???)



 Remarks:

  - The image data (scans) is immediately following the SOS segment.

*/
