#include "metadata.h"

Metadata::Metadata()
{
    {
    #ifdef ISDEBUG
    qDebug() << GData::t.restart() << "Metadata::Metadata";
    #endif
    }
    // some initialization
    initExifHash();
    initIfdHash();
    report = false;
}

 ulong Metadata::get2(QByteArray c)
{
     {
//     #ifdef ISDEBUG
//     qDebug() << GData::t.restart() << "Metadata::get2";
//     #endif
     }
     if (order == 0x4D4D) return static_cast<unsigned int>
             ((c[0]&0xFF) << 8 | (c[1]&0xFF));
     else return static_cast<unsigned int>
             ((c[0]&0xFF) | (c[1]&0xFF) << 8);
}

ulong Metadata::get4(QByteArray c)
{
    {
//    #ifdef ISDEBUG
//    qDebug() << GData::t.restart() << "Metadata::get4";
//    #endif
    }
    if (order == 0x4D4D) return static_cast<unsigned long>
            ((c[0]&0xFF) << 24 | (c[1]&0xFF) << 16 | (c[2]&0xFF) << 8 | (c[3]&0xFF));
    else return static_cast<unsigned long>
            ((c[0]&0xFF) | (c[1]&0xFF) << 8 | (c[2]&0xFF) << 16 | (c[3]&0xFF) << 24);
}

void Metadata::initExifHash()
{
    {
    #ifdef ISDEBUG
    qDebug() << GData::t.restart() << "Metadata::initExifHash";
    #endif
    }
    exifHash[293] = "ExposureTime";
    exifHash[294] = "FNumber";
    exifHash[295] = "ExposureProgram";
    exifHash[296] = "ISOSpeedRatings";
    exifHash[297] = "ExifVersion";
    exifHash[304] = "DateTimeOriginal";
    exifHash[305] = "DateTimeDigitized";
    exifHash[306] = "ComponentConfiguration";
    exifHash[307] = "CompressedBitsPerPixel";
    exifHash[308] = "ShutterSpeedValue";
    exifHash[309] = "ApertureValue";
    exifHash[310] = "BrightnessValue";
    exifHash[311] = "ExposureBiasValue";
    exifHash[312] = "MaxApertureValue";
    exifHash[313] = "SubjectDistance";
    exifHash[320] = "MeteringMode";
    exifHash[321] = "LightSource";
    exifHash[322] = "Flash";
    exifHash[323] = "FocalLength";
    exifHash[324] = "MakerNote";
    exifHash[325] = "UserComment";
    exifHash[326] = "FlashPixVersion";
    exifHash[327] = "ColorSpace";
    exifHash[328] = "ExifImageWidth";
    exifHash[329] = "ExifImageHeight";
    exifHash[336] = "RelatedSoundFile";
    exifHash[337] = "ExifInteroperabilityOffset";
    exifHash[338] = "FocalPlaneXResolution";
    exifHash[339] = "FocalPlaneYResolution";
    exifHash[340] = "FocalPlaneResolutionUnit";
    exifHash[341] = "SensingMethod";
    exifHash[342] = "FileSource";
    exifHash[343] = "SceneType";
    exifHash[33434] = "ExposureTime";
    exifHash[33437] = "FNumber";
    exifHash[34850] = "ExposureProgram";
    exifHash[34852] = "SpectralSensitivity";
    exifHash[34855] = "ISOSpeedRatings";
    exifHash[34856] = "OECF";
    exifHash[36864] = "ExifVersion";
    exifHash[36867] = "DateTimeOriginal";
    exifHash[36868] = "DateTimeDigitized";
    exifHash[37121] = "ComponentsConfiguration";
    exifHash[37122] = "CompressedBitsPerPixel";
    exifHash[37377] = "ShutterSpeedValue";
    exifHash[37378] = "ApertureValue";
    exifHash[37379] = "BrightnessValue";
    exifHash[37380] = "ExposureBiasValue";
    exifHash[37381] = "MaxApertureValue";
    exifHash[37382] = "SubjectDistance";
    exifHash[37383] = "MeteringMode";
    exifHash[37384] = "LightSource";
    exifHash[37385] = "Flash";
    exifHash[37386] = "FocalLength";
    exifHash[37396] = "SubjectArea";
    exifHash[37500] = "MakerNote";
    exifHash[37510] = "UserComment";
    exifHash[37520] = "SubsecTime";
    exifHash[37521] = "SubsecTimeOriginal";
    exifHash[37522] = "SubsecTimeDigitized";
    exifHash[40960] = "FlashpixVersion";
    exifHash[40961] = "ColorSpace";
    exifHash[40962] = "PixelXDimension";
    exifHash[40963] = "PixelYDimension";
    exifHash[40964] = "RelatedSoundFile";
    exifHash[41483] = "FlashEnergy";
    exifHash[41484] = "SpatialFrequencyResponse";
    exifHash[41486] = "FocalPlaneXResolution";
    exifHash[41487] = "FocalPlaneYResolution";
    exifHash[41488] = "FocalPlaneResolutionUnit";
    exifHash[41492] = "SubjectLocation";
    exifHash[41493] = "ExposureIndex";
    exifHash[41495] = "SensingMethod";
    exifHash[41728] = "FileSource";
    exifHash[41729] = "SceneType";
    exifHash[41730] = "CFAPattern";
    exifHash[41985] = "CustomRendered";
    exifHash[41986] = "ExposureMode";
    exifHash[41987] = "WhiteBalance";
    exifHash[41988] = "DigitalZoomRatio";
    exifHash[41989] = "FocalLengthIn35mmFilm";
    exifHash[41990] = "SceneCaptureType";
    exifHash[41991] = "GainControl";
    exifHash[41992] = "Contrast";
    exifHash[41993] = "Saturation";
    exifHash[41994] = "Sharpness";
    exifHash[41995] = "DeviceSettingDescription";
    exifHash[41996] = "SubjectDistanceRange";
    exifHash[42016] = "ImageUniqueID";
}

