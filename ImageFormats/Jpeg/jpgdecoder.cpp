#include <iostream>
#include <fstream>

#include "jpgDecoder.h"



/****************************************************************************************
    JPEG terminology
*****************************************************************************************

Baseline
Progressive
Huffman Coding
    Symbols
        The bit length of a coeffients in MCU
    Codes
        The lookup bits stuffed into the bitstream
    Values
        The coefficient value in the MCU

Zigzag

Inverse Quantization
Inverse DCT

Frequency
    The similarity / difference of adjacent pixels
    How homoginous the image is

Discrete Cosine Transformation
Chroma subsampling

YCrCb
    Y  - Luminace
    Cr - Red chrominance
    Cb - Blue chrominance

Quantization
    Setting the number of steps or values for a component
    ie if Quantization = 16, then divide component by 16
    Separate divisors for every element of each MCU channel
    Different quantization talbes for different jpg qualities ie 50 vs 100

Chillitomatonoodle - youtuber c++
OneLoneCoder
Color Channel
    A part of a color space
    ie RGB is three color channels: Red, Green and Blue channels
       YCrCb same thing

Component
    The value of color channel of a pixel ie R in RGB

MCU Minimum Coded Unit
    8x8 pixels by component
    Coefficient
        The value of each elemennt of the MCU


*/

/****************************************************************************************
    BitReader Class
    helper class to read bits from a file
****************************************************************************************/

BitReader::BitReader(const std::string& filename) {
    inFile.open(filename, std::ios::in | std::ios::binary);
    // QFile file(QString::fromStdString(filename));
    // if (file.open(QIODevice::ReadOnly)) {
    //     QByteArray data = file.readAll();
    //     buffer.setData(data);
    //     buffer.open(QIODevice::ReadOnly);
    //     nextBit = 0;
    // } else {
    //     std::cerr << "Error - Unable to open file: " << filename << '\n';
    // }
}

BitReader::~BitReader()
{
    // if (inFile.is_open()) {
    //     inFile.close();
    // }
    if (buffer.isOpen()) {
        buffer.close();
    }
}

bool BitReader::hasBits()
{
    return !!inFile;
    // return buffer.bytesAvailable() > 0;
}

quint8 BitReader::readByte()
{
    nextBit = 0;
    return (quint8)inFile.get();
    // return static_cast<quint8>(buffer.getChar());
}

uint BitReader::readWord() {
    nextBit = 0;
    return (inFile.get() << 8) + inFile.get();
    // char highByte, lowByte;
    // buffer.getChar(&highByte);
    // buffer.getChar(&lowByte);
    // return (static_cast<quint8>(highByte) << 8) + static_cast<quint8>(lowByte);
}

// read one bit (0 or 1) or return -1 if all bits have already been read
uint BitReader::readBit()
{
    if (nextBit == 0) {
        if (!hasBits()) {
            return -1;
        }
        nextByte = inFile.get();
        // buffer.getChar(reinterpret_cast<char*>(&nextByte));
        while (nextByte == 0xFF) {
            if (!hasBits()) {
                return -1;
            }
            quint8 marker = inFile.peek();
            // char marker = static_cast<char>(buffer.peek(buffer.pos()));
            // ignore multiple 0xFF's in a row
            while (marker == 0xFF) {
                inFile.get();
                if (!hasBits()) {
                    return -1;
                }
                marker = inFile.peek();
            }
            // literal 0xFF's are encoded in the bitstream as 0xFF00
            if (marker == 0x00) {
                inFile.get();
                break;
            }
            // restart marker
            else if (marker >= RST0 && marker <= RST7) {
                inFile.get();
                nextByte = inFile.get();
            }
            else {
                std::cout << "Error - Invalid marker: 0x" << std::hex << (uint)marker << std::dec << '\n';
                return -1;
            }
        }
    }
    uint bit = (nextByte >> (7 - nextBit)) & 1;
    nextBit = (nextBit + 1) % 8;
    return bit;
}

// read a variable number of bits
// first read bit is most significant bit
// return -1 if at any point all bits have already been read
uint BitReader::readBits(const uint length)
{
    uint bits = 0;
    for (uint i = 0; i < length; ++i) {
        uint bit = readBit();
        if (bit == -1) {
            bits = -1;
            break;
        }
        bits = (bits << 1) | bit;
    }
    return bits;
}

// advance to the 0th bit of the next byte
void BitReader::align()
{
    nextBit = 0;
}



// SOF specifies frame type, dimensions, and number of color components
void JpgDecoder::readStartOfFrame(BitReader& bitReader, JPGImage* const image) {
    if (isDebug) std::cout << "Reading SOF Marker\n";
    if (image->numComponents != 0) {
        std::cout << "Error - Multiple SOFs detected\n";
        image->valid = false;
        return;
    }

    uint length = bitReader.readWord();

    quint8 precision = bitReader.readByte();
    if (precision != 8) {
        std::cout << "Error - Invalid precision: " << (uint)precision << '\n';
        image->valid = false;
        return;
    }

    image->height = bitReader.readWord();
    image->width = bitReader.readWord();
    if (image->height == 0 || image->width == 0) {
        std::cout << "Error - Invalid dimensions\n";
        image->valid = false;
        return;
    }
    image->blockHeight = (image->height + 7) / 8;
    image->blockWidth = (image->width + 7) / 8;
    image->blockHeightReal = image->blockHeight;
    image->blockWidthReal = image->blockWidth;

    image->numComponents = bitReader.readByte();
    if (image->numComponents == 4) {
        std::cout << "Error - CMYK color mode not supported\n";
        image->valid = false;
        return;
    }
    if (image->numComponents != 1 && image->numComponents != 3) {
        std::cout << "Error - " << (uint)image->numComponents << " color components given (1 or 3 required)\n";
        image->valid = false;
        return;
    }
    for (uint i = 0; i < image->numComponents; ++i) {
        quint8 componentID = bitReader.readByte();
        // component IDs are usually 1, 2, 3 but rarely can be seen as 0, 1, 2
        // always force them into 1, 2, 3 for consistency
        if (componentID == 0 && i == 0) {
            image->zeroBased = true;
        }
        if (image->zeroBased) {
            componentID += 1;
        }
        if (componentID == 0 || componentID > image->numComponents) {
            std::cout << "Error - Invalid component ID: " << (uint)componentID << '\n';
            image->valid = false;
            return;
        }
        ColorComponent& component = image->colorComponents[componentID - 1];
        if (component.usedInFrame) {
            std::cout << "Error - Duplicate color component ID: " << (uint)componentID << '\n';
            image->valid = false;
            return;
        }
        component.usedInFrame = true;

        quint8 samplingFactor = bitReader.readByte();
        component.horizontalSamplingFactor = samplingFactor >> 4;
        component.verticalSamplingFactor = samplingFactor & 0x0F;
        if (componentID == 1) {
            if ((component.horizontalSamplingFactor != 1 && component.horizontalSamplingFactor != 2) ||
                (component.verticalSamplingFactor != 1 && component.verticalSamplingFactor != 2)) {
                std::cout << "Error - Sampling factors not supported\n";
                image->valid = false;
                return;
            }
            if (component.horizontalSamplingFactor == 2 && image->blockWidth % 2 == 1) {
                image->blockWidthReal += 1;
            }
            if (component.verticalSamplingFactor == 2 && image->blockHeight % 2 == 1) {
                image->blockHeightReal += 1;
            }
            image->horizontalSamplingFactor = component.horizontalSamplingFactor;
            image->verticalSamplingFactor = component.verticalSamplingFactor;
        }
        else {
            if (component.horizontalSamplingFactor != 1 || component.verticalSamplingFactor != 1) {
                std::cout << "Error - Sampling factors not supported\n";
                image->valid = false;
                return;
            }
        }

        component.quantizationTableID = bitReader.readByte();
        if (component.quantizationTableID > 3) {
            std::cout << "Error - Invalid quantization table ID: " << (uint)component.quantizationTableID << '\n';
            image->valid = false;
            return;
        }
    }

    if (length - 8 - (3 * image->numComponents) != 0) {
        std::cout << "Error - SOF invalid\n";
        image->valid = false;
        return;
    }
}

