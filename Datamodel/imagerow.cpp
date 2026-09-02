#include "Datamodel/imagerow.h"

/*
    The column <-> field mapping. This is the seam that lets the storage change
    without any view, delegate or saved column-width setting changing with it:
    G::dataModelColumns stays the public address space, and this switch is the
    one place that knows how a column is actually held.
*/

/*  THE ONE COLUMN <-> FIELD MAPPING. Every column this store holds has a BIT
    here, and covers() is derived from it rather than kept as a second list --
    two lists that must agree is one list too many, which is the lesson the
    write-side guard already taught (see the PathRole note below).

    The bit index doubles as the setMask position, so "is this column held here"
    and "has this row's copy of it been written" are answered from the same
    table. Indices are dense and their ORDER is private to this file: nothing is
    persisted by bit number.
*/
namespace {

enum Field {
    F_Path = 0, F_Name, F_FolderName, F_Type, F_ByteSize, F_Created, F_Modified,
    F_Year, F_Day, F_Rating, F_Label, F_Pick, F_Title, F_Creator, F_Copyright,
    F_CameraMake, F_CameraModel, F_Lens, F_Iso, F_Aperture, F_Shutterspeed,
    F_FocalLength, F_Width, F_Height, F_Dimensions, F_MegaPixels, F_GPSCoord,
    F_Keywords, F_KeywordPaths, F_KeywordsAll, F_Video, F_MetadataStatus,
    F_IconLoaded, F_SearchText, F_Compare, F_RowNumber,
    /* second batch */
    F_ExposureComp, F_Duration, F_FocusX, F_FocusY, F_AspectRatio,
    F_IconAspectRatio, F_Orientation, F_Rotation, F_Email, F_Url,
    F_MetadataReading, F_Rating_, F_Label_, F_Creator_, F_Title_, F_Copyright_,
    F_Email_, F_Url_, F_Permissions, F_ReadWrite, F_Sidecar, F_OrientationOffset,
    F_RotationDegrees, F_ShootingInfo, F_Err, F_Develop, F_DevPreviewKey,
    F_Search, F_Ingested, F_Availability,
    /* column 0's custom roles -- not values */
    F_IconRect, F_DupHideRaw, F_DupIsJpg, F_DupRawType, F_DupOtherIdx,
    F_Count
};
static_assert(F_Count <= 128, "ImageRow::setLo/setHi hold 128 bits");

int fieldBit(int column, int role)
{
    /*  Column 0 is six fields, not one. The path is on G::PathRole because
        addFileDataForRow never set EditRole there -- an unset item returns an
        invalid QVariant and callers test for it -- and the icon rect and the
        four pairing roles sit beside it on their own roles. */
    if (column == G::PathColumn) {
        switch (role) {
        case G::PathRole:        return F_Path;
        case G::IconRectRole:    return F_IconRect;
        case G::DupHideRawRole:  return F_DupHideRaw;
        case G::DupIsJpgRole:    return F_DupIsJpg;
        case G::DupRawTypeRole:  return F_DupRawType;
        case G::DupOtherIdxRole: return F_DupOtherIdx;
        default:                 return -1;
        }
    }
    if (role != Qt::EditRole && role != Qt::DisplayRole) return -1;

    switch (column) {
    case G::NameColumn:                 return F_Name;
    case G::FolderNameColumn:           return F_FolderName;
    case G::TypeColumn:                 return F_Type;
    case G::ByteSizeColumn:             return F_ByteSize;
    case G::CreatedColumn:              return F_Created;
    case G::ModifiedColumn:             return F_Modified;
    case G::YearColumn:                 return F_Year;
    case G::DayColumn:                  return F_Day;
    case G::RatingColumn:               return F_Rating;
    case G::LabelColumn:                return F_Label;
    case G::PickColumn:                 return F_Pick;
    case G::TitleColumn:                return F_Title;
    case G::CreatorColumn:              return F_Creator;
    case G::CopyrightColumn:            return F_Copyright;
    case G::CameraMakeColumn:           return F_CameraMake;
    case G::CameraModelColumn:          return F_CameraModel;
    case G::LensColumn:                 return F_Lens;
    case G::ISOColumn:                  return F_Iso;
    case G::ApertureColumn:             return F_Aperture;
    case G::ShutterspeedColumn:         return F_Shutterspeed;
    case G::FocalLengthColumn:          return F_FocalLength;
    case G::WidthColumn:                return F_Width;
    case G::HeightColumn:               return F_Height;
    case G::DimensionsColumn:           return F_Dimensions;
    case G::MegaPixelsColumn:           return F_MegaPixels;
    case G::GPSCoordColumn:             return F_GPSCoord;
    case G::KeywordsColumn:             return F_Keywords;
    case G::KeywordPathsColumn:         return F_KeywordPaths;
    case G::KeywordsAllColumn:          return F_KeywordsAll;
    case G::VideoColumn:                return F_Video;
    case G::MetadataStatusColumn:       return F_MetadataStatus;
    case G::IconLoadedColumn:           return F_IconLoaded;
    case G::SearchTextColumn:           return F_SearchText;
    case G::CompareColumn:              return F_Compare;
    case G::RowNumberColumn:            return F_RowNumber;
    case G::ExposureCompensationColumn: return F_ExposureComp;
    case G::DurationColumn:             return F_Duration;
    case G::FocusXColumn:               return F_FocusX;
    case G::FocusYColumn:               return F_FocusY;
    case G::AspectRatioColumn:          return F_AspectRatio;
    case G::IconAspectRatioColumn:      return F_IconAspectRatio;
    case G::OrientationColumn:          return F_Orientation;
    case G::RotationColumn:             return F_Rotation;
    case G::EmailColumn:                return F_Email;
    case G::UrlColumn:                  return F_Url;
    case G::MetadataReadingColumn:      return F_MetadataReading;
    case G::_RatingColumn:              return F_Rating_;
    case G::_LabelColumn:               return F_Label_;
    case G::_CreatorColumn:             return F_Creator_;
    case G::_TitleColumn:               return F_Title_;
    case G::_CopyrightColumn:           return F_Copyright_;
    case G::_EmailColumn:               return F_Email_;
    case G::_UrlColumn:                 return F_Url_;
    case G::PermissionsColumn:          return F_Permissions;
    case G::ReadWriteColumn:            return F_ReadWrite;
    case G::SidecarColumn:              return F_Sidecar;
    case G::OrientationOffsetColumn:    return F_OrientationOffset;
    case G::RotationDegreesColumn:      return F_RotationDegrees;
    case G::ShootingInfoColumn:         return F_ShootingInfo;
    case G::ErrColumn:                  return F_Err;
    case G::DevelopColumn:              return F_Develop;
    case G::DevPreviewKeyColumn:        return F_DevPreviewKey;
    case G::AvailabilityColumn:         return F_Availability;

    /*  SETTLED, AND NOW HELD. Both columns used to carry a different TYPE
        depending on which path wrote them last -- "false" (QString) at row
        creation, a bool once the work ran, "true" (QString) from a third path
        -- and both are bools throughout now (see "Settling Search and Ingested"
        in Documentation.txt for what that was and was not costing). */
    case G::SearchColumn:               return F_Search;
    case G::IngestedColumn:             return F_Ingested;
    default:                            return -1;
    }
}

inline bool bitIsSet(const ImageRow &r, int bit)
{
    return bit < 64 ? (r.setLo & (quint64(1) << bit))
                    : (r.setHi & (quint64(1) << (bit - 64)));
}

inline void setBit(ImageRow &r, int bit)
{
    if (bit < 64) r.setLo |= (quint64(1) << bit);
    else          r.setHi |= (quint64(1) << (bit - 64));
}

} // namespace