void Metadata::initIfdHash()
{
    {
    #ifdef ISDEBUG
    qDebug() << GData::t.restart() << "Metadata::initIfdHash";
    #endif
    }
    ifdHash[254] = "NewSubfileType";
    ifdHash[255] = "SubfileType";
    ifdHash[256] = "ImageWidth";
    ifdHash[257] = "ImageLength";
    ifdHash[258] = "BitsPerSample";
    ifdHash[259] = "Compression";
    ifdHash[262] = "PhotometricInterpretation";
    ifdHash[263] = "Threshholding";
    ifdHash[264] = "CellWidth";
    ifdHash[265] = "CellLength";
    ifdHash[266] = "FillOrder";
    ifdHash[269] = "DocumentName";
    ifdHash[270] = "ImageDescription";
    ifdHash[271] = "Make";
    ifdHash[272] = "Model";
    ifdHash[273] = "StripOffsets";
    ifdHash[274] = "Orientation";
    ifdHash[275] = "XResolution";
    ifdHash[276] = "YResolution";
    ifdHash[277] = "ResolutionUnit";
    ifdHash[278] = "RowsPerStrip";      // at least for canon files
    ifdHash[279] = "StripByteCount";    // at least for canon files
    ifdHash[280] = "WhitePoint";
    ifdHash[281] = "PrimaryChromaticities";
    ifdHash[282] = "XResolution";
    ifdHash[283] = "YResolution";
    ifdHash[284] = "PlanarConfiguration";
    ifdHash[285] = "PageName";
    ifdHash[286] = "XPosition";
    ifdHash[287] = "YPosition";
    ifdHash[288] = "YCbCrCoefficients";
    ifdHash[289] = "YCbCrPositioning";
    ifdHash[290] = "ReferenceBlackWhite";
    ifdHash[291] = "Copyright";
    ifdHash[292] = "ExifOffset";
    ifdHash[293] = "T6Options";
    ifdHash[296] = "ResolutionUnit";
    ifdHash[297] = "PageNumber";
    ifdHash[301] = "TransferFunction";
    ifdHash[305] = "Software";
    ifdHash[306] = "DateTime";
    ifdHash[315] = "Artist";
    ifdHash[316] = "HostComputer";
    ifdHash[317] = "Predictor";
    ifdHash[318] = "WhitePoint";
    ifdHash[319] = "PrimaryChromaticities";
    ifdHash[320] = "ColorMap";
    ifdHash[321] = "HalftoneHints";
    ifdHash[322] = "TileWidth";
    ifdHash[323] = "TileLength";
    ifdHash[324] = "TileOffsets";
    ifdHash[325] = "TileByteCounts";
    ifdHash[326] = "BadFaxLines";
    ifdHash[327] = "CleanFaxData";
    ifdHash[328] = "ConsecutiveBadFaxLines";
    ifdHash[330] = "SubIFDs";
    ifdHash[332] = "InkSet";
    ifdHash[333] = "InkNames";
    ifdHash[334] = "NumberOfInks";
    ifdHash[336] = "DotRange";
    ifdHash[337] = "TargetPrinter";
    ifdHash[338] = "ExtraSamples";
    ifdHash[339] = "SampleFormat";
    ifdHash[340] = "SMinSampleValue";
    ifdHash[341] = "SMaxSampleValue";
    ifdHash[342] = "TransferRange";
    ifdHash[343] = "ClipPath";
    ifdHash[344] = "ImageWidth";
    ifdHash[345] = "ImageLength";
    ifdHash[346] = "Indexed";
    ifdHash[347] = "JPEGTables";
    ifdHash[351] = "OPIProxy";
    ifdHash[352] = "BitsPerSample";
    ifdHash[353] = "Compression";
    ifdHash[354] = "PhotometricInterpretation";
    ifdHash[355] = "StripOffsets";
    ifdHash[356] = "SamplesPerPixel";
    ifdHash[357] = "RowsPerStrip";
    ifdHash[358] = "StripByteConunts";
    ifdHash[359] = "XResolution";
    ifdHash[360] = "YResolution";
    ifdHash[361] = "PlanarConfiguration";
    ifdHash[368] = "ResolutionUnit";
    ifdHash[369] = "JpegIFOffset";
    ifdHash[370] = "JpegIFByteCount";
    ifdHash[371] = "YCbCrCoefficients";
    ifdHash[372] = "YCbCrSubSampling";
    ifdHash[373] = "YCbCrPositioning";
    ifdHash[374] = "ReferenceBlackWhite";
    ifdHash[400] = "GlobalParametersIFD";
    ifdHash[401] = "ProfileType";
    ifdHash[402] = "FaxProfile";
    ifdHash[403] = "CodingMethods";
    ifdHash[404] = "VersionYear";
    ifdHash[405] = "ModeNumber";
    ifdHash[433] = "Decode";
    ifdHash[434] = "DefaultImageColor";
    ifdHash[512] = "JPEGProc";
    ifdHash[513] = "JPEGInterchangeFormat";
    ifdHash[514] = "JPEGInterchangeFormatLength";
    ifdHash[515] = "JPEGRestartInterval";
    ifdHash[517] = "JPEGLosslessPredictors";
    ifdHash[518] = "JPEGPointTransforms";
    ifdHash[519] = "JPEGQTables";
    ifdHash[520] = "JPEGDCTables";
    ifdHash[521] = "JPEGACTables";
    ifdHash[529] = "YCbCrCoefficients";
    ifdHash[530] = "YCbCrSubSampling";
    ifdHash[531] = "YCbCrPositioning";
    ifdHash[532] = "ReferenceBlackWhite";
    ifdHash[559] = "StripRowCounts";
    ifdHash[700] = "XMP";
    ifdHash[32781] = "ImageID";
    ifdHash[32932] = "Wang Annotation";
    ifdHash[33421] = "CFARepeatPatternDim";
    ifdHash[33422] = "CFAPattern";
    ifdHash[33423] = "BatteryLevel";
    ifdHash[33432] = "Copyright";
    ifdHash[33445] = "MD FileTag";
    ifdHash[33446] = "MD ScalePixel";
    ifdHash[33447] = "MD ColorTable";
    ifdHash[33448] = "MD LabName";
    ifdHash[33449] = "MD SampleInfo";
    ifdHash[33450] = "MD PrepDate";
    ifdHash[33451] = "MD PrepTime";
    ifdHash[33452] = "MD FileUnits";
    ifdHash[33550] = "ModelPixelScaleTag";
    ifdHash[33723] = "IPTC/NAA";
    ifdHash[33918] = "INGR Packet Data Tag";
    ifdHash[33919] = "INGR Flag Registers";
    ifdHash[33920] = "IrasB Transformation Matrix";
    ifdHash[33922] = "ModelTiepointTag";
    ifdHash[34264] = "ModelTransformationTag";
    ifdHash[34377] = "Photoshop";
    ifdHash[34665] = "Exif IFD";
    ifdHash[34675] = "ICC Profile";
    ifdHash[34732] = "ImageLayer";
    ifdHash[34735] = "GeoKeyDirectoryTag";
    ifdHash[34736] = "GeoDoubleParamsTag";
    ifdHash[34737] = "GeoAsciiParamsTag";
    ifdHash[34852] = "SpectralSensitivity";
    ifdHash[34853] = "GPS IFD";
    ifdHash[34856] = "OECF";
    ifdHash[34857] = "Interlace";
    ifdHash[34858] = "TimeZoneOffset";
    ifdHash[34859] = "SelfTimerMode";
    ifdHash[34908] = "HylaFAX FaxRecvParams";
    ifdHash[34909] = "HylaFAX FaxSubAddress";
    ifdHash[34910] = "HylaFAX FaxRecvTime";
    ifdHash[37387] = "FlashEnergy";
    ifdHash[37388] = "SpatialFrequencyResponse";
    ifdHash[37389] = "Noise";
    ifdHash[37393] = "ImageNumber";
    ifdHash[37394] = "SecurityClassification";
    ifdHash[37395] = "ImageHistory";
    ifdHash[37396] = "SubjectLocation";
    ifdHash[37397] = "ExposureIndex";
    ifdHash[37398] = "TIFF/EPStandardID";
    ifdHash[37520] = "SubSecTime";
    ifdHash[37521] = "SubSecTimeOriginal";
    ifdHash[37522] = "SubSecTimeDigitized";
    ifdHash[37724] = "ImageSourceData";
    ifdHash[40965] = "Interoperability IFD";
    ifdHash[41483] = "FlashEnergy";
    ifdHash[41484] = "SpatialFrequencyResponse";
    ifdHash[41492] = "SubjectLocation";
    ifdHash[41493] = "ExposureIndex";
    ifdHash[41730] = "CFAPattern";
    ifdHash[42112] = "GDAL_METADATA";
    ifdHash[42113] = "GDAL_NODATA";
    ifdHash[50215] = "Oce Scanjob Description";
    ifdHash[50216] = "Oce Application Selector";
    ifdHash[50217] = "Oce Identification Number";
    ifdHash[50218] = "Oce ImageLogic Characteristics";
    ifdHash[50706] = "DNGVersion";
    ifdHash[50707] = "DNGBackwardVersion";
    ifdHash[50708] = "UniqueCameraModel";
    ifdHash[50709] = "LocalizedCameraModel";
    ifdHash[50710] = "CFAPlaneColor";
    ifdHash[50711] = "CFALayout";
    ifdHash[50712] = "LinearizationTable";
    ifdHash[50713] = "BlackLevelRepeatDim";
    ifdHash[50714] = "BlackLevel";
    ifdHash[50715] = "BlackLevelDeltaH";
    ifdHash[50716] = "BlackLevelDeltaV";
    ifdHash[50717] = "WhiteLevel";
    ifdHash[50718] = "DefaultScale";
    ifdHash[50719] = "DefaultCropOrigin";
    ifdHash[50720] = "DefaultCropSize";
    ifdHash[50721] = "ColorMatrix1";
    ifdHash[50722] = "ColorMatrix2";
    ifdHash[50723] = "CameraCalibration1";
    ifdHash[50724] = "CameraCalibration2";
    ifdHash[50725] = "ReductionMatrix1";
    ifdHash[50726] = "ReductionMatrix2";
    ifdHash[50727] = "AnalogBalance";
    ifdHash[50728] = "AsShotNeutral";
    ifdHash[50729] = "AsShotWhiteXY";
    ifdHash[50730] = "BaselineExposure";
    ifdHash[50731] = "BaselineNoise";
    ifdHash[50732] = "BaselineSharpness";
    ifdHash[50733] = "BayerGreenSplit";
    ifdHash[50734] = "LinearResponseLimit";
    ifdHash[50735] = "CameraSerialNumber";
    ifdHash[50736] = "LensInfo";
    ifdHash[50737] = "ChromaBlurRadius";
    ifdHash[50738] = "AntiAliasStrength";
    ifdHash[50740] = "DNGPrivateData";
    ifdHash[50741] = "MakerNoteSafety";
    ifdHash[50778] = "CalibrationIlluminant1";
    ifdHash[50779] = "CalibrationIlluminant2";
    ifdHash[50780] = "BestQualityScale";
    ifdHash[50784] = "Alias Layer Metadata";
}

