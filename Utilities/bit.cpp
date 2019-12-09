#include "bit.h"

Bit::Bit()
{

}

/*
bitset positions: 31, 30, 29, ... 5, 4, 3, 2, 1, 0
*/
void Bit::uint8ToBits(quint8 byte, std::bitset<32>& bits, ulong pos)
{
    for (ulong i = 0; i < 8; ++i) {
        bits[i + pos] = ((byte >> i) & 1);
    }
}

void Bit::uint32ToBits(quint32 byte, std::bitset<32> &bits)
{
    for (uint i = 0; i < 32; ++i) {
        bits[i] = ((byte >> i) & 1);
    }
}

std::bitset<32> Bit::shiftLeft(uint n, std::bitset<32> &bits)
{
    return bits << n;
}

std::bitset<32> Bit::shiftRight(uint n, std::bitset<32> &bits)
{
    return bits >> n;
}

QString Bit::strBits(std::bitset<32> &bits)
{
    return QString::fromStdString(bits.to_string());
}