/****************************************************************************************
    JpgDecoder Class
****************************************************************************************/

JpgDecoder::JpgDecoder() {}

// DQT contains one or more quantization tables
void JpgDecoder::readQuantizationTable(BitReader& bitReader, JPGImage* const image)
{
    if (isDebug) std::cout << "Reading DQT Marker\n";
    int length = bitReader.readWord();
    length -= 2;

    while (length > 0) {
        quint8 tableInfo = bitReader.readByte();
        length -= 1;
        quint8 tableID = tableInfo & 0x0F;

        if (tableID > 3) {
            std::cout << "Error - Invalid quantization table ID: " << (uint)tableID << '\n';
            image->valid = false;
            return;
        }
        QuantizationTable& qTable = image->quantizationTables[tableID];
        qTable.set = true;

        if (tableInfo >> 4 != 0) {
            for (uint i = 0; i < 64; ++i) {
                qTable.table[zigZagMap[i]] = bitReader.readWord();
            }
            length -= 128;
        }
        else {
            for (uint i = 0; i < 64; ++i) {
                qTable.table[zigZagMap[i]] = bitReader.readByte();
            }
            length -= 64;
        }
    }

    if (length != 0) {
        std::cout << "Error - DQT invalid\n";
        image->valid = false;
        return;
    }
}

// generate all Huffman codes based on symbols from a Huffman table
void JpgDecoder::generateCodes(HuffmanTable& hTable)
{
    uint code = 0;
    for (uint i = 0; i < 16; ++i) {
        for (uint j = hTable.offsets[i]; j < hTable.offsets[i + 1]; ++j) {
            hTable.codes[j] = code;
            code += 1;
        }
        code <<= 1;
    }
}

// DHT contains one or more Huffman tables
void JpgDecoder::readHuffmanTable(BitReader& bitReader, JPGImage* const image)
{
    if (isDebug) std::cout << "Reading DHT Marker\n";
    int length = bitReader.readWord();
    length -= 2;

    while (length > 0) {
        quint8 tableInfo = bitReader.readByte();
        quint8 tableID = tableInfo & 0x0F;
        bool acTable = tableInfo >> 4;

        if (tableID > 3) {
            std::cout << "Error - Invalid Huffman table ID: " << (uint)tableID << '\n';
            image->valid = false;
            return;
        }

        HuffmanTable& hTable = (acTable) ?
                                   (image->huffmanACTables[tableID]) :
                                   (image->huffmanDCTables[tableID]);
        hTable.set = true;

        hTable.offsets[0] = 0;
        uint allSymbols = 0;
        for (uint i = 1; i <= 16; ++i) {
            allSymbols += bitReader.readByte();
            hTable.offsets[i] = allSymbols;
        }
        if (allSymbols > 176) {
            std::cout << "Error - Too many symbols in Huffman table: " << allSymbols << '\n';
            image->valid = false;
            return;
        }

        for (uint i = 0; i < allSymbols; ++i) {
            hTable.symbols[i] = bitReader.readByte();
        }

        generateCodes(hTable);

        length -= 17 + allSymbols;
    }

    if (length != 0) {
        std::cout << "Error - DHT invalid\n";
        image->valid = false;
        return;
    }
}