void Metadata::reportMetadata()
{
    {
    #ifdef ISDEBUG
    qDebug() << GData::t.restart() << "Metadata::reportMetadata";
    #endif
    }
    qDebug() << " ";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "offsetFullJPG"
              << std::setw(16) << std::setfill(' ') << std::left << offsetFullJPG
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "lengthFullJPG"
              << std::setw(16) << std::setfill(' ') << std::left << lengthFullJPG
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "offsetThumbJPG"
              << std::setw(16) << std::setfill(' ') << std::left << offsetThumbJPG
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "lengthThumbJPG"
              << std::setw(16) << std::setfill(' ') << std::left << lengthThumbJPG
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "offsetSmallJPG"
              << std::setw(16) << std::setfill(' ') << std::left << offsetSmallJPG
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "lengthSmallJPG"
              << std::setw(16) << std::setfill(' ') << std::left << lengthSmallJPG
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "orientation"
              << std::setw(16) << std::setfill(' ') << std::left << orientation
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "width"
              << std::setw(16) << std::setfill(' ') << std::left << width
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "height"
              << std::setw(16) << std::setfill(' ') << std::left << height
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "dateTime"
              << std::setw(16) << std::setfill(' ') << std::left << dateTime.toStdString()
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "model"
              << std::setw(16) << std::setfill(' ') << std::left << model.toStdString()
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "exposureTime"
              << std::setw(16) << std::setfill(' ') << std::left << exposureTime.toStdString()
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "aperture"
              << std::setw(16) << std::setfill(' ') << std::left << aperture.toStdString()
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "ISO"
              << std::setw(16) << std::setfill(' ') << std::left << ISO.toStdString()
              << "\n";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "focalLength"
              << std::setw(16) << std::setfill(' ') << std::left << focalLength.toStdString()
              << "\n";

//    qDebug() << "focalLength35mm" << focalLength35mm;
}

