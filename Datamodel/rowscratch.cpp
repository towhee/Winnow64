#include "Datamodel/rowscratch.h"

/*
    The column <-> field mapping for the scratch set, and the one place that
    knows a scratch column is not held on the row at all. G::dataModelColumns
    stays the public address space; DataModel::data/setData is the seam.
*/

namespace {

/*  One bit per covered column, in G::dataModelColumns order. Returned as a
    MASK rather than an index so "not covered" is simply 0 and every caller
    tests the same way. */
enum ScratchBit : quint32 {
    B_OffsetFull          = 1u << 0,
    B_LengthFull          = 1u << 1,
    B_WidthOrigPreview    = 1u << 2,
    B_HeightOrigPreview   = 1u << 3,
    B_OffsetThumb         = 1u << 4,
    B_LengthThumb         = 1u << 5,
    B_SamplesPerPixel     = 1u << 6,
    B_IsBigEndian         = 1u << 7,
    B_Ifd0Offset          = 1u << 8,
    B_IfdOffsets          = 1u << 9,
    B_XmpSegmentOffset    = 1u << 10,
    B_XmpSegmentLength    = 1u << 11,
    B_IsXmp               = 1u << 12,
    B_IccSegmentOffset    = 1u << 13,
    B_IccSegmentLength    = 1u << 14,
    B_IccBuf              = 1u << 15,
    B_IccSpace            = 1u << 16,
    B_CacheSize           = 1u << 17,
    B_IsCaching           = 1u << 18,
    B_IsCached            = 1u << 19,
    B_Attempts            = 1u << 20,
    B_DecoderId           = 1u << 21,
    B_DecoderStatus       = 1u << 22,
    B_DecoderErrMsg       = 1u << 23,
};

quint32 bitFor(int column)
{
    switch (column) {
    case G::OffsetFullColumn:            return B_OffsetFull;
    case G::LengthFullColumn:            return B_LengthFull;
    case G::WidthOrigPreviewColumn:      return B_WidthOrigPreview;
    case G::HeightOrigPreviewColumn:     return B_HeightOrigPreview;
    case G::OffsetThumbColumn:           return B_OffsetThumb;
    case G::LengthThumbColumn:           return B_LengthThumb;
    case G::samplesPerPixelColumn:       return B_SamplesPerPixel;
    case G::isBigEndianColumn:           return B_IsBigEndian;
    case G::ifd0OffsetColumn:            return B_Ifd0Offset;
    case G::ifdOffsetsColumn:            return B_IfdOffsets;
    case G::XmpSegmentOffsetColumn:      return B_XmpSegmentOffset;
    case G::XmpSegmentLengthColumn:      return B_XmpSegmentLength;
    case G::IsXMPColumn:                 return B_IsXmp;
    case G::ICCSegmentOffsetColumn:      return B_IccSegmentOffset;
    case G::ICCSegmentLengthColumn:      return B_IccSegmentLength;
    case G::ICCBufColumn:                return B_IccBuf;
    case G::ICCSpaceColumn:              return B_IccSpace;
    case G::CacheSizeColumn:             return B_CacheSize;
    case G::IsCachingColumn:             return B_IsCaching;
    case G::IsCachedColumn:              return B_IsCached;
    case G::AttemptsColumn:              return B_Attempts;
    case G::DecoderIdColumn:             return B_DecoderId;
    case G::DecoderReturnStatusColumn:   return B_DecoderStatus;
    case G::DecoderErrMsgColumn:         return B_DecoderErrMsg;
    default:                             return 0;
    }
}

} // namespace

bool ScratchStore::covers(int column, int role)
{
    /*  EditRole and DisplayRole only, matching RowStore and matching what a
        QStandardItem actually does -- it keeps those two in the same slot.
        Alignment, tooltip and the G:: custom roles are presentation and fall
        through to the items untouched. */
    if (role != Qt::EditRole && role != Qt::DisplayRole) return false;
    return bitFor(column) != 0;
}