// SOS contains color component info for the next scan
void JpgDecoder::readStartOfScan(BitReader& bitReader, JPGImage* const image)
{
    if (isDebug) std::cout << "Reading SOS Marker\n";
    if (image->numComponents == 0) {
        std::cout << "Error - SOS detected before SOF\n";
        image->valid = false;
        return;
    }

    uint length = bitReader.readWord();

    for (uint i = 0; i < image->numComponents; ++i) {
        image->colorComponents[i].usedInScan = false;
    }

    // the number of components in the next scan might not be all
    //   components in the image
    image->componentsInScan = bitReader.readByte();
    if (image->componentsInScan == 0) {
        std::cout << "Error - Scan must include at least 1 component\n";
        image->valid = false;
        return;
    }
    for (uint i = 0; i < image->componentsInScan; ++i) {
        quint8 componentID = bitReader.readByte();
        // component IDs are usually 1, 2, 3 but rarely can be seen as 0, 1, 2
        if (image->zeroBased) {
            componentID += 1;
        }
        if (componentID == 0 || componentID > image->numComponents) {
            std::cout << "Error - Invalid color component ID: " << (uint)componentID << '\n';
            image->valid = false;
            return;
        }
        ColorComponent& component = image->colorComponents[componentID - 1];
        if (!component.usedInFrame) {
            std::cout << "Error - Invalid color component ID: " << (uint)componentID << '\n';
            image->valid = false;
            return;
        }
        if (component.usedInScan) {
            std::cout << "Error - Duplicate color component ID: " << (uint)componentID << '\n';
            image->valid = false;
            return;
        }
        component.usedInScan = true;

        quint8 huffmanTableIDs = bitReader.readByte();
        component.huffmanDCTableID = huffmanTableIDs >> 4;
        component.huffmanACTableID = huffmanTableIDs & 0x0F;
        if (component.huffmanDCTableID > 3) {
            std::cout << "Error - Invalid Huffman DC table ID: " << (uint)component.huffmanDCTableID << '\n';
            image->valid = false;
            return;
        }
        if (component.huffmanACTableID > 3) {
            std::cout << "Error - Invalid Huffman AC table ID: " << (uint)component.huffmanACTableID << '\n';
            image->valid = false;
            return;
        }
    }

    image->startOfSelection = bitReader.readByte();
    image->endOfSelection = bitReader.readByte();
    quint8 successiveApproximation = bitReader.readByte();
    image->successiveApproximationHigh = successiveApproximation >> 4;
    image->successiveApproximationLow = successiveApproximation & 0x0F;

    if (image->frameType == SOF0) {
        // Baseline JPGs don't use spectral selection or successive approximtion
        if (image->startOfSelection != 0 || image->endOfSelection != 63) {
            std::cout << "Error - Invalid spectral selection\n";
            image->valid = false;
            return;
        }
        if (image->successiveApproximationHigh != 0 || image->successiveApproximationLow != 0) {
            std::cout << "Error - Invalid successive approximation\n";
            image->valid = false;
            return;
        }
    }
    else if (image->frameType == SOF2) {
        if (image->startOfSelection > image->endOfSelection) {
            std::cout << "Error - Invalid spectral selection (start greater than end)\n";
            image->valid = false;
            return;
        }
        if (image->endOfSelection > 63) {
            std::cout << "Error - Invalid spectral selection (end greater than 63)\n";
            image->valid = false;
            return;
        }
        if (image->startOfSelection == 0 && image->endOfSelection != 0) {
            std::cout << "Error - Invalid spectral selection (contains DC and AC)\n";
            image->valid = false;
            return;
        }
        if (image->startOfSelection != 0 && image->componentsInScan != 1) {
            std::cout << "Error - Invalid spectral selection (AC scan contains multiple components)\n";
            image->valid = false;
            return;
        }
        if (image->successiveApproximationHigh != 0 &&
            image->successiveApproximationLow != image->successiveApproximationHigh - 1) {
            std::cout << "Error - Invalid successive approximation\n";
            image->valid = false;
            return;
        }
    }

    for (uint i = 0; i < image->numComponents; ++i) {
        const ColorComponent& component = image->colorComponents[i];
        if (image->colorComponents[i].usedInScan) {
            if (image->quantizationTables[component.quantizationTableID].set == false) {
                std::cout << "Error - Color component using uninitialized quantization table\n";
                image->valid = false;
                return;
            }
            if (image->startOfSelection == 0) {
                if (image->huffmanDCTables[component.huffmanDCTableID].set == false) {
                    std::cout << "Error - Color component using uninitialized Huffman DC table\n";
                    image->valid = false;
                    return;
                }
            }
            if (image->endOfSelection > 0) {
                if (image->huffmanACTables[component.huffmanACTableID].set == false) {
                    std::cout << "Error - Color component using uninitialized Huffman AC table\n";
                    image->valid = false;
                    return;
                }
            }
        }
    }

    if (length - 6 - (2 * image->componentsInScan) != 0) {
        std::cout << "Error - SOS invalid\n";
        image->valid = false;
        return;
    }
}

// restart interval is needed to stay synchronized during data scans
void JpgDecoder::readRestartInterval(BitReader& bitReader, JPGImage* const image)
{
    if (isDebug) std::cout << "Reading DRI Marker\n";
    uint length = bitReader.readWord();

    image->restartInterval = bitReader.readWord();
    if (length - 4 != 0) {
        std::cout << "Error - DRI invalid\n";
        image->valid = false;
        return;
    }
}

// APPNs simply get skipped based on length
void JpgDecoder::readAPPN(BitReader& bitReader, JPGImage* const image)
{
    if (isDebug) std::cout << "Reading APPN Marker\n";
    uint length = bitReader.readWord();
    if (length < 2) {
        std::cout << "Error - APPN invalid\n";
        image->valid = false;
        return;
    }

    for (uint i = 0; i < length - 2; ++i) {
        bitReader.readByte();
    }
}

// comments simply get skipped based on length
void JpgDecoder::readComment(BitReader& bitReader, JPGImage* const image)
{
    if (isDebug) std::cout << "Reading COM Marker\n";
    uint length = bitReader.readWord();
    if (length < 2) {
        std::cout << "Error - COM invalid\n";
        image->valid = false;
        return;
    }

    for (uint i = 0; i < length - 2; ++i) {
        bitReader.readByte();
    }
}

// print all info extracted from the JPG file
void JpgDecoder::printFrameInfo(const JPGImage* const image)
{
    if (image == nullptr) return;
    std::cout << "SOF=============\n";
    std::cout << "Frame Type: 0x" << std::hex << (uint)image->frameType << std::dec << '\n';
    std::cout << "Height: " << image->height << '\n';
    std::cout << "Width: " << image->width << '\n';
    std::cout << "Color Components:\n";
    for (uint i = 0; i < image->numComponents; ++i) {
        if (image->colorComponents[i].usedInFrame) {
            std::cout << "Component ID: " << (i + 1) << '\n';
            std::cout << "Horizontal Sampling Factor: " << (uint)image->colorComponents[i].horizontalSamplingFactor << '\n';
            std::cout << "Vertical Sampling Factor: " << (uint)image->colorComponents[i].verticalSamplingFactor << '\n';
            std::cout << "Quantization Table ID: " << (uint)image->colorComponents[i].quantizationTableID << '\n';
        }
    }
    std::cout << "DQT=============\n";
    for (uint i = 0; i < 4; ++i) {
        if (image->quantizationTables[i].set) {
            std::cout << "Table ID: " << i << '\n';
            std::cout << "Table Data:";
            for (uint j = 0; j < 64; ++j) {
                if (j % 8 == 0) {
                    std::cout << '\n';
                }
                std::cout << image->quantizationTables[i].table[j] << ' ';
            }
            std::cout << '\n';
        }
    }
}