void Metadata::reportIfdDataHash()
{
    std::cout << "  Offset  tagId  tagType  tagCount  tagValue   tagDescription\n";
    for (ifdIter=ifdDataHash.begin(); ifdIter!=ifdDataHash.end(); ++ifdIter) {
         uint tagId = ifdIter.key();
         uint tagType = ifdIter.value().tagType;
         ulong tagCount = ifdIter.value().tagCount;
         ulong tagValue = ifdIter.value().tagValue;
         std::cout << std::setw(8) << std::setfill(' ') << std::right << "N/A"
                   << std::setw(7) << std::setfill(' ') << std::right << tagId
                   << std::setw(9) << std::setfill(' ') << std::right << tagType
                   << std::setw(10) << std::setfill(' ') << std::right << tagCount
                   << std::setw(10) << std::setfill(' ') << std::right << tagValue
                   << "  \n";
    }
}

float Metadata::getReal(long offset)
{
    {
//    #ifdef ISDEBUG
//    qDebug() << GData::t.restart() << "Metadata::getReal";
//    #endif
    }
    file.seek(offset);
    ulong a = get4(file.read(4));
    ulong b = get4(file.read(4));
    if (b==0) return 0;
    float x = (float)a/b;
    return x;
}

ulong Metadata::readIFD(QString hdr, ulong offset)
{
    {
//    #ifdef ISDEBUG
//    qDebug() << GData::t.restart() << "Metadata::readIFD";
//    #endif
    }
    uint tagId, tagType = 0;
    long tagCount, tagValue = 0;
    IFDData ifdData;
    ifdDataHash.clear();

    file.seek(offset);
    int tags = get2(file.read(2));

    // iterate through IFD0, looking for the subIFD tag
    if (report) {
        qDebug() << " ";
        qDebug() << "*************************" << hdr << "*************************";
        std::cout << "  Offset  tagId  tagType  tagCount  tagValue   tagDescription\n";
    }
    QString pos;
    QString tagDescription;
    for (int i=0; i<tags; i++){
        tagId = get2(file.read(2));
        tagType = get2(file.read(2));
        tagCount = get4(file.read(4));
        tagValue = get4(file.read(4));

        ifdData.tagType = tagType;
        ifdData.tagCount = tagCount;
        ifdData.tagValue = tagValue;

        ifdDataHash.insert(tagId, ifdData);

        if (hdr.left(3) == "Sub" || hdr.left(3) == "IFD")
            (ifdHash.contains(tagId))? tagDescription = ifdHash.value(tagId)
                : tagDescription = "Undefined tag";
        if (hdr == "IFD Exif")
            (exifHash.contains(tagId))? tagDescription = exifHash.value(tagId)
                : tagDescription = "Undefined tag";

        if (report) {
            pos = QString::number(file.pos(), 16).toUpper();
            std::cout << std::setw(8) << std::setfill(' ') << std::right << pos.toStdString()
                      << std::setw(7) << std::setfill(' ') << std::right << tagId
                      << std::setw(9) << std::setfill(' ') << std::right << tagType
                      << std::setw(10) << std::setfill(' ') << std::right << tagCount
                      << std::setw(10) << std::setfill(' ') << std::right << tagValue
                      << "  ";
            qDebug() << tagDescription;
        }
    }
    ulong nextIFDOffset = get4(file.read(4));
    if (report) qDebug() << "   "
             << QString::number(file.pos(), 16).toUpper()
             << "nextIFDOffset ="
             << QString::number(nextIFDOffset, 16).toUpper();
    return(nextIFDOffset);
}