bool RowStore::covers(int column, int role)
{
    return fieldBit(column, role) >= 0;
}

QVariant RowStore::value(int row, int column, int role) const
{
    /*  Read lock: see "THREADING" in imagerow.h. Taken here rather than through
        contains(), which takes its own -- two acquisitions would leave a window
        between the bounds check and the read. */
    QReadLocker locker(&mLock);
    if (row < 0 || row >= mRows.size()) return QVariant();
    const ImageRow &r = mRows.at(row);

    /*  Nothing has written this field, so the item it replaces is UNSET and an
        invalid QVariant is the right answer -- not the field's zero, and not an
        interned empty string. See "WHICH FIELDS HAVE ACTUALLY BEEN WRITTEN" in
        imagerow.h for why that distinction is load-bearing. */
    const int bit = fieldBit(column, role);
    if (bit < 0 || !bitIsSet(r, bit)) return QVariant();

    if (column == G::PathColumn) {
        switch (role) {
        case G::PathRole:        return r.path;
        case G::IconRectRole:    return r.iconRect;
        case G::DupHideRawRole:  return r.dupHideRaw;
        case G::DupIsJpgRole:    return r.dupIsJpg;
        case G::DupRawTypeRole:  return mStrings.value(r.dupRawTypeId);
        case G::DupOtherIdxRole: return r.dupOtherIdx;
        default:                 return QVariant();
        }
    }

    switch (column) {
    case G::NameColumn:            return r.name;
    case G::FolderNameColumn:      return mStrings.value(r.folderId);
    case G::TypeColumn:            return mStrings.value(r.typeId);
    case G::ByteSizeColumn:        return r.byteSize;
    case G::CreatedColumn:         return mStrings.value(r.createdId);
    case G::ModifiedColumn:        return mStrings.value(r.modifiedId);
    case G::YearColumn:            return mStrings.value(r.yearId);
    case G::DayColumn:             return mStrings.value(r.dayId);
    case G::RatingColumn:          return mStrings.value(r.ratingId);
    case G::LabelColumn:           return mStrings.value(r.labelId);
    case G::PickColumn:            return mStrings.value(r.pickId);
    case G::TitleColumn:           return r.title;
    case G::CreatorColumn:         return mStrings.value(r.creatorId);
    case G::CopyrightColumn:       return mStrings.value(r.copyrightId);
    case G::CameraMakeColumn:      return mStrings.value(r.makeId);
    case G::CameraModelColumn:     return mStrings.value(r.modelId);
    case G::LensColumn:            return mStrings.value(r.lensId);
    case G::ISOColumn:             return r.iso;
    case G::ApertureColumn:        return r.aperture;
    case G::ShutterspeedColumn:    return r.exposureTime;
    case G::FocalLengthColumn:     return r.focalLength;
    case G::WidthColumn:           return r.width;
    case G::HeightColumn:          return r.height;
    case G::DimensionsColumn:      return mStrings.value(r.dimensionsId);
    case G::MegaPixelsColumn:      return mStrings.value(r.megaPixelsId);
    case G::GPSCoordColumn:        return mStrings.value(r.gpsId);
    case G::CompareColumn:         return r.isCompare;
    case G::SearchColumn:          return r.isSearch;
    case G::IngestedColumn:        return r.isIngested;
    case G::SearchTextColumn:      return r.searchText;
    case G::RowNumberColumn:       return r.rowNumber;
    case G::VideoColumn:           return r.isVideo;
    case G::IconLoadedColumn:      return r.iconLoaded;
    case G::MetadataStatusColumn:  return int(r.metaStatus);
    case G::ExposureCompensationColumn: return mStrings.value(r.exposureCompId);
    case G::DurationColumn:        return mStrings.value(r.durationId);
    case G::FocusXColumn:          return r.focusX;
    case G::FocusYColumn:          return r.focusY;
    case G::AspectRatioColumn:     return mStrings.value(r.aspectRatioId);
    case G::IconAspectRatioColumn: return r.iconAspectRatio;
    case G::OrientationColumn:     return r.orientation;
    case G::RotationColumn:        return mStrings.value(r.rotationId);
    case G::EmailColumn:           return mStrings.value(r.emailId);
    case G::UrlColumn:             return mStrings.value(r.urlId);
    case G::MetadataReadingColumn: return r.metadataReading;
    case G::_RatingColumn:         return mStrings.value(r._ratingId);
    case G::_LabelColumn:          return mStrings.value(r._labelId);
    case G::_CreatorColumn:        return mStrings.value(r._creatorId);
    case G::_TitleColumn:          return mStrings.value(r._titleId);
    case G::_CopyrightColumn:      return mStrings.value(r._copyrightId);
    case G::_EmailColumn:          return mStrings.value(r._emailId);
    case G::_UrlColumn:            return mStrings.value(r._urlId);
    case G::PermissionsColumn:     return r.permissions;
    case G::ReadWriteColumn:       return r.isReadWrite;
    case G::SidecarColumn:         return r.hasSidecar;
    case G::OrientationOffsetColumn: return r.orientationOffset;
    case G::RotationDegreesColumn: return r.rotationDegrees;
    case G::ShootingInfoColumn:    return mStrings.value(r.shootingInfoId);
    case G::ErrColumn:             return r.err;
    case G::DevelopColumn:         return r.developEdited;
    case G::DevPreviewKeyColumn:   return mStrings.value(r.devPreviewKeyId);
    case G::AvailabilityColumn:    return int(r.availability);
    case G::KeywordsColumn:
    case G::KeywordPathsColumn:
    case G::KeywordsAllColumn: {
        const QVector<qint32> &ids =
            column == G::KeywordsColumn     ? r.keywordIds :
            column == G::KeywordPathsColumn ? r.keywordPathIds : r.keywordAllIds;
        QStringList out;
        out.reserve(ids.size());
        for (qint32 id : ids) out << mStrings.value(id);
        return out;
    }
    default:
        return QVariant();
    }
}