// print info for the next scan
void JpgDecoder::printScanInfo(const JPGImage* const image)
{
    if (image == nullptr) return;
    std::cout << "SOS=============\n";
    std::cout << "Start of Selection: " << (uint)image->startOfSelection << '\n';
    std::cout << "End of Selection: " << (uint)image->endOfSelection << '\n';
    std::cout << "Successive Approximation High: " << (uint)image->successiveApproximationHigh << '\n';
    std::cout << "Successive Approximation Low: " << (uint)image->successiveApproximationLow << '\n';
    std::cout << "Color Components:\n";
    for (uint i = 0; i < image->numComponents; ++i) {
        if (image->colorComponents[i].usedInScan) {
            std::cout << "Component ID: " << (i + 1) << '\n';
            std::cout << "Huffman DC Table ID: " << (uint)image->colorComponents[i].huffmanDCTableID << '\n';
            std::cout << "Huffman AC Table ID: " << (uint)image->colorComponents[i].huffmanACTableID << '\n';
        }
    }
    std::cout << "DHT=============\n";
    std::cout << "DC Tables:\n";
    for (uint i = 0; i < 4; ++i) {
        if (image->huffmanDCTables[i].set) {
            std::cout << "Table ID: " << i << '\n';
            std::cout << "Symbols:\n";
            for (uint j = 0; j < 16; ++j) {
                std::cout << (j + 1) << ": ";
                for (uint k = image->huffmanDCTables[i].offsets[j]; k < image->huffmanDCTables[i].offsets[j + 1]; ++k) {
                    std::cout << std::hex << (uint)image->huffmanDCTables[i].symbols[k] << std::dec << ' ';
                }
                std::cout << '\n';
            }
        }
    }
    std::cout << "AC Tables:\n";
    for (uint i = 0; i < 4; ++i) {
        if (image->huffmanACTables[i].set) {
            std::cout << "Table ID: " << i << '\n';
            std::cout << "Symbols:\n";
            for (uint j = 0; j < 16; ++j) {
                std::cout << (j + 1) << ": ";
                for (uint k = image->huffmanACTables[i].offsets[j]; k < image->huffmanACTables[i].offsets[j + 1]; ++k) {
                    std::cout << std::hex << (uint)image->huffmanACTables[i].symbols[k] << std::dec << ' ';
                }
                std::cout << '\n';
            }
        }
    }
    std::cout << "DRI=============\n";
    std::cout << "Restart Interval: " << image->restartInterval << '\n';
}

void JpgDecoder::readFrameHeader(BitReader& bitReader, JPGImage* const image)
{
    // first two bytes must be 0xFF, SOI
    quint8 last = bitReader.readByte();
    quint8 current = bitReader.readByte();
    if (last != 0xFF || current != SOI) {
        std::cout << "Error - SOI invalid\n";
        image->valid = false;
        return;
    }
    last = bitReader.readByte();
    current = bitReader.readByte();

    // read markers until first scan
    while (image->valid) {
        if (!bitReader.hasBits()) {
            std::cout << "Error - File ended prematurely\n";
            image->valid = false;
            return;
        }
        if (last != 0xFF) {
            std::cout << "Error - Expected a marker\n";
            image->valid = false;
            return;
        }

        if (current == SOF0) {
            image->frameType = SOF0;
            readStartOfFrame(bitReader, image);
        }
        else if (current == SOF2) {
            image->frameType = SOF2;
            readStartOfFrame(bitReader, image);
        }
        else if (current == DQT) {
            readQuantizationTable(bitReader, image);
        }
        else if (current == DHT) {
            readHuffmanTable(bitReader, image);
        }
        else if (current == SOS) {
            // break from while loop at SOS
            break;
        }
        else if (current == DRI) {
            readRestartInterval(bitReader, image);
        }
        else if (current >= APP0 && current <= APP15) {
            readAPPN(bitReader, image);
        }
        else if (current == COM) {
            readComment(bitReader, image);
        }
        // unused markers that can be skipped
        else if ((current >= JPG0 && current <= JPG13) ||
                 current == DNL ||
                 current == DHP ||
                 current == EXP) {
            readComment(bitReader, image);
        }
        else if (current == TEM) {
            // TEM has no size
        }
        // any number of 0xFF in a row is allowed and should be ignored
        else if (current == 0xFF) {
            current = bitReader.readByte();
            continue;
        }

        else if (current == SOI) {
            std::cout << "Error - Embedded JPGs not supported\n";
            image->valid = false;
            return;
        }
        else if (current == EOI) {
            std::cout << "Error - EOI detected before SOS\n";
            image->valid = false;
            return;
        }
        else if (current == DAC) {
            std::cout << "Error - Arithmetic Coding mode not supported\n";
            image->valid = false;
            return;
        }
        else if (current >= SOF0 && current <= SOF15) {
            std::cout << "Error - SOF marker not supported: 0x" << std::hex << (uint)current << std::dec << '\n';
            image->valid = false;
            return;
        }
        else if (current >= RST0 && current <= RST7) {
            std::cout << "Error - RSTN detected before SOS\n";
            image->valid = false;
            return;
        }
        else {
            std::cout << "Error - Unknown marker: 0x" << std::hex << (uint)current << std::dec << '\n';
            image->valid = false;
            return;
        }
        last = bitReader.readByte();
        current = bitReader.readByte();
    }
}

void JpgDecoder::readScans(BitReader& bitReader, JPGImage* const image)
{
    // decode first scan
    readStartOfScan(bitReader, image);
    qDebug() << "readStartOfScan         µs =" << t.nsecsElapsed() /1000; t.restart();
    if (!image->valid) {
        return;
    }
    if (isDebug) printScanInfo(image);
    decodeHuffmanData(bitReader, image);
    qDebug() << "decodeHuffmanData       µs =" << t.nsecsElapsed() /1000; t.restart();

    quint8 last = bitReader.readByte();
    quint8 current = bitReader.readByte();

    // decode additional scans, if any
    while (image->valid) {
        if (!bitReader.hasBits()) {
            std::cout << "Error - File ended prematurely\n";
            image->valid = false;
            return;
        }
        if (last != 0xFF) {
            std::cout << "Error - Expected a marker\n";
            image->valid = false;
            return;
        }

        // end of image
        if (current == EOI) {
            break;
        }
        // huffman tables (progressive only)
        else if (current == DHT && image->frameType == SOF2) {
            readHuffmanTable(bitReader, image);
        }
        // additional scans (progressive only)
        else if (current == SOS && image->frameType == SOF2) {
            readStartOfScan(bitReader, image);
            if (!image->valid) {
                return;
            }
            if (isDebug) printScanInfo(image);
            decodeHuffmanData(bitReader, image);
        }
        // new restart interval (progressive only)
        else if (current == DRI && image->frameType == SOF2) {
            readRestartInterval(bitReader, image);
        }
        // restart marker, perhaps from the very end of previous scan
        else if (current >= RST0 && current <= RST7) {
            // RSTN has no size
        }
        // ignore multiple 0xFF's in a row
        else if (current == 0xFF) {
            current = bitReader.readByte();
            continue;
        }
        else {
            std::cout << "Error - Invalid marker: 0x" << std::hex << (uint)current << std::dec << '\n';
            image->valid = false;
            return;
        }
        last = bitReader.readByte();
        current = bitReader.readByte();
    }
    // qDebug() << "decode additional scans µs =" << t.nsecsElapsed() /1000; t.restart();
}