QList<ulong> Metadata::getSubIfdOffsets(ulong subIFDaddr)
{
    {
//    #ifdef ISDEBUG
//    qDebug() << GData::t.restart() << "Metadata::getSubIfdOffsets";
//    #endif
    }
    QList<ulong> offsets;
    file.seek(subIFDaddr);
    for (int i=0; i<3; i++) {
        offsets.append(get4(file.read(4)));
    }
    return offsets;
}

ulong Metadata::getExifOffset(ulong offsetIfd0)
{
    {
//    #ifdef ISDEBUG
//    qDebug() << GData::t.restart() << "Metadata::getExifOffset";
//    #endif
    }
    ulong tagId, tagType = 0;
    ulong tagCount, tagValue, exifIFDaddr = 0;
    file.seek(offsetIfd0);
    ulong tags = get2(file.read(2));
    for (ulong i=0; i<tags; i++){
        tagId = get2(file.read(2));
        tagType = get2(file.read(2));
        tagCount = get4(file.read(4));
        tagValue = get4(file.read(4));
        if (tagId == 34665) {
            exifIFDaddr = tagValue;
            break;
        }
    }
    return tagValue;
}

QString Metadata::getString(ulong offset, ulong length)
{
    {
//    #ifdef ISDEBUG
//    qDebug() << GData::t.restart() << "Metadata::getString";
//    #endif
    }
    file.seek(offset);
    return(file.read(length));
}

