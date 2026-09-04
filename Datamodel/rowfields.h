#ifndef ROWFIELDS_H
#define ROWFIELDS_H

/*
    THE FIELDS OF AN IMAGE, DECLARED ONCE.

    The same set of facts about an image is currently written out FOUR times, by
    hand, and they must agree:

      1. G::dataModelColumns          (Main/global.h)      -- the model's columns
      2. the datamodel cells          (DataModel::addFileDataForRow / addMetadataForItem)
      3. the catalog's image table    (Cache/cachedb.cpp)  -- the persistent form
      4. Catalog::categorySql         (Cache/catalog.cpp)  -- how a column is grouped

    That they agree is currently a matter of vigilance, and it has already been
    paid for: tst_catalog::categoryItemsMatchWhatTheDatamodelWrites exists ONLY
    to pin that 3 and 4 still say what 1 and 2 say. This table is the single
    declaration those four are meant to share, so a field added here reaches all
    of them instead of three of them.

    THIS IS THE ANSWER TO "SHOULD THE INDEX AND THE DATAMODEL BE ONE ENTITY".
    Yes -- one SCHEMA and one loader, not one storage engine. The index is the
    persistent form of a row and the model is the in-memory form; making them
    share a field list is what makes that true in the code rather than in the
    comments.

    -------------------------------------------------------------------------
    RESIDENT vs SCRATCH is the other distinction this table draws, and it is
    what makes a 250,000-row catalog fit in memory.

    RESIDENT fields are the ones every row needs in order to be listed, sorted,
    filtered and drawn. They are held for the whole scope.

    SCRATCH fields -- segment offsets, ICC buffers, decoder id/status/error, the
    cache flags -- are needed only while a decoder or a cache is working on that
    particular image, and they are roughly a third of the current column set.
    They belong in a side table keyed by row, populated for the handful of rows
    in flight. ImageCache's own bookkeeping already moved out of the model on
    exactly this reasoning (see "Worker Threads and the Model" in
    Documentation.txt); this is the same argument applied to the rest.

    -------------------------------------------------------------------------
    MEASURED, NOT ASSUMED. The plan this table serves rests on one number, so it
    was measured on a prototype before any of DataModel was rewritten, at 5,000
    and 20,000 rows (the harness is disposable; the numbers are in
    Documentation.txt under "The Row Store"):

      QStandardItemModel, 92 columns   19,900 bytes/row    4.6 GB at 250k
      packed struct + interning           573 bytes/row    137 MB at 250k
                                                           (34x)

    The 573 is the PESSIMISTIC end -- every row given a title and five keywords.
    A sparse row (neither) measured 413 bytes, so a real library lands between
    98 MB and 137 MB. Storing the full path costs ~190 of those bytes; it is
    kept anyway, because the path is the identity key that icd, fPathRow and the
    proxy snapshot all use, and composing it per access would trade 45 MB for an
    allocation on every hot path.

    -------------------------------------------------------------------------
    HOW TO USE IT. WINNOW_ROW_FIELDS is an X-macro: define WF and include it.
    Fields are declared as

        WF(enumName, cppType, sqlName, sqlType, kind)

    where kind is one of Resident / Scratch / Derived. Derived fields are
    computed from others (Year, Month and Day come from captured) and exist as columns
    because the Filters panel groups on them -- they are not stored twice.
*/

/* kind */
#define WF_RESIDENT 0
#define WF_SCRATCH  1
#define WF_DERIVED  2

/*  The resident set. Order follows G::dataModelColumns so the two can be
    checked against each other mechanically (see tests/unit/tst_rowfields.cpp).
    sqlName empty means the field is not persisted in the catalog. */