JpgDecoder::JPGImage* JpgDecoder::readJPG(const std::string& filename)
{
    // open file
    if (isDebug) std::cout << "Reading " << filename << "...\n";
    BitReader bitReader(filename);
    if (!bitReader.hasBits()) {
        std::cout << "Error - Error opening input file\n";
        return nullptr;
    }

    JPGImage* image = new (std::nothrow) JPGImage;
    if (image == nullptr) {
        std::cout << "Error - Memory error\n";
        return nullptr;
    }

    readFrameHeader(bitReader, image);
    qDebug() << "readFrameHeader         µs =" << t.nsecsElapsed() /1000; t.restart();

    if (!image->valid) {
        return image;
    }

    if (isDebug) printFrameInfo(image);

    image->blocks = new (std::nothrow) Block[image->blockHeightReal * image->blockWidthReal];
    if (image->blocks == nullptr) {
        std::cout << "Error - Memory error\n";
        image->valid = false;
        return image;
    }

    readScans(bitReader, image);
    // qDebug() << "readScans               µs =" << t.nsecsElapsed() /1000; t.restart();

    return image;
}

// HUFFMAN VERSION 1

// return the symbol from the Huffman table that corresponds to
//   the next Huffman code read from the BitReader
quint8 JpgDecoder::getNextSymbol(BitReader& bitReader, const HuffmanTable& hTable)
{
    uint currentCode = 0;
    for (uint i = 0; i < 16; ++i) {
        int bit = bitReader.readBit();
        if (bit == -1) {
            return -1;
        }
        currentCode = (currentCode << 1) | bit;
        for (uint j = hTable.offsets[i]; j < hTable.offsets[i + 1]; ++j) {
            if (currentCode == hTable.codes[j]) {
                return hTable.symbols[j];
            }
        }
    }
    return -1;
}