void Metadata::nikon()
{
    {
    #ifdef ISDEBUG
    qDebug() << GData::t.restart() << "Metadata::nikon";
    #endif
    }
    file.open(QIODevice::ReadOnly);
    // get endian
    order = get2(file.read(4));
    // get offset to first IFD and read it
    ulong offsetIfd0 = get4(file.read(4));

    ulong nextIFDOffset = readIFD("IFD0", offsetIfd0);

/*
    // test working
    QHashIterator<uint, IFDData> i(ifdDataHash);
    while (i.hasNext()) {
        i.next();
        qDebug() << i.key() << ":"
                 << i.value().tagType
                 << i.value().tagCount
                 << i.value().tagValue;
    }
    */

    // pull data reqd from IFD0
    (ifdDataHash.contains(273))
        ? offsetThumbJPG = ifdDataHash.value(273).tagValue
        : offsetThumbJPG = 0;
    (ifdDataHash.contains(279))
        ? lengthThumbJPG = ifdDataHash.value(279).tagValue
        : lengthThumbJPG = 0;
    (ifdDataHash.contains(272))
        ? model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount)
        : model = "Unknown";
    (ifdDataHash.contains(274))
        ? orientation = ifdDataHash.value(274).tagValue
        : orientation = 1;
    (ifdDataHash.contains(272))
        ? dateTime = getString(ifdDataHash.value(306).tagValue, ifdDataHash.value(306).tagCount)
        : dateTime = "Unknown";


    // NIkon provides an offset in IFD0 to the offsets for
    // all the subIFDs
    // get the offsets for the subIFD and read them
    QList<ulong> ifdOffsets;
    if(ifdDataHash.contains(330)) {
        ifdOffsets = getSubIfdOffsets(ifdDataHash.value(330).tagValue);

        // SubIFD1 contains full size jpg offset and length
        QString hdr = "SubIFD0";
        readIFD(hdr, ifdOffsets[0]);

        // pull data reqd from SubIFD1
        (ifdDataHash.contains(513))
            ? offsetFullJPG = ifdDataHash.value(513).tagValue
            : offsetFullJPG = 0;
        (ifdDataHash.contains(514))
            ? lengthFullJPG = ifdDataHash.value(514).tagValue
            : lengthFullJPG = 0;

        // pull data reqd from SubIFD2
        (ifdDataHash.contains(513))
            ? width = ifdDataHash.value(256).tagValue
            : width = 0;
        (ifdDataHash.contains(514))
            ? height = ifdDataHash.value(257).tagValue
            : height = 0;

        // SubIFD3 contains small size jpg offset and length

        hdr = "SubIFD3";
        readIFD(hdr, ifdOffsets[2]);

        // pull data reqd from SubIFD3
        (ifdDataHash.contains(513))
            ? offsetSmallJPG = ifdDataHash.value(513).tagValue
            : offsetSmallJPG = 0;
        (ifdDataHash.contains(514))
            ? lengthSmallJPG = ifdDataHash.value(514).tagValue
            : lengthSmallJPG = 0;
    }

    // get the offset for ExifIFD and read it
    readIFD("IFD Exif", getExifOffset(offsetIfd0));
    if (ifdDataHash.contains(33434)) {
        float x = getReal(ifdDataHash.value(33434).tagValue);
        if (x<1) {
            uint t = qRound(1/x);
            exposureTime = "1/" + QString::number(t);
        } else {
            uint t = (uint)x;
            exposureTime = QString::number(t);
        }
        exposureTime += " sec";
    } else {
        exposureTime = "Unknown";
    }
    if (ifdDataHash.contains(33437)) {
        float x = getReal(ifdDataHash.value(33437).tagValue);
        aperture = "f/" + QString::number(x, 'f', 1);
    } else {
        aperture = "Unknown";
    }
    if (ifdDataHash.contains(34855)) {
        ISO = "ISO " + QString::number(ifdDataHash.value(34855).tagValue);
    } else {
        ISO = "Unknown";
    }