QVariant ScratchStore::value(int row, int column) const
{
    auto it = mRows.constFind(row);
    if (it == mRows.cend()) return QVariant();
    const RowScratch &s = *it;

    const quint32 bit = bitFor(column);
    /*  Nothing has written this field, so the item it shadows is UNSET and an
        invalid QVariant is the right answer. Returning the field's zero here
        would turn "no offset yet" into "offset 0", which is a legal offset. */
    if (!bit || !(s.setMask & bit)) return QVariant();

    switch (column) {
    case G::OffsetFullColumn:            return s.offsetFull;
    case G::LengthFullColumn:            return s.lengthFull;
    case G::WidthOrigPreviewColumn:      return s.widthOrigPreview;
    case G::HeightOrigPreviewColumn:     return s.heightOrigPreview;
    case G::OffsetThumbColumn:           return s.offsetThumb;
    case G::LengthThumbColumn:           return s.lengthThumb;
    case G::samplesPerPixelColumn:       return s.samplesPerPixel;
    case G::isBigEndianColumn:           return s.isBigEnd;
    case G::ifd0OffsetColumn:            return s.ifd0Offset;
    case G::ifdOffsetsColumn: {
        QVariantList out;
        out.reserve(s.ifdOffsets.size());
        for (quint32 v : s.ifdOffsets) out.append(QVariant(v));
        return out;
    }
    case G::XmpSegmentOffsetColumn:      return s.xmpSegmentOffset;
    case G::XmpSegmentLengthColumn:      return s.xmpSegmentLength;
    case G::IsXMPColumn:                 return s.isXmp;
    case G::ICCSegmentOffsetColumn:      return s.iccSegmentOffset;
    case G::ICCSegmentLengthColumn:      return s.iccSegmentLength;
    case G::ICCBufColumn:                return mBlobs.value(s.iccBufId);
    case G::ICCSpaceColumn:              return mStrings.value(s.iccSpaceId);
    case G::CacheSizeColumn:             return s.cacheSizeMB;
    case G::IsCachingColumn:             return s.isCaching;
    case G::IsCachedColumn:              return s.isCached;
    case G::AttemptsColumn:              return s.attempts;
    case G::DecoderIdColumn:             return s.decoderId;
    case G::DecoderReturnStatusColumn:   return s.decoderStatus;
    case G::DecoderErrMsgColumn:         return s.decoderErrMsg;
    default:                             return QVariant();
    }
}

void ScratchStore::setValue(int row, int column, const QVariant &v)
{
    const quint32 bit = bitFor(column);
    if (!bit || row < 0) return;

    /*  The entry is created on the first write, not at row creation. That is
        the whole point: a row nothing has decoded or cached costs nothing. */
    RowScratch &s = mRows[row];
    s.setMask |= bit;

    switch (column) {
    case G::OffsetFullColumn:            s.offsetFull = v.toUInt(); break;
    case G::LengthFullColumn:            s.lengthFull = v.toUInt(); break;
    case G::WidthOrigPreviewColumn:      s.widthOrigPreview = v.toInt(); break;
    case G::HeightOrigPreviewColumn:     s.heightOrigPreview = v.toInt(); break;
    case G::OffsetThumbColumn:           s.offsetThumb = v.toUInt(); break;
    case G::LengthThumbColumn:           s.lengthThumb = v.toUInt(); break;
    case G::samplesPerPixelColumn:       s.samplesPerPixel = v.toInt(); break;
    case G::isBigEndianColumn:           s.isBigEnd = v.toBool(); break;
    case G::ifd0OffsetColumn:            s.ifd0Offset = v.toUInt(); break;
    case G::ifdOffsetsColumn: {
        const QVariantList in = v.toList();
        s.ifdOffsets.clear();
        s.ifdOffsets.reserve(in.size());
        for (const QVariant &e : in) s.ifdOffsets.append(e.toUInt());
        break;
    }
    case G::XmpSegmentOffsetColumn:      s.xmpSegmentOffset = v.toUInt(); break;
    case G::XmpSegmentLengthColumn:      s.xmpSegmentLength = v.toUInt(); break;
    case G::IsXMPColumn:                 s.isXmp = v.toBool(); break;
    case G::ICCSegmentOffsetColumn:      s.iccSegmentOffset = v.toUInt(); break;
    case G::ICCSegmentLengthColumn:      s.iccSegmentLength = v.toUInt(); break;
    case G::ICCBufColumn:                s.iccBufId = mBlobs.id(v.toByteArray()); break;
    case G::ICCSpaceColumn:              s.iccSpaceId = mStrings.id(v.toString()); break;
    case G::CacheSizeColumn:             s.cacheSizeMB = v.toFloat(); break;
    case G::IsCachingColumn:             s.isCaching = v.toBool(); break;
    case G::IsCachedColumn:              s.isCached = v.toBool(); break;
    case G::AttemptsColumn:              s.attempts = v.toInt(); break;
    case G::DecoderIdColumn:             s.decoderId = v.toInt(); break;
    case G::DecoderReturnStatusColumn:   s.decoderStatus = v.toInt(); break;
    case G::DecoderErrMsgColumn:         s.decoderErrMsg = v.toString(); break;
    default:                             break;
    }
}