// fill the coefficients of a block component based on Huffman codes
//   read from the BitReader
bool JpgDecoder::decodeBlockComponent(
        const JPGImage* const image,
        BitReader& bitReader,
        int* const component,
        int& previousDC,
        uint& skips,
        const HuffmanTable& dcTable,
        const HuffmanTable& acTable
        )
{
    if (image->frameType == SOF0) {
        // get the DC value for this block component
        quint8 length = getNextSymbol(bitReader, dcTable);
        if (length == (quint8)-1) {
            std::cout << "Error - Invalid DC value\n";
            return false;
        }
        if (length > 11) {
            std::cout << "Error - DC coefficient length greater than 11\n";
            return false;
        }

        int coeff = bitReader.readBits(length);
        if (coeff == -1) {
            std::cout << "Error - Invalid DC value\n";
            return false;
        }
        if (length != 0 && coeff < (1 << (length - 1))) {
            coeff -= (1 << length) - 1;
        }
        component[0] = coeff + previousDC;
        previousDC = component[0];

        // get the AC values for this block component
        for (uint i = 1; i < 64; ++i) {
            quint8 symbol = getNextSymbol(bitReader, acTable);
            if (symbol == (quint8)-1) {
                std::cout << "Error - Invalid AC value\n";
                return false;
            }

            // symbol 0x00 means fill remainder of component with 0
            if (symbol == 0x00) {
                return true;
            }

            // otherwise, read next component coefficient
            quint8 numZeroes = symbol >> 4;
            quint8 coeffLength = symbol & 0x0F;
            coeff = 0;

            if (i + numZeroes >= 64) {
                std::cout << "Error - Zero run-length exceeded block component\n";
                return false;
            }
            i += numZeroes;

            if (coeffLength > 10) {
                std::cout << "Error - AC coefficient length greater than 10\n";
                return false;
            }
            coeff = bitReader.readBits(coeffLength);
            if (coeff == -1) {
                std::cout << "Error - Invalid AC value\n";
                return false;
            }
            if (coeff < (1 << (coeffLength - 1))) {
                coeff -= (1 << coeffLength) - 1;
            }
            component[zigZagMap[i]] = coeff;
        }
        return true;
    }
    else { // image->frameType == SOF2
        if (image->startOfSelection == 0 && image->successiveApproximationHigh == 0) {
            // DC first visit
            quint8 length = getNextSymbol(bitReader, dcTable);
            if (length == (quint8)-1) {
                std::cout << "Error - Invalid DC value\n";
                return false;
            }
            if (length > 11) {
                std::cout << "Error - DC coefficient length greater than 11\n";
                return false;
            }

            int coeff = bitReader.readBits(length);
            if (coeff == -1) {
                std::cout << "Error - Invalid DC value\n";
                return false;
            }
            if (length != 0 && coeff < (1 << (length - 1))) {
                coeff -= (1 << length) - 1;
            }
            coeff += previousDC;
            previousDC = coeff;
            component[0] = coeff << image->successiveApproximationLow;
            return true;
        }
        else if (image->startOfSelection == 0 && image->successiveApproximationHigh != 0) {
            // DC refinement
            int bit = bitReader.readBit();
            if (bit == -1) {
                std::cout << "Error - Invalid DC value\n";
                return false;
            }
            component[0] |= bit << image->successiveApproximationLow;
            return true;
        }
        else if (image->startOfSelection != 0 && image->successiveApproximationHigh == 0) {
            // AC first visit
            if (skips > 0) {
                skips -= 1;
                return true;
            }
            for (uint i = image->startOfSelection; i <= image->endOfSelection; ++i) {
                quint8 symbol = getNextSymbol(bitReader, acTable);
                if (symbol == (quint8)-1) {
                    std::cout << "Error - Invalid AC value\n";
                    return false;
                }

                quint8 numZeroes = symbol >> 4;
                quint8 coeffLength = symbol & 0x0F;

                if (coeffLength != 0) {
                    if (i + numZeroes > image->endOfSelection) {
                        std::cout << "Error - Zero run-length exceeded spectral selection\n";
                        return false;
                    }
                    for (uint j = 0; j < numZeroes; ++j, ++i) {
                        component[zigZagMap[i]] = 0;
                    }
                    if (coeffLength > 10) {
                        std::cout << "Error - AC coefficient length greater than 10\n";
                        return false;
                    }

                    int coeff = bitReader.readBits(coeffLength);
                    if (coeff == -1) {
                        std::cout << "Error - Invalid AC value\n";
                        return false;
                    }
                    if (coeff < (1 << (coeffLength - 1))) {
                        coeff -= (1 << coeffLength) - 1;
                    }
                    component[zigZagMap[i]] = coeff << image->successiveApproximationLow;
                }
                else {
                    if (numZeroes == 15) {
                        if (i + numZeroes > image->endOfSelection) {
                            std::cout << "Error - Zero run-length exceeded spectral selection\n";
                            return false;
                        }
                        for (uint j = 0; j < numZeroes; ++j, ++i) {
                            component[zigZagMap[i]] = 0;
                        }
                    }
                    else {
                        skips = (1 << numZeroes) - 1;
                        uint extraSkips = bitReader.readBits(numZeroes);
                        if (extraSkips == (uint)-1) {
                            std::cout << "Error - Invalid AC value\n";
                            return false;
                        }
                        skips += extraSkips;
                        break;
                    }
                }
            }
            return true;
        }
        else { // image->startOfSelection != 0 && image->successiveApproximationHigh != 0
            // AC refinement
            int positive = 1 << image->successiveApproximationLow;
            int negative = ((unsigned)-1) << image->successiveApproximationLow;
            int i = image->startOfSelection;
            if (skips == 0) {
                for (; i <= image->endOfSelection; ++i) {
                    quint8 symbol = getNextSymbol(bitReader, acTable);
                    if (symbol == (quint8)-1) {
                        std::cout << "Error - Invalid AC value\n";
                        return false;
                    }

                    quint8 numZeroes = symbol >> 4;
                    quint8 coeffLength = symbol & 0x0F;
                    int coeff = 0;

                    if (coeffLength != 0) {
                        if (coeffLength != 1) {
                            std::cout << "Error - Invalid AC value\n";
                            return false;
                        }
                        switch (bitReader.readBit()) {
                        case 1:
                            coeff = positive;
                            break;
                        case 0:
                            coeff = negative;
                            break;
                        default: // -1, data stream is empty
                            std::cout << "Error - Invalid AC value\n";
                            return false;
                        }
                    }
                    else {
                        if (numZeroes != 15) {
                            skips = 1 << numZeroes;
                            uint extraSkips = bitReader.readBits(numZeroes);
                            if (extraSkips == (uint)-1) {
                                std::cout << "Error - Invalid AC value\n";
                                return false;
                            }
                            skips += extraSkips;
                            break;
                        }
                    }

                    do {
                        if (component[zigZagMap[i]] != 0) {
                            switch (bitReader.readBit()) {
                            case 1:
                                if ((component[zigZagMap[i]] & positive) == 0) {
                                    if (component[zigZagMap[i]] >= 0) {
                                        component[zigZagMap[i]] += positive;
                                    }
                                    else {
                                        component[zigZagMap[i]] += negative;
                                    }
                                }
                                break;
                            case 0:
                                // do nothing
                                break;
                            default: // -1, data stream is empty
                                std::cout << "Error - Invalid AC value\n";
                                return false;
                            }
                        }
                        else {
                            if (numZeroes == 0) {
                                break;
                            }
                            numZeroes -= 1;
                        }

                        i += 1;
                    } while (i <= image->endOfSelection);

                    if (coeff != 0 && i <= image->endOfSelection) {
                        component[zigZagMap[i]] = coeff;
                    }
                }
            }

            if (skips > 0) {
                for (; i <= image->endOfSelection; ++i) {
                    if (component[zigZagMap[i]] != 0) {
                        switch (bitReader.readBit()) {
                        case 1:
                            if ((component[zigZagMap[i]] & positive) == 0) {
                                if (component[zigZagMap[i]] >= 0) {
                                    component[zigZagMap[i]] += positive;
                                }
                                else {
                                    component[zigZagMap[i]] += negative;
                                }
                            }
                            break;
                        case 0:
                            // do nothing
                            break;
                        default: // -1, data stream is empty
                            std::cout << "Error - Invalid AC value\n";
                            return false;
                        }
                    }
                }
                skips -= 1;
            }
            return true;
        }
    }
}