/*
//    if (ifdDataHash.contains(41989)) {
//        focalLength35mm = QString::number(ifdDataHash.value(41989).tagValue) + "mm";
//    } else {
//        focalLength35mm = "Unknown";
//    }
*/
    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "Unknown";
    }

    if (report) reportMetadata();

}

void Metadata::canon()
{
    {
    #ifdef ISDEBUG
    qDebug() << GData::t.restart() << "Metadata::canon";
    #endif
    }
    file.open(QIODevice::ReadOnly);
    // get endian
    order = get2(file.read(4));
    // is canon always offset 16 to IFD0 ???
    ulong offsetIfd0 = 16;
    ulong nextIFDOffset = readIFD("IFD0", offsetIfd0);

    // pull data reqd from IFD0
    (ifdDataHash.contains(273))
        ? offsetFullJPG = ifdDataHash.value(273).tagValue
        : offsetFullJPG = 0;
    (ifdDataHash.contains(279))
        ? lengthFullJPG = ifdDataHash.value(279).tagValue
        : lengthFullJPG = 0;
    (ifdDataHash.contains(272))
        ? model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount)
        : model = "Unknown";
    (ifdDataHash.contains(274))
        ? orientation = ifdDataHash.value(274).tagValue
        : orientation = 1;
    (ifdDataHash.contains(306))
        ? dateTime = getString(ifdDataHash.value(306).tagValue, ifdDataHash.value(306).tagCount)
        : dateTime = "Unknown";

    if (nextIFDOffset) nextIFDOffset = readIFD("IFD1", nextIFDOffset);

    // pull data reqd from IFD1
    (ifdDataHash.contains(513))
        ? offsetThumbJPG = ifdDataHash.value(513).tagValue
        : offsetThumbJPG = 0;
    (ifdDataHash.contains(514))
        ? lengthThumbJPG = ifdDataHash.value(514).tagValue
        : lengthThumbJPG = 0;

    if (nextIFDOffset) nextIFDOffset = readIFD("IFD2", nextIFDOffset);

    // pull small size jpg from IFD2 (does this work??)
    (ifdDataHash.contains(273))
        ? offsetSmallJPG = ifdDataHash.value(273).tagValue
        : offsetSmallJPG = 0;
    (ifdDataHash.contains(279))
        ? lengthSmallJPG = ifdDataHash.value(279).tagValue
        : lengthSmallJPG = 0;

    if (nextIFDOffset) nextIFDOffset = readIFD("IFD3", nextIFDOffset);

    // get the offset for ExifIFD and read it
    readIFD("IFD Exif", getExifOffset(offsetIfd0));
    // shutter speed
    if (ifdDataHash.contains(33434)) {
        float x = getReal(ifdDataHash.value(33434).tagValue);
        if (x<1) {
            uint t = qRound(1/x);
            exposureTime = "1/" + QString::number(t);
        } else {
            uint t = (uint)x;
            exposureTime = QString::number(t);
        }
        exposureTime += " sec";
    } else {
        exposureTime = "Unknown";
    }
    if (ifdDataHash.contains(40962)) {
        width = ifdDataHash.value(40962).tagValue;
    } else {
        width = 0;
    }
    // height
    if (ifdDataHash.contains(40963)) {
        height = ifdDataHash.value(40963).tagValue;
    } else {
        height = 0;
    }
    // aperture
    if (ifdDataHash.contains(33437)) {
        float x = getReal(ifdDataHash.value(33437).tagValue);
        aperture = "f/" + QString::number(x, 'f', 1);
    } else {
        aperture = "Unknown";
    }
    //ISO
    if (ifdDataHash.contains(34855)) {
        ISO = "ISO " + QString::number(ifdDataHash.value(34855).tagValue);
    } else {
        ISO = "Unknown";
    }
    // focal length
    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "Unknown";
    }

    if (report) reportMetadata();
}