#define WINNOW_ROW_FIELDS(WF)                                                       \
    /*   enum name         C++ type        sql name       sql type   kind      */   \
    WF(Path,               QString,        "path",        "TEXT",    WF_RESIDENT)   \
    WF(Name,               QString,        "filename",    "TEXT",    WF_RESIDENT)   \
    WF(FolderName,         QString,        "folder",      "TEXT",    WF_RESIDENT)   \
    WF(Type,               QString,        "ext",         "TEXT",    WF_RESIDENT)   \
    WF(ByteSize,           qint64,         "srcsize",     "INTEGER", WF_RESIDENT)   \
    WF(Modified,           qint64,         "srcmtime",    "INTEGER", WF_RESIDENT)   \
    WF(Created,            qint64,         "captured",    "INTEGER", WF_RESIDENT)   \
    WF(Year,               QString,        "",            "",        WF_DERIVED)    \
    WF(Month,              QString,        "",            "",        WF_DERIVED)    \
    WF(Day,                QString,        "",            "",        WF_DERIVED)    \
    WF(Rating,             quint8,         "rating",      "INTEGER", WF_RESIDENT)   \
    WF(Label,              QString,        "label",       "TEXT",    WF_RESIDENT)   \
    WF(Pick,               quint8,         "pick",        "INTEGER", WF_RESIDENT)   \
    WF(Title,              QString,        "title",       "TEXT",    WF_RESIDENT)   \
    WF(Creator,            QString,        "creator",     "TEXT",    WF_RESIDENT)   \
    WF(Copyright,          QString,        "copyright",   "TEXT",    WF_RESIDENT)   \
    WF(CameraMake,         qint32,         "make",        "TEXT",    WF_RESIDENT)   \
    WF(CameraModel,        qint32,         "model",       "TEXT",    WF_RESIDENT)   \
    WF(Lens,               qint32,         "lens",        "TEXT",    WF_RESIDENT)   \
    WF(Iso,                qint32,         "iso",         "INTEGER", WF_RESIDENT)   \
    WF(Aperture,           float,          "aperture",    "REAL",    WF_RESIDENT)   \
    WF(ShutterSpeed,       float,          "shutter",     "REAL",    WF_RESIDENT)   \
    WF(FocalLength,        float,          "focallength", "REAL",    WF_RESIDENT)   \
    WF(Width,              qint32,         "width",       "INTEGER", WF_RESIDENT)   \
    WF(Height,             qint32,         "height",      "INTEGER", WF_RESIDENT)   \
    WF(GPSCoord,           QString,        "gpscoord",    "TEXT",    WF_RESIDENT)   \
    WF(Keywords,           QStringList,    "",            "",        WF_RESIDENT)   \
    WF(KeywordPaths,       QStringList,    "",            "",        WF_RESIDENT)   \
    WF(KeywordsAll,        QStringList,    "",            "",        WF_RESIDENT)   \
    WF(Video,              bool,           "",            "",        WF_RESIDENT)   \
    WF(MetadataStatus,     quint8,         "",            "",        WF_RESIDENT)   \
    WF(IconLoaded,         bool,           "",            "",        WF_RESIDENT)   \
    /* --- scratch: alive only while a decoder or a cache is on this row --- */     \
    WF(OffsetFull,         qint64,         "",            "",        WF_SCRATCH)    \
    WF(LengthFull,         qint64,         "",            "",        WF_SCRATCH)    \
    WF(OffsetThumb,        qint64,         "",            "",        WF_SCRATCH)    \
    WF(LengthThumb,        qint64,         "",            "",        WF_SCRATCH)    \
    WF(XmpSegmentOffset,   qint64,         "",            "",        WF_SCRATCH)    \
    WF(XmpSegmentLength,   qint64,         "",            "",        WF_SCRATCH)    \
    WF(ICCSegmentOffset,   qint64,         "",            "",        WF_SCRATCH)    \
    WF(ICCSegmentLength,   qint64,         "",            "",        WF_SCRATCH)    \
    WF(ICCBuf,             QByteArray,     "",            "",        WF_SCRATCH)    \
    WF(IsBigEndian,        bool,           "",            "",        WF_SCRATCH)    \
    WF(Ifd0Offset,         qint64,         "",            "",        WF_SCRATCH)    \
    WF(SamplesPerPixel,    qint32,         "",            "",        WF_SCRATCH)

/*  Field ids, generated from the table. NOT a replacement for
    G::dataModelColumns -- that stays the public address space that every view,
    delegate and saved column-width setting is keyed on. This enum is the
    STORAGE side, and RowFields::columnFor() is the one mapping between them. */
namespace RowFields {
    enum Field {
#define WF_ENUM(name, type, sql, sqltype, kind) name,
        WINNOW_ROW_FIELDS(WF_ENUM)
#undef WF_ENUM
        FieldCount
    };

    constexpr int fieldCount = FieldCount;
}

#endif // ROWFIELDS_H