void RowStore::setValue(int row, int column, int role, const QVariant &v)
{
    QWriteLocker locker(&mLock);
    if (row < 0 || row >= mRows.size()) return;
    const int bit = fieldBit(column, role);
    if (bit < 0) return;
    ImageRow &r = mRows[row];
    setBit(r, bit);

    if (column == G::PathColumn) {
        switch (role) {
        case G::PathRole:        r.path = v.toString(); break;
        case G::IconRectRole:    r.iconRect = v.toRect(); break;
        case G::DupHideRawRole:  r.dupHideRaw = v.toBool(); break;
        case G::DupIsJpgRole:    r.dupIsJpg = v.toBool(); break;
        case G::DupRawTypeRole:  r.dupRawTypeId = mStrings.id(v.toString()); break;
        case G::DupOtherIdxRole: r.dupOtherIdx = v.toInt(); break;
        default: break;
        }
        return;
    }

    switch (column) {
    case G::NameColumn:            r.name = v.toString(); break;
    case G::FolderNameColumn:      r.folderId = mStrings.id(v.toString()); break;
    case G::TypeColumn:            r.typeId = mStrings.id(v.toString()); break;
    case G::ByteSizeColumn:        r.byteSize = v.toLongLong(); break;
    case G::CreatedColumn:         r.createdId = mStrings.id(v.toString()); break;
    case G::ModifiedColumn:        r.modifiedId = mStrings.id(v.toString()); break;
    case G::YearColumn:            r.yearId = mStrings.id(v.toString()); break;
    case G::DayColumn:             r.dayId = mStrings.id(v.toString()); break;
    case G::RatingColumn:          r.ratingId = mStrings.id(v.toString()); break;
    case G::LabelColumn:           r.labelId = mStrings.id(v.toString()); break;
    case G::PickColumn:            r.pickId = mStrings.id(v.toString()); break;
    case G::TitleColumn:           r.title = v.toString(); break;
    case G::CreatorColumn:         r.creatorId = mStrings.id(v.toString()); break;
    case G::CopyrightColumn:       r.copyrightId = mStrings.id(v.toString()); break;
    case G::CameraMakeColumn:      r.makeId = mStrings.id(v.toString()); break;
    case G::CameraModelColumn:     r.modelId = mStrings.id(v.toString()); break;
    case G::LensColumn:            r.lensId = mStrings.id(v.toString()); break;
    case G::ISOColumn:             r.iso = v.toInt(); break;
    case G::ApertureColumn:        r.aperture = v.toDouble(); break;
    case G::ShutterspeedColumn:    r.exposureTime = v.toDouble(); break;
    case G::FocalLengthColumn:     r.focalLength = v.toInt(); break;
    case G::WidthColumn:           r.width = v.toInt(); break;
    case G::HeightColumn:          r.height = v.toInt(); break;
    case G::DimensionsColumn:      r.dimensionsId = mStrings.id(v.toString()); break;
    case G::MegaPixelsColumn:      r.megaPixelsId = mStrings.id(v.toString()); break;
    case G::GPSCoordColumn:        r.gpsId = mStrings.id(v.toString()); break;
    case G::CompareColumn:         r.isCompare = v.toBool(); break;
    case G::SearchColumn:          r.isSearch = v.toBool(); break;
    case G::IngestedColumn:        r.isIngested = v.toBool(); break;
    case G::SearchTextColumn:      r.searchText = v.toString(); break;
    case G::RowNumberColumn:       r.rowNumber = v.toInt(); break;
    case G::VideoColumn:           r.isVideo = v.toBool(); break;
    case G::IconLoadedColumn:      r.iconLoaded = v.toBool(); break;
    case G::MetadataStatusColumn:  r.metaStatus = quint8(v.toInt()); break;
    case G::ExposureCompensationColumn: r.exposureCompId = mStrings.id(v.toString()); break;
    case G::DurationColumn:        r.durationId = mStrings.id(v.toString()); break;
    case G::FocusXColumn:          r.focusX = v.toFloat(); break;
    case G::FocusYColumn:          r.focusY = v.toFloat(); break;
    case G::AspectRatioColumn:     r.aspectRatioId = mStrings.id(v.toString()); break;
    case G::IconAspectRatioColumn: r.iconAspectRatio = v.toDouble(); break;
    case G::OrientationColumn:     r.orientation = v.toInt(); break;
    case G::RotationColumn:        r.rotationId = mStrings.id(v.toString()); break;
    case G::EmailColumn:           r.emailId = mStrings.id(v.toString()); break;
    case G::UrlColumn:             r.urlId = mStrings.id(v.toString()); break;
    case G::MetadataReadingColumn: r.metadataReading = v.toBool(); break;
    case G::_RatingColumn:         r._ratingId = mStrings.id(v.toString()); break;
    case G::_LabelColumn:          r._labelId = mStrings.id(v.toString()); break;
    case G::_CreatorColumn:        r._creatorId = mStrings.id(v.toString()); break;
    case G::_TitleColumn:          r._titleId = mStrings.id(v.toString()); break;
    case G::_CopyrightColumn:      r._copyrightId = mStrings.id(v.toString()); break;
    case G::_EmailColumn:          r._emailId = mStrings.id(v.toString()); break;
    case G::_UrlColumn:            r._urlId = mStrings.id(v.toString()); break;
    case G::PermissionsColumn:     r.permissions = v.toUInt(); break;
    case G::ReadWriteColumn:       r.isReadWrite = v.toBool(); break;
    case G::SidecarColumn:         r.hasSidecar = v.toBool(); break;
    case G::OrientationOffsetColumn: r.orientationOffset = v.toUInt(); break;
    case G::RotationDegreesColumn: r.rotationDegrees = v.toInt(); break;
    case G::ShootingInfoColumn:    r.shootingInfoId = mStrings.id(v.toString()); break;
    case G::ErrColumn:             r.err = v.toStringList(); break;
    case G::DevelopColumn:         r.developEdited = v.toBool(); break;
    case G::DevPreviewKeyColumn:   r.devPreviewKeyId = mStrings.id(v.toString()); break;
    case G::AvailabilityColumn:    r.availability = quint8(v.toInt()); break;
    case G::KeywordsColumn:
    case G::KeywordPathsColumn:
    case G::KeywordsAllColumn: {
        QVector<qint32> ids;
        const QStringList list = v.toStringList();
        ids.reserve(list.size());
        for (const QString &s : list) ids.append(mStrings.id(s));
        if (column == G::KeywordsColumn)          r.keywordIds = ids;
        else if (column == G::KeywordPathsColumn) r.keywordPathIds = ids;
        else                                      r.keywordAllIds = ids;
        break;
    }
    default:
        break;
    }
}