bool Metadata::jpeg()
{
    file.open(QIODevice::ReadOnly);
    order = 0x4D4D;
    ulong startOffset = 0;
    if (get2(file.read(2)) != 0xFFD8) return 0;

    bool foundEndian = false;
    while (!foundEndian) {
        uint a = get2(file.read(2));
        if (a == 0x4949 || a == 0x4D4D) {
            order = a;
            // offsets are from the endian position in JPEGs
            // therefore must adjust all offsets found in tagValue
            startOffset = file.pos() - 2;
            foundEndian = true;
        }
        // add condition to check for EOF
    }
    uint a = get2(file.read(2));  // magic 42
    a = get2(file.read(2));
    ulong offsetIfd0 = a + startOffset;

    // read IFD0
    ulong nextIFDOffset = readIFD("IFD0", offsetIfd0) + startOffset;
    // get Model
    (ifdDataHash.contains(272))
        ? model = getString(ifdDataHash.value(272).tagValue + startOffset,
        ifdDataHash.value(272).tagCount)
        : model = "Unknown";
    // get DateTime
    (ifdDataHash.contains(306))
        ? dateTime = getString(ifdDataHash.value(306).tagValue + startOffset,
        ifdDataHash.value(306).tagCount)
        : dateTime = "Unknown";
    // orientation
    (ifdDataHash.contains(274))
        ? orientation = ifdDataHash.value(274).tagValue
        : orientation = 1;

    // read IFD1 - unfortunately nothing of interest
    if (nextIFDOffset) nextIFDOffset = readIFD("IFD1", nextIFDOffset);

    // get the offset for ExifIFD and read it
    ulong exifOffset = getExifOffset(offsetIfd0) + startOffset;
    readIFD("IFD Exif", exifOffset);
    // get shutter speed
    if (ifdDataHash.contains(33434)) {
        float x = getReal(ifdDataHash.value(33434).tagValue + startOffset);
        if (x<1) {
            uint t = qRound(1/x);
            exposureTime = "1/" + QString::number(t);
        } else {
            uint t = (uint)x;
            exposureTime = QString::number(t);
        }
        exposureTime += " sec";
    } else {
        exposureTime = "Unknown";
    }
    // aperture
    if (ifdDataHash.contains(33437)) {
        float x = getReal(ifdDataHash.value(33437).tagValue + startOffset);
        aperture = "f/" + QString::number(x, 'f', 1);
    } else {
        aperture = "Unknown";
    }
    //ISO
    if (ifdDataHash.contains(34855)) {
        ISO = "ISO " + QString::number(ifdDataHash.value(34855).tagValue);
    } else {
        ISO = "Unknown";
    }
    // focal length
    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue + startOffset);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "Unknown";
    }
    // width
    (ifdDataHash.contains(40962))
        ? width = ifdDataHash.value(40962).tagValue
        : width = 0;
    // height
    (ifdDataHash.contains(40963))
        ? height = ifdDataHash.value(40963).tagValue
        : height = 0;

    if (!width || !height) getDimensions();

    if (report) reportMetadata();
}

void Metadata::clearMetadata()
{
    {
    #ifdef ISDEBUG
    qDebug() << GData::t.restart() << "Metadata::clearMetadata";
    #endif
    }
    offsetFullJPG = 0;
    lengthFullJPG = 0;
    offsetThumbJPG = 0;
    lengthThumbJPG = 0;
    offsetSmallJPG = 0;
    lengthSmallJPG = 0;
    width = 0;
    height = 0;
    orientation = 0;
    dateTime = "";
    model = "";
    exposureTime = "";
    aperture = "";
    ISO = "";
    focalLength = "";

    ifdDataHash.clear();
}

bool Metadata::readMetadata(bool report, const QString &imageFullPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << GData::t.restart() << "Metadata::readMetadata";
    #endif
    }
    QElapsedTimer t;
    t.start();
    clearMetadata();
    file.setFileName(imageFullPath);
    if (report) qDebug() << "File name =" << imageFullPath;
    QFileInfo fileInfo(imageFullPath);
    QString ext = fileInfo.completeSuffix().toLower();
    if (ext == "nef") nikon();
    if (ext == "cr2") canon();
    if (ext == "jpg") jpeg();
    file.close();

    reportMetadata();

//    if (GData::isTimer) qDebug() << "Time to read metadata =" << t.elapsed();
    return true;
}