// decode all the Huffman data and fill all MCUs
void JpgDecoder::decodeHuffmanData(BitReader& bitReader, JPGImage* const image)
{
    int previousDCs[3] = { 0 };
    uint skips = 0;

    const bool luminanceOnly = image->componentsInScan == 1 && image->colorComponents[0].usedInScan;
    const uint yStep = luminanceOnly ? 1 : image->verticalSamplingFactor;
    const uint xStep = luminanceOnly ? 1 : image->horizontalSamplingFactor;
    const uint restartInterval = image->restartInterval * xStep * yStep;

    for (uint y = 0; y < image->blockHeight; y += yStep) {
        for (uint x = 0; x < image->blockWidth; x += xStep) {
            if (restartInterval != 0 && (y * image->blockWidthReal + x) % restartInterval == 0) {
                previousDCs[0] = 0;
                previousDCs[1] = 0;
                previousDCs[2] = 0;
                skips = 0;
                bitReader.align();
            }

            for (uint i = 0; i < image->numComponents; ++i) {
                const ColorComponent& component = image->colorComponents[i];
                if (component.usedInScan) {
                    const uint vMax = luminanceOnly ? 1 : component.verticalSamplingFactor;
                    const uint hMax = luminanceOnly ? 1 : component.horizontalSamplingFactor;
                    for (uint v = 0; v < vMax; ++v) {
                        for (uint h = 0; h < hMax; ++h) {
                            if (!decodeBlockComponent(
                                image,
                                bitReader,
                                image->blocks[(y + v) * image->blockWidthReal + (x + h)][i],
                                previousDCs[i],
                                skips,
                                image->huffmanDCTables[component.huffmanDCTableID],
                                image->huffmanACTables[component.huffmanACTableID]))
                            {
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}

// dequantize a block component based on a quantization table
void JpgDecoder::dequantizeBlockComponent(const JpgDecoder::QuantizationTable& qTable, int* const component)
{
    for (uint i = 0; i < 64; ++i) {
        component[i] *= qTable.table[i];
    }
}

// dequantize all MCUs
void JpgDecoder::dequantize(const JPGImage* const image)
{
    for (uint y = 0; y < image->blockHeight; y += image->verticalSamplingFactor) {
        for (uint x = 0; x < image->blockWidth; x += image->horizontalSamplingFactor) {
            for (uint i = 0; i < image->numComponents; ++i) {
                const ColorComponent& component = image->colorComponents[i];
                for (uint v = 0; v < component.verticalSamplingFactor; ++v) {
                    for (uint h = 0; h < component.horizontalSamplingFactor; ++h) {
                        dequantizeBlockComponent(image->quantizationTables[component.quantizationTableID],
                                                 image->blocks[(y + v) * image->blockWidthReal + (x + h)][i]);
                    }
                }
            }
        }
    }
}

// perform 1-D IDCT on all columns and rows of a block component
//   resulting in 2-D IDCT
void JpgDecoder::inverseDCTBlockComponent(int* const component)
{

    float intermediate[64];

    for (uint i = 0; i < 8; ++i) {
        const float g0 = component[0 * 8 + i] * s0;
        const float g1 = component[4 * 8 + i] * s4;
        const float g2 = component[2 * 8 + i] * s2;
        const float g3 = component[6 * 8 + i] * s6;
        const float g4 = component[5 * 8 + i] * s5;
        const float g5 = component[1 * 8 + i] * s1;
        const float g6 = component[7 * 8 + i] * s7;
        const float g7 = component[3 * 8 + i] * s3;

        const float f0 = g0;
        const float f1 = g1;
        const float f2 = g2;
        const float f3 = g3;
        const float f4 = g4 - g7;
        const float f5 = g5 + g6;
        const float f6 = g5 - g6;
        const float f7 = g4 + g7;

        const float e0 = f0;
        const float e1 = f1;
        const float e2 = f2 - f3;
        const float e3 = f2 + f3;
        const float e4 = f4;
        const float e5 = f5 - f7;
        const float e6 = f6;
        const float e7 = f5 + f7;
        const float e8 = f4 + f6;

        const float d0 = e0;
        const float d1 = e1;
        const float d2 = e2 * m1;
        const float d3 = e3;
        const float d4 = e4 * m2;
        const float d5 = e5 * m3;
        const float d6 = e6 * m4;
        const float d7 = e7;
        const float d8 = e8 * m5;

        const float c0 = d0 + d1;
        const float c1 = d0 - d1;
        const float c2 = d2 - d3;
        const float c3 = d3;
        const float c4 = d4 + d8;
        const float c5 = d5 + d7;
        const float c6 = d6 - d8;
        const float c7 = d7;
        const float c8 = c5 - c6;

        const float b0 = c0 + c3;
        const float b1 = c1 + c2;
        const float b2 = c1 - c2;
        const float b3 = c0 - c3;
        const float b4 = c4 - c8;
        const float b5 = c8;
        const float b6 = c6 - c7;
        const float b7 = c7;

        intermediate[0 * 8 + i] = b0 + b7;
        intermediate[1 * 8 + i] = b1 + b6;
        intermediate[2 * 8 + i] = b2 + b5;
        intermediate[3 * 8 + i] = b3 + b4;
        intermediate[4 * 8 + i] = b3 - b4;
        intermediate[5 * 8 + i] = b2 - b5;
        intermediate[6 * 8 + i] = b1 - b6;
        intermediate[7 * 8 + i] = b0 - b7;
    }
    for (uint i = 0; i < 8; ++i) {
        const float g0 = intermediate[i * 8 + 0] * s0;
        const float g1 = intermediate[i * 8 + 4] * s4;
        const float g2 = intermediate[i * 8 + 2] * s2;
        const float g3 = intermediate[i * 8 + 6] * s6;
        const float g4 = intermediate[i * 8 + 5] * s5;
        const float g5 = intermediate[i * 8 + 1] * s1;
        const float g6 = intermediate[i * 8 + 7] * s7;
        const float g7 = intermediate[i * 8 + 3] * s3;

        const float f0 = g0;
        const float f1 = g1;
        const float f2 = g2;
        const float f3 = g3;
        const float f4 = g4 - g7;
        const float f5 = g5 + g6;
        const float f6 = g5 - g6;
        const float f7 = g4 + g7;

        const float e0 = f0;
        const float e1 = f1;
        const float e2 = f2 - f3;
        const float e3 = f2 + f3;
        const float e4 = f4;
        const float e5 = f5 - f7;
        const float e6 = f6;
        const float e7 = f5 + f7;
        const float e8 = f4 + f6;

        const float d0 = e0;
        const float d1 = e1;
        const float d2 = e2 * m1;
        const float d3 = e3;
        const float d4 = e4 * m2;
        const float d5 = e5 * m3;
        const float d6 = e6 * m4;
        const float d7 = e7;
        const float d8 = e8 * m5;

        const float c0 = d0 + d1;
        const float c1 = d0 - d1;
        const float c2 = d2 - d3;
        const float c3 = d3;
        const float c4 = d4 + d8;
        const float c5 = d5 + d7;
        const float c6 = d6 - d8;
        const float c7 = d7;
        const float c8 = c5 - c6;

        const float b0 = c0 + c3;
        const float b1 = c1 + c2;
        const float b2 = c1 - c2;
        const float b3 = c0 - c3;
        const float b4 = c4 - c8;
        const float b5 = c8;
        const float b6 = c6 - c7;
        const float b7 = c7;

        component[i * 8 + 0] = b0 + b7 + 0.5f;
        component[i * 8 + 1] = b1 + b6 + 0.5f;
        component[i * 8 + 2] = b2 + b5 + 0.5f;
        component[i * 8 + 3] = b3 + b4 + 0.5f;
        component[i * 8 + 4] = b3 - b4 + 0.5f;
        component[i * 8 + 5] = b2 - b5 + 0.5f;
        component[i * 8 + 6] = b1 - b6 + 0.5f;
        component[i * 8 + 7] = b0 - b7 + 0.5f;
    }
}

// perform IDCT on all MCUs
void JpgDecoder::inverseDCT(const JPGImage* const image)
{
    for (uint y = 0; y < image->blockHeight; y += image->verticalSamplingFactor) {
        for (uint x = 0; x < image->blockWidth; x += image->horizontalSamplingFactor) {
            for (uint i = 0; i < image->numComponents; ++i) {
                const ColorComponent& component = image->colorComponents[i];
                for (uint v = 0; v < component.verticalSamplingFactor; ++v) {
                    for (uint h = 0; h < component.horizontalSamplingFactor; ++h) {
                        inverseDCTBlockComponent(image->blocks[(y + v) * image->blockWidthReal + (x + h)][i]);
                    }
                }
            }
        }
    }
}

// convert all pixels in a block from YCbCr color space to RGB
void JpgDecoder::YCbCrToRGBBlock(Block& yBlock, const Block& cbcrBlock,
                     const uint vSamp, const uint hSamp,
                     const uint v, const uint h)
{
    for (uint y = 7; y < 8; --y) {
        for (uint x = 7; x < 8; --x) {
            const uint pixel = y * 8 + x;
            const uint cbcrPixelRow = y / vSamp + 4 * v;
            const uint cbcrPixelColumn = x / hSamp + 4 * h;
            const uint cbcrPixel = cbcrPixelRow * 8 + cbcrPixelColumn;
            int r = yBlock.y[pixel]                                    + 1.402f * cbcrBlock.cr[cbcrPixel] + 128;
            int g = yBlock.y[pixel] - 0.344f * cbcrBlock.cb[cbcrPixel] - 0.714f * cbcrBlock.cr[cbcrPixel] + 128;
            int b = yBlock.y[pixel] + 1.772f * cbcrBlock.cb[cbcrPixel]                                    + 128;
            if (r < 0)   r = 0;
            if (r > 255) r = 255;
            if (g < 0)   g = 0;
            if (g > 255) g = 255;
            if (b < 0)   b = 0;
            if (b > 255) b = 255;
            yBlock.r[pixel] = r;
            yBlock.g[pixel] = g;
            yBlock.b[pixel] = b;
        }
    }
}

// convert all pixels from YCbCr color space to RGB
void JpgDecoder::YCbCrToRGB(const JPGImage* const image)
{
    const uint vSamp = image->verticalSamplingFactor;
    const uint hSamp = image->horizontalSamplingFactor;
    for (uint y = 0; y < image->blockHeight; y += vSamp) {
        for (uint x = 0; x < image->blockWidth; x += hSamp) {
            const Block& cbcrBlock = image->blocks[y * image->blockWidthReal + x];
            for (uint v = vSamp - 1; v < vSamp; --v) {
                for (uint h = hSamp - 1; h < hSamp; --h) {
                    Block& yBlock = image->blocks[(y + v) * image->blockWidthReal + (x + h)];
                    YCbCrToRGBBlock(yBlock, cbcrBlock, vSamp, hSamp, v, h);
                }
            }
        }
    }
}

// helper function to write a 4-quint8 integer in little-endian
void JpgDecoder::putInt(quint8*& bufferPos, const uint v)
{
    *bufferPos++ = v >>  0;
    *bufferPos++ = v >>  8;
    *bufferPos++ = v >> 16;
    *bufferPos++ = v >> 24;
}

// helper function to write a 2-quint8 short integer in little-endian
void JpgDecoder::putShort(quint8*& bufferPos, const uint v)
{
    *bufferPos++ = v >> 0;
    *bufferPos++ = v >> 8;
}

// write all the pixels in the MCUs to a BMP file
void JpgDecoder::writeBMP(const JPGImage* const image, const std::string& filename)
{
    // open file
    if (isDebug) std::cout << "Writing " << filename << "...\n";
    std::ofstream outFile(filename, std::ios::out | std::ios::binary);
    if (!outFile.is_open()) {
        std::cout << "Error - Error opening output file\n";
        return;
    }

    const uint paddingSize = image->width % 4;
    const uint size = 14 + 12 + image->height * image->width * 3 + paddingSize * image->height;

    quint8* buffer = new (std::nothrow) quint8[size];
    if (buffer == nullptr) {
        std::cout << "Error - Memory error\n";
        outFile.close();
        return;
    }
    quint8* bufferPos = buffer;

    *bufferPos++ = 'B';
    *bufferPos++ = 'M';
    putInt(bufferPos, size);
    putInt(bufferPos, 0);
    putInt(bufferPos, 0x1A);
    putInt(bufferPos, 12);
    putShort(bufferPos, image->width);
    putShort(bufferPos, image->height);
    putShort(bufferPos, 1);
    putShort(bufferPos, 24);

    for (uint y = image->height - 1; y < image->height; --y) {
        const uint blockRow = y / 8;
        const uint pixelRow = y % 8;
        for (uint x = 0; x < image->width; ++x) {
            const uint blockColumn = x / 8;
            const uint pixelColumn = x % 8;
            const uint blockIndex = blockRow * image->blockWidthReal + blockColumn;
            const uint pixelIndex = pixelRow * 8 + pixelColumn;
            *bufferPos++ = image->blocks[blockIndex].b[pixelIndex];
            *bufferPos++ = image->blocks[blockIndex].g[pixelIndex];
            *bufferPos++ = image->blocks[blockIndex].r[pixelIndex];
        }
        for (uint i = 0; i < paddingSize; ++i) {
            *bufferPos++ = 0;
        }
    }

    outFile.write((char*)buffer, size);
    outFile.close();
    delete[] buffer;
}

bool JpgDecoder::decode(QString fPath)
{
    const std::string filename = fPath.toStdString();

    t.start();

    // read image
    JPGImage* image = readJPG(filename);
    qDebug() << "readJPG                 µs =" << t.nsecsElapsed() /1000; t.restart();
    // validate image
    if (image == nullptr) {
        return false;
    }
    if (image->blocks == nullptr) {
        delete image;
        return false;
    }
    if (image->valid == false) {
        delete[] image->blocks;
        delete image;
        return false;
    }
    // qDebug() << "finish prep             µs =" << t.nsecsElapsed() /1000; t.restart();

    // dequantize DCT coefficients
    dequantize(image);
    qDebug() << "dequantize              µs =" << t.nsecsElapsed() /1000; t.restart();

    // Inverse Discrete Cosine Transform
    inverseDCT(image);
    qDebug() << "inverseDCT              µs =" << t.nsecsElapsed() /1000; t.restart();

    // color conversion
    YCbCrToRGB(image);
    qDebug() << "YCbCrToRGB              µs =" << t.nsecsElapsed() /1000; t.restart();

    // qDebug() << "JpgDecoder::decode  ms =" << t.elapsed() << fPath;

    if (isDebug)
    {
        // write BMP file
        const std::size_t pos = filename.find_last_of('.');
        const std::string outFilename = (pos == std::string::npos) ?
                                            (filename + ".bmp") :
                                            (filename.substr(0, pos) + ".bmp");
        writeBMP(image, outFilename);
}

    delete[] image->blocks;
    delete image;

    return true;
}
