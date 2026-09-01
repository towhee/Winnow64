#include "Datamodel/imagerow.h"

/*
    The column <-> field mapping. This is the seam that lets the storage change
    without any view, delegate or saved column-width setting changing with it:
    G::dataModelColumns stays the public address space, and this switch is the
    one place that knows how a column is actually held.
*/

bool RowStore::covers(int column, int role)
{
    /*  The path lives on PathColumn under G::PathRole, NOT under EditRole --
        addFileDataForRow never sets EditRole there, so the item is UNSET and
        data() must keep returning an invalid QVariant for it. Claiming the
        column at EditRole made the store answer "" where the model answered
        nothing, which is a different value to anything that tests isValid(). */
    if (column == G::PathColumn) return role == G::PathRole;

    if (role != Qt::EditRole && role != Qt::DisplayRole) return false;

    switch (column) {
    case G::PathColumn:
    case G::NameColumn:
    case G::FolderNameColumn:
    case G::TypeColumn:
    case G::ByteSizeColumn:
    case G::CreatedColumn:
    case G::ModifiedColumn:
    case G::YearColumn:
    case G::DayColumn:
    case G::RatingColumn:
    case G::LabelColumn:
    case G::PickColumn:
    case G::TitleColumn:
    case G::CreatorColumn:
    case G::CopyrightColumn:
    case G::CameraMakeColumn:
    case G::CameraModelColumn:
    case G::LensColumn:
    case G::ISOColumn:
    case G::ApertureColumn:
    case G::ShutterspeedColumn:
    case G::FocalLengthColumn:
    case G::DimensionsColumn:
    case G::MegaPixelsColumn:
    case G::GPSCoordColumn:
    case G::KeywordsColumn:
    case G::KeywordPathsColumn:
    case G::KeywordsAllColumn:
    case G::VideoColumn:
    case G::MetadataStatusColumn:
    case G::IconLoadedColumn:
    case G::SearchTextColumn:
    case G::CompareColumn:
    case G::RowNumberColumn:
        return true;
    /*  NOT COVERED, and the reason is a defect in the model rather than a gap
        here: these three columns hold a DIFFERENT TYPE depending on which code
        path wrote them last, so a store with one type per column cannot
        reproduce them and must not pretend to.

          Search   "false" (QString) at row creation, then m.isSearch and
                   SearchTerms::matches() (bool) once metadata arrives
          Width    QString::number(m.width) from addMetadataForItem, but an
          Height   int from Image/thumb.cpp via setValDm

        An unstable column type is worth fixing on its own account -- a delegate
        or a comparison that branches on the type behaves differently depending
        on load ORDER, which is not a property anything should depend on. They
        stay on the items until that is settled; see "The Row Store" in
        Documentation.txt. */
    case G::SearchColumn:
    case G::WidthColumn:
    case G::HeightColumn:
    default:
        return false;
    }
}

QVariant RowStore::value(int row, int column) const
{
    if (!contains(row)) return QVariant();
    const ImageRow &r = mRows.at(row);
    switch (column) {
    case G::PathColumn:            return r.path;
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
    case G::WidthColumn:           return mStrings.value(r.widthId);
    case G::HeightColumn:          return mStrings.value(r.heightId);
    case G::DimensionsColumn:      return mStrings.value(r.dimensionsId);
    case G::MegaPixelsColumn:      return mStrings.value(r.megaPixelsId);
    case G::GPSCoordColumn:        return mStrings.value(r.gpsId);
    case G::CompareColumn:         return r.isCompare;
    case G::SearchColumn:          return mStrings.value(r.searchId);
    case G::SearchTextColumn:      return r.searchText;
    case G::RowNumberColumn:       return r.rowNumber;
    case G::VideoColumn:           return r.isVideo;
    case G::IconLoadedColumn:      return r.iconLoaded;
    case G::MetadataStatusColumn:  return int(r.metaStatus);
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

void RowStore::setValue(int row, int column, const QVariant &v)
{
    if (!contains(row)) return;
    ImageRow &r = mRows[row];
    switch (column) {
    case G::PathColumn:            r.path = v.toString(); break;
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
    case G::WidthColumn:           r.widthId = mStrings.id(v.toString()); break;
    case G::HeightColumn:          r.heightId = mStrings.id(v.toString()); break;
    case G::DimensionsColumn:      r.dimensionsId = mStrings.id(v.toString()); break;
    case G::MegaPixelsColumn:      r.megaPixelsId = mStrings.id(v.toString()); break;
    case G::GPSCoordColumn:        r.gpsId = mStrings.id(v.toString()); break;
    case G::CompareColumn:         r.isCompare = v.toBool(); break;
    case G::SearchColumn:          r.searchId = mStrings.id(v.toString()); break;
    case G::SearchTextColumn:      r.searchText = v.toString(); break;
    case G::RowNumberColumn:       r.rowNumber = v.toInt(); break;
    case G::VideoColumn:           r.isVideo = v.toBool(); break;
    case G::IconLoadedColumn:      r.iconLoaded = v.toBool(); break;
    case G::MetadataStatusColumn:  r.metaStatus = quint8(v.toInt()); break;
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
