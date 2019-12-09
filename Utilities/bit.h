#ifndef BIT_H
#define BIT_H

#include <QCoreApplication>
#include <bitset>

class Bit
{
public:
    Bit();
    static void uint8ToBits(quint8 byte, std::bitset<32>& bits, ulong pos);
    static void uint32ToBits(quint32 byte, std::bitset<32>& bits);
    static std::bitset<32> shiftRight(uint n, std::bitset<32>& bits);
    static std::bitset<32> shiftLeft(uint n, std::bitset<32>& bits);
    static QString strBits(std::bitset<32> &bits);
};

#endif // BIT_H
