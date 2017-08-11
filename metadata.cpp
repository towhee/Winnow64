#include "metadata.h"
#include <QDebug>
#include "global.h"

Metadata::Metadata()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::Metadata";
    #endif
    }
    // some initialization
    initSegCodeHash();
    initExifHash();
    initIfdHash();
    initSupportedFiles();
    report = false;
}

/*
    * At start of file determine endian
    * Read IFD0 (has ref tagid 330 to offset and number of subIFDs
    * Read subIFDs
    * IFD data is stored in ifdDataHash(tagId, IFDData)
    * Walk JPEG markers to find FFC0 (has image width/height)



*/

void Metadata::initSupportedFiles()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::initSupportedFiles";
    #endif
    }
    // add raw file types here as they are supported
//    rawFormats << "arw" << "cr2" << "nef" << "orf";
        rawFormats << "cr2" << "nef" << "orf";

//    supportedFormats << "arw" << "bmp" << "cr2" << "cur" << "dds" << "gif" <<
//    "icns" << "ico" << "jpeg" << "jpg" << "jp2" << "jpe" << "mng" << "nef" <<
//    "orf" << "pbm" << "pgm" << "png" << "ppm" << "svg" << "svgz" << "tga" <<
//    "wbmp" << "webp" << "xbm" << "xpm";

    supportedFormats << "bmp" << "cr2" << "cur" << "dds" << "gif" <<
    "icns" << "ico" << "jpeg" << "jpg" << "jp2" << "jpe" << "mng" << "nef" <<
    "orf" << "pbm" << "pgm" << "png" << "ppm" << "svg" << "svgz" << "tga" <<
    "wbmp" << "webp" << "xbm" << "xpm";
}

void Metadata::initSegCodeHash()
{
    segCodeHash[0xFFC0] = "SOF0";    //Start of Frame 0
    segCodeHash[0xFFC1] = "SOF1";
    segCodeHash[0xFFC2] = "SOF2";
    segCodeHash[0xFFC3] = "SOF3";
    segCodeHash[0xFFC4] = "SOF4";
    segCodeHash[0xFFC5] = "SOF5";
    segCodeHash[0xFFC6] = "SOF6";
    segCodeHash[0xFFC7] = "SOF7";
    segCodeHash[0xFFC8] = "JPG";     //JPEG extensions
    segCodeHash[0xFFC9] = "SOF9";
    segCodeHash[0xFFCA] = "SOF10";
    segCodeHash[0xFFCB] = "SOF11";
    segCodeHash[0xFFCC] = "DAC";     // Define arithmetic coding
    segCodeHash[0xFFCD] = "SOF13";
    segCodeHash[0xFFCE] = "SOF14";
    segCodeHash[0xFFCF] = "SOF15";
    segCodeHash[0xFFD0] = "RST0";    // Restart Marker 0
    segCodeHash[0xFFD1] = "RST1";
    segCodeHash[0xFFD2] = "RST2";
    segCodeHash[0xFFD3] = "RST3";
    segCodeHash[0xFFD4] = "RST4";
    segCodeHash[0xFFD5] = "RST5";
    segCodeHash[0xFFD6] = "RST6";
    segCodeHash[0xFFD7] = "RST7";
    segCodeHash[0xFFD8] = "SOI";     // Start of Image
    segCodeHash[0xFFD9] = "EOI";     // End of Image
    segCodeHash[0xFFDA] = "SOS";     // Start of Scan
    segCodeHash[0xFFDB] = "DQT";     // Define Quantization Table
    segCodeHash[0xFFDC] = "DNL";     // Define Number of Lines
    segCodeHash[0xFFDD] = "DRI";     // Define Restart Interval
    segCodeHash[0xFFDE] = "DHP";     // Define Hierarchical Progression
    segCodeHash[0xFFDF] = "EXP";     // Expand Reference Component
    segCodeHash[0xFFE0] = "JFIF";
    // FFE1 can be EXIF or XMP.  Defined in getSegments()
    segCodeHash[0xFFE2] = "ICC";
    segCodeHash[0xFFE3] = "APP3";
    segCodeHash[0xFFE4] = "APP4";
    segCodeHash[0xFFE5] = "APP5";
    segCodeHash[0xFFE6] = "APP6";
    segCodeHash[0xFFE7] = "APP7";
    segCodeHash[0xFFE8] = "APP8";
    segCodeHash[0xFFE9] = "APP9";
    segCodeHash[0xFFEA] = "APP10";
    segCodeHash[0xFFEB] = "APP11";
    segCodeHash[0xFFEC] = "APP12";
    segCodeHash[0xFFED] = "IPTC";
    segCodeHash[0xFFEE] = "APP14";
    segCodeHash[0xFFEF] = "APP15";
    segCodeHash[0xFFF0] = "JPG0";    // JPEG Extension 0
    segCodeHash[0xFFF1] = "JPG2";
    segCodeHash[0xFFF2] = "JPG3";
    segCodeHash[0xFFF3] = "JPG4";
    segCodeHash[0xFFF4] = "JPG5";
    segCodeHash[0xFFF5] = "JPG6";
    segCodeHash[0xFFF6] = "JPG7";
    segCodeHash[0xFFF7] = "JPG8";
    segCodeHash[0xFFF8] = "JPG9";
    segCodeHash[0xFFF9] = "JPG10";
    segCodeHash[0xFFFA] = "JPG11";
    segCodeHash[0xFFFB] = "JPG12";
    segCodeHash[0xFFFC] = "JPG13";
    segCodeHash[0xFFFD] = "JPG14";
    segCodeHash[0xFFFE] = "JPG15";
}

void Metadata::initExifHash()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::initExifHash";
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
    qDebug() << "Metadata::initIfdHash";
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
    ifdHash[36867] = "DateTimeOriginal";
    ifdHash[37387] = "FlashEnergy";
    ifdHash[37388] = "SpatialFrequencyResponse";
    ifdHash[37389] = "Noise";
    ifdHash[37393] = "ImageNumber";
    ifdHash[37394] = "SecurityClassification";
    ifdHash[37395] = "ImageHistory";
    ifdHash[37396] = "SubjectLocation";
    ifdHash[37397] = "ExposureIndex";
    ifdHash[37398] = "TIFF/EPStandardID";
    ifdHash[37399] = "SensingMethod";
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
    ifdHash[50778] = "CalibrationIlluminant1";
    ifdHash[50778] = "CalibrationIlluminant1";
    ifdHash[50778] = "CalibrationIlluminant1";
    ifdHash[50778] = "CalibrationIlluminant1";
    ifdHash[50778] = "CalibrationIlluminant1";
    ifdHash[50778] = "CalibrationIlluminant1";
    ifdHash[50778] = "CalibrationIlluminant1";
    ifdHash[50778] = "CalibrationIlluminant1";
    ifdHash[50778] = "CalibrationIlluminant1";
    ifdHash[50737] = "ChromaBlurRadius";
    ifdHash[50738] = "AntiAliasStrength";
    ifdHash[50740] = "DNGPrivateData";
    ifdHash[50741] = "MakerNoteSafety";
    ifdHash[50779] = "CalibrationIlluminant2";
    ifdHash[50780] = "BestQualityScale";
    ifdHash[50784] = "Alias Layer Metadata";
}

void Metadata::reportMetadata()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::reportMetadata";
    #endif
    }
    std::cout << "\n";
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
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "title"
              << std::setw(16) << std::setfill(' ') << std::left << title.toStdString()
              << "\n";
    std::cout << std::flush;

//    qDebug() << "focalLength35mm" << focalLength35mm;
}

void Metadata::track(QString fPath, QString msg)
{
    if (G::isThreadTrackingOn) qDebug() << "• Metadata Caching" << fPath << msg;
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
                   << "\n";
    }
}

uint Metadata::get1(QByteArray c)
{
    return static_cast<unsigned int>(c[0]&0xFF);
}

ulong Metadata::get2(QByteArray c)
{
     {
//     #ifdef ISDEBUG
//     qDebug() << "Metadata::get2";
//     #endif
     }
     if (order == 0x4D4D) return static_cast<unsigned long>
             ((c[0]&0xFF) << 8 | (c[1]&0xFF));
     else return static_cast<unsigned int>
             ((c[0]&0xFF) | (c[1]&0xFF) << 8);
}

ulong Metadata::get4(QByteArray c)
{
    {
//    #ifdef ISDEBUG
//    qDebug() << "Metadata::get4";
//    #endif
    }
    if (order == 0x4D4D) return static_cast<unsigned long>
            ((c[0]&0xFF) << 24 | (c[1]&0xFF) << 16 | (c[2]&0xFF) << 8 | (c[3]&0xFF));
    else return static_cast<unsigned long>
            ((c[0]&0xFF) | (c[1]&0xFF) << 8 | (c[2]&0xFF) << 16 | (c[3]&0xFF) << 24);
}

float Metadata::getReal(long offset)
{
    {
//    #ifdef ISDEBUG
//    qDebug() << "Metadata::getReal";
//    #endif
    }
    file.seek(offset);
    ulong a = get4(file.read(4));
    ulong b = get4(file.read(4));
    if (b==0) return 0;
    float x = (float)a/b;
    return x;
}

void Metadata::readIPTC(ulong offset)
{
    order = 0x4D4D;                  // only IFD/EXIF can be little endian

    // skip the APP FFED and length bytes
    file.seek(offset + 2);
//    ulong appLength = get2(file.read(2));
    ulong segmentLength = get2(file.read(2));
    bool foundIPTC = false;
    ulong count = 0;
    while (!foundIPTC && count < segmentLength) {
        count +=2;
        // find "8BIM" = 0x3842 0x494D
        if (get2(file.read(2)) == 0x3842) {
            if (get2(file.read(2)) == 0x494D) {
                // is it IPTC data?
                if (get2(file.read(2)) == 0x0404) foundIPTC = true;
            }
        }
    }
    if (foundIPTC) {
        // calc pascal-style string length
        int pasStrLen = file.read(1).toInt() + 1;
        pasStrLen = (pasStrLen % 2) ? pasStrLen + 1: pasStrLen;
        file.seek(file.pos() + pasStrLen - 1);
        // read size of resource data
        ulong resourceDataSize = get4(file.read(4));
        ulong endResourceData = file.pos() + resourceDataSize - 4;

        // read data blocks searching for title (0x05)
        bool foundTitle = false;
        while (!foundTitle) {
            // every block starts with tag marker 0x1C
            uint tagMarker = get1(file.read(1));
            if (tagMarker != 0x1C) break;
            uint recordNumber = get1(file.read(1));
            uint tag = get1(file.read(1));
            uint dataLength = get2(file.read(2));
            if (recordNumber == 2 && tag == 5) {
                title = file.read(dataLength);
                foundTitle = true;
            }
            else file.seek(file.pos() + dataLength);
            if (file.pos() > endResourceData) break;
        }
    }
}

ulong Metadata::readIFD(QString hdr, ulong offset)
{
    {
//    #ifdef ISDEBUG
//    qDebug() << "Metadata::readIFD";
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
        std::cout << "\n*******************************"
                  << hdr.toStdString()
                  << "*******************************";
        std::cout << "\n  Offset  tagId  tagType  tagCount  tagValue   tagDescription\n";
        std::cout << std::flush;
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
                      << "   "
                      << std::setw(20) << std::left << tagDescription.toStdString()
                      << "\n";
            std::cout << std::flush;
        }
        // quit if more than 200 tags - prob error
        if (i>200) break;
    }
    ulong nextIFDOffset = get4(file.read(4));
    if (report) std::cout << std::setw(8) << std::setfill(' ') << std::right
             << QString::number(file.pos(), 16).toUpper().toStdString()
             << " nextIFDOffset = "
             << QString::number(nextIFDOffset, 16).toUpper().toStdString()
             << "\n" << std::flush;
    return(nextIFDOffset);
}

QList<ulong> Metadata::getSubIfdOffsets(ulong subIFDaddr, int count)
{
    {
        #ifdef ISDEBUG
        qDebug() << "Metadata::getSubIfdOffsets";
        #endif
    }
    QList<ulong> offsets;
    file.seek(subIFDaddr);
    for (int i = 0; i < count; i++) {
        offsets.append(get4(file.read(4)));
    }
    return offsets;
}

void Metadata::getSegments(ulong offset)
{
    order = 0x4D4D;                  // only IFD/EXIF can be little endian
    uint marker = 0xFFFF;
    while (marker > 0xFFBF) {
        file.seek(offset);           // APP1 FFE*
        marker = get2(file.read(2));
        if (marker < 0xFFC0) break;
        ulong nextOffset = file.pos() + get2(file.read(2));
        if (marker == 0xFFE1) {
            QString segName = file.read(4);
            if (segName == "Exif") segmentHash["EXIF"] = offset;
            if (segName == "http") segName += file.read(15);
            if (segName == "http://ns.adobe.com") segmentHash["XMP"] = offset;
        }
        else if (segCodeHash.contains(marker)) {
            segmentHash[segCodeHash[marker]] = offset;
        }
        offset = nextOffset;
    }
}

bool Metadata::getDimensions(ulong jpgOffset)
{
    {
//    #ifdef ISDEBUG
//    qDebug() << "Metadata::getDimensions";
//    #endif
    }
    order = 0x4D4D;                  // only IFD/EXIF can be little endian
    ulong marker = 0;
    ulong offset = jpgOffset + 2;
    while (marker != 0xFFC0) {
        file.seek(offset);           // APP1 FFE*
        marker = get2(file.read(2));
        if (marker < 0xFF01) return false;
        offset = get2(file.peek(2)) + file.pos();
    }
    file.seek(file.pos()+3);
    width = get2(file.read(2));
    height = get2(file.read(2));
    return true;
}

//ulong Metadata::getExifOffset(ulong offsetIfd0)
//{
//    {
////    #ifdef ISDEBUG
////    qDebug() << "Metadata::getExifOffset";
////    #endif
//    }
//    ulong tagId, tagType = 0;
//    ulong tagCount, tagValue, exifIFDaddr = 0;
//    file.seek(offsetIfd0);
//    ulong tags = get2(file.read(2));
//    for (ulong i=0; i<tags; i++){
//        tagId = get2(file.read(2));
//        tagType = get2(file.read(2));
//        tagCount = get4(file.read(4));
//        tagValue = get4(file.read(4));
//        if (tagId == 34665) {
//            exifIFDaddr = tagValue;
//            break;
//        }
//    }
//    return tagValue;
//}

QString Metadata::getString(ulong offset, ulong length)
{
    {
//    #ifdef ISDEBUG
//    qDebug() << "Metadata::getString";
//    #endif
    }
    file.seek(offset);
    return(file.read(length));
}

void Metadata::formatNikon()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::nikon";
    #endif
    }
    // moved file.open to readMetadata
//    file.open(QIODevice::ReadOnly);
    // get endian
    order = get2(file.read(4));
    // get offset to first IFD and read it
    ulong offsetIfd0 = get4(file.read(4));

    // Nikon does not chaim IFDs
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
    (ifdDataHash.contains(272))
        ? model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount)
        : model = "Unknown";
    (ifdDataHash.contains(274))
        ? orientation = ifdDataHash.value(274).tagValue
        : orientation = 1;
    (ifdDataHash.contains(272))
        ? dateTime = getString(ifdDataHash.value(306).tagValue, ifdDataHash.value(306).tagCount)
        : dateTime = "Unknown";

    ulong offsetEXIF;
    (ifdDataHash.contains(34665))
        ? offsetEXIF = ifdDataHash.value(34665).tagValue
        : offsetEXIF = 0;

    // NIkon provides an offset in IFD0 to the offsets for
    // all the subIFDs
    // get the offsets for the subIFD and read them
    QList<ulong> ifdOffsets;
    if(ifdDataHash.contains(330)) {
        ifdOffsets = getSubIfdOffsets(ifdDataHash.value(330).tagValue,
                                      ifdDataHash.value(330).tagCount);

        // SubIFD1 contains full size jpg offset and length
        QString hdr = "SubIFD1";
        readIFD(hdr, ifdOffsets[0]);

        // pull data reqd from SubIFD1
        (ifdDataHash.contains(513))
            ? offsetFullJPG = ifdDataHash.value(513).tagValue
            : offsetFullJPG = 0;
        (ifdDataHash.contains(514))
            ? lengthFullJPG = ifdDataHash.value(514).tagValue
            : lengthFullJPG = 0;

        // pull data reqd from SubIFD2
        // SubIFD2 contains image width and height

        hdr = "SubIFD2";
        readIFD(hdr, ifdOffsets[1]);
        (ifdDataHash.contains(256))
            ? width = ifdDataHash.value(256).tagValue
            : width = 0;
        (ifdDataHash.contains(257))
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

    // read ExifIFD
    readIFD("IFD Exif", offsetEXIF);
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
//        ISO = "ISO " + QString::number(ifdDataHash.value(34855).tagValue);
        ISO = QString::number(ifdDataHash.value(34855).tagValue);
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

    // read makernoteIFD
    // Offsets in makernote are relative to the start of the makernotes
    ulong makerOffset = ifdDataHash.value(37500).tagValue;
    if (ifdDataHash.contains(37500)) {
        if (report) {
            qDebug() << " ";
            qDebug() << "Maker Offset = " << makerOffset;
        }
        // The IFD starts 10 or 12 bits after the offset to make room for the
        // string "OLYMPUS II  "
        readIFD("IFD Nikon Maker Note", makerOffset + 18);
        // Find the preview IFD offset
        if (ifdDataHash.contains(17)) {
            readIFD("IFD Nikon Maker Note: PreviewIFD",
                    ifdDataHash.value(17).tagValue + makerOffset + 10);
            (ifdDataHash.contains(513))
                ? offsetThumbJPG = ifdDataHash.value(513).tagValue + makerOffset + 10
                : offsetThumbJPG = 0;
            (ifdDataHash.contains(514))
                ? lengthThumbJPG = ifdDataHash.value(514).tagValue
                : lengthThumbJPG = 0;
        }
    }

    if (report) reportMetadata();

}

void Metadata::formatCanon()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::canon";
    #endif
    }
    //file.open in readMetadata
//    file.open(QIODevice::ReadOnly);
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
    // EXIF IFD offset
    ulong offsetEXIF;
    (ifdDataHash.contains(34665))
        ? offsetEXIF = ifdDataHash.value(34665).tagValue
        : offsetEXIF = 0;

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

    // read ExifIFD
    readIFD("IFD Exif", offsetEXIF);
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
        ISO = QString::number(ifdDataHash.value(34855).tagValue);
//        ISO = "ISO " + QString::number(ifdDataHash.value(34855).tagValue);
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

void Metadata::formatOlympus()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::olympus";
    #endif
    }
    //file.open in readMetadata
//    file.open(QIODevice::ReadOnly);
    // get endian
    // get endian
    order = get2(file.read(4));
    // get offset to first IFD and read it
    ulong offsetIfd0 = get4(file.read(4));

    ulong nextIFDOffset = readIFD("IFD0", offsetIfd0);

    // pull data reqd from IFD0
    (ifdDataHash.contains(272))
        ? model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount)
        : model = "Unknown";
    (ifdDataHash.contains(274))
        ? orientation = ifdDataHash.value(274).tagValue
        : orientation = 1;
    (ifdDataHash.contains(272))
        ? dateTime = getString(ifdDataHash.value(306).tagValue, ifdDataHash.value(306).tagCount)
        : dateTime = "Unknown";

    // get the offset for ExifIFD and read it
    ulong offsetEXIF;
    (ifdDataHash.contains(34665))
        ? offsetEXIF = ifdDataHash.value(34665).tagValue
        : offsetEXIF = 0;
    readIFD("IFD Exif", offsetEXIF);

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
        ISO = QString::number(ifdDataHash.value(34855).tagValue);
//        ISO = "ISO " + QString::number(ifdDataHash.value(34855).tagValue);
    } else {
        ISO = "Unknown";
    }

    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "Unknown";
    }

    // read makernoteIFD
    // Offsets in makernote are relative to the start of the makernotes
    ulong makerOffset = ifdDataHash.value(37500).tagValue;
    if (ifdDataHash.contains(37500)) {
        // The IFD starts 10 or 12 bits after the offset to make room for the
        // string "OLYMPUS II  "
//        qDebug() << " ";
//        qDebug() << "Maker Offset = " << makerOffset;
        readIFD("IFD Olympus Maker Note", makerOffset + 12);
        // Get the thumbnail Jpg offset and length
        if (ifdDataHash.contains(256)) {
            offsetThumbJPG = ifdDataHash.value(256).tagValue + makerOffset;
            lengthThumbJPG = ifdDataHash.value(256).tagCount;
        }

        // read CameraSettingsIFD
        if (ifdDataHash.contains(8224)) {
            readIFD("IFD Olympus Maker Note: CameraSettingsIFD",
                    ifdDataHash.value(8224).tagValue + makerOffset);
            (ifdDataHash.contains(257))
                ? offsetFullJPG = ifdDataHash.value(257).tagValue + makerOffset
                : offsetFullJPG = 0;
            (ifdDataHash.contains(258))
                ? lengthFullJPG = ifdDataHash.value(258).tagValue
                : lengthFullJPG = 0;
            getDimensions(offsetFullJPG);

        }
    }

    if (report) reportMetadata();
}

void Metadata::formatSony()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::formatSony";
    #endif
    }
//    file.open(QIODevice::ReadOnly);
    // get endian
    order = get2(file.read(4));
    // get offset to first IFD and read it
    ulong offsetIfd0 = get4(file.read(4));

    ulong nextIFDOffset = readIFD("IFD0", offsetIfd0);

    // pull data reqd from IFD0
    (ifdDataHash.contains(513))
        ? offsetFullJPG = ifdDataHash.value(273).tagValue
        : offsetFullJPG = 0;
    (ifdDataHash.contains(514))
        ? lengthFullJPG = ifdDataHash.value(279).tagValue
        : lengthFullJPG = 0;
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
    (ifdDataHash.contains(306))
        ? dateTime = getString(ifdDataHash.value(306).tagValue, ifdDataHash.value(306).tagCount)
        : dateTime = "Unknown";

    ulong offsetEXIF;
    (ifdDataHash.contains(34665))
        ? offsetEXIF = ifdDataHash.value(34665).tagValue
        : offsetEXIF = 0;

    if (nextIFDOffset) readIFD("IFD1", nextIFDOffset);

    // Sony provides an offset in IFD0 to the offsets for
    // all the subIFDs
    // get the offsets for the subIFD and read them
    QList<ulong> ifdOffsets;
    if(ifdDataHash.contains(330)) {
        ifdOffsets = getSubIfdOffsets(ifdDataHash.value(330).tagValue,
                                      ifdDataHash.value(330).tagCount);

        // SubIFD1 contains full size jpg offset and length
        QString hdr = "SubIFD0";
//        readIFD(hdr, ifdOffsets[0]);

    }

    // get the offset for ExifIFD and read it
    readIFD("IFD Exif", offsetEXIF);
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
        ISO = QString::number(ifdDataHash.value(34855).tagValue);
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

void Metadata::formatFuji()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::fuji";
    #endif
    }
    file.open(QIODevice::ReadOnly);
    // get endian
    order = get2(file.read(4));
    // get offset to first IFD and read it
    ulong offsetIfd0 = get4(file.read(4));

//    ulong nextIFDOffset = readIFD("IFD0", offsetIfd0);

    // pull data reqd from IFD0
    (ifdDataHash.contains(272))
        ? model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount)
        : model = "Unknown";
    (ifdDataHash.contains(274))
        ? orientation = ifdDataHash.value(274).tagValue
        : orientation = 1;
    (ifdDataHash.contains(272))
        ? dateTime = getString(ifdDataHash.value(306).tagValue, ifdDataHash.value(306).tagCount)
        : dateTime = "Unknown";

    ulong offsetEXIF;
    (ifdDataHash.contains(34665))
        ? offsetEXIF = ifdDataHash.value(34665).tagValue
        : offsetEXIF = 0;

}

bool Metadata::formatJPG()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::formatJPG";
    #endif
    }
    //file.open in readMetadata
    order = 0x4D4D;
    ulong startOffset = 0;
    if (get2(file.read(2)) != 0xFFD8) return 0;

    // build a hash of jpg segment offsets
    getSegments(file.pos());

//    // get the APP segment - should be FFE1
//    ulong appSegment = get2(file.read(2));
//    // get length of first APP segment
//    ulong appLength = get2(file.read(2));
//    // get offset to next APP segment
//    ulong nextAppOffset = appLength + file.pos() - 2;

    // read the EXIF data
    if (segmentHash.contains("EXIF")) file.seek(segmentHash["EXIF"]);
    else return false;

    bool foundEndian = false;
    while (!foundEndian) {
        ulong a = get2(file.read(2));
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
    // ifdEXIF offset
    ulong offsetEXIF;
    (ifdDataHash.contains(34665))
        ? offsetEXIF = ifdDataHash.value(34665).tagValue + startOffset
        : offsetEXIF = 0;    // read IFD1 - unfortunately nothing of interest

    // check for daisy chain next IFD
    if (nextIFDOffset) nextIFDOffset = readIFD("IFD1", nextIFDOffset);
    (ifdDataHash.contains(513))
        ? offsetThumbJPG = ifdDataHash.value(513).tagValue + 12
        : offsetThumbJPG = 0;
    (ifdDataHash.contains(514))
        ? lengthThumbJPG = ifdDataHash.value(514).tagValue
        : lengthThumbJPG = 0;

    // read EXIF
    readIFD("IFD Exif", offsetEXIF);
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
        ISO = QString::number(ifdDataHash.value(34855).tagValue);
//        ISO = "ISO " + QString::number(ifdDataHash.value(34855).tagValue);
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

    if (!width || !height) getDimensions(0);

    // read next app segment
    if (segmentHash.contains("IPTC")) readIPTC(segmentHash["IPTC"]);

    //    order = 0x4D4D;
//    file.seek(nextAppOffset);
//    appSegment = get2(file.read(2));
//    qDebug() << "nextAppOffset" << QString::number(nextAppOffset, 16).toUpper();

//    if (appSegment == 0xFFED) nextAppOffset = readIPTC(file.pos());

    if (report) reportMetadata();
    return true;
}

void Metadata::clearMetadata()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::clearMetadata";
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
    title = "";
    err = "";

    ifdDataHash.clear();
}

bool Metadata::readMetadata(bool rpt, const QString &fPath)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::readMetadata";
    #endif
    }
    report = rpt;
//    report = false;
//    QElapsedTimer t;
//    t.start();
    clearMetadata();
    file.setFileName(fPath);
    if (report) std::cout << "\nFile name =" << fPath.toStdString()
                          << "\n" << std::flush;
    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.completeSuffix().toLower();
    if (G::isThreadTrackingOn) track(fPath, "Reading ");
    bool success = false;
    int totDelay = 50;
    int msDelay = 0;
    int msInc = 1;
    bool fileOpened = false;
    do {
        if (file.open(QIODevice::ReadOnly)) {
//            qDebug() << "MetadataCache opened. Delay =" << msDelay << imageFullPath;
            if (ext == "nef") formatNikon();
            if (ext == "cr2") formatCanon();
            if (ext == "orf") formatOlympus();
            if (ext == "arw") formatSony();
            if (ext == "jpg") formatJPG();
            fileOpened = true;
            file.close();
            success = true;
        }
        else {
            err = "Could not open file to read metadata";    // try again
            QThread::msleep(msInc);
            msDelay += msInc;
            if (G::isThreadTrackingOn) track(fPath, err);
        }
    }
    while ((msDelay < totDelay) && !success);

    // not all files have thumb or small jpg embedded
    if (offsetFullJPG == 0 && fileOpened) {
        err = "No embedded JPG found";
        if (G::isThreadTrackingOn) track(fPath, err);
    }

    if (offsetSmallJPG == 0) offsetSmallJPG = offsetFullJPG;
    if (offsetThumbJPG == 0) offsetThumbJPG = offsetSmallJPG;

//    qDebug() << fPath << offsetThumbJPG << offsetSmallJPG << offsetFullJPG;

    if (!success) track(fPath, "FAILED TO LOAD METADATA");
    else track(fPath, "Success");

//    if (GData::isTimer) qDebug() << "Time to read metadata =" << t.elapsed();
    return success;
}

void Metadata::removeImage(QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::removeImage";
    #endif
    }
    metaCache.remove(imageFileName);
}

bool Metadata::isLoaded(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::isLoaded";
    #endif
    }
    return metaCache[imageFileName].isLoaded;
}

ulong Metadata::getOffsetFullJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getOffsetFullJPG";
    #endif
    }
    return metaCache[imageFileName].offsetFullJPG;
}

ulong Metadata::getLengthFullJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getLengthFullJPG";
    #endif
    }
    return metaCache[imageFileName].lengthFullJPG;
}

ulong Metadata::getOffsetThumbJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getOffsetThumbJPG" << imageFileName;
    #endif
    }
    return metaCache[imageFileName].offsetThumbJPG;
}

ulong Metadata::getLengthThumbJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getLengthThumbJPG";
    #endif
    }
    return metaCache[imageFileName].lengthThumbJPG;
}

ulong Metadata::getOffsetSmallJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getOffsetSmallJPG";
    #endif
    }
    return metaCache[imageFileName].offsetSmallJPG;
}

ulong Metadata::getLengthSmallJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getLengthSmallJPG";
    #endif
    }
    return metaCache[imageFileName].lengthSmallJPG;
}

ulong Metadata::getWidth(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getWidth";
    #endif
    }
    return metaCache[imageFileName].width;
}

ulong Metadata::getHeight(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getHeight";
    #endif
    }
    return metaCache[imageFileName].height;
}

QString Metadata::getDateTime(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getDateTime";
    #endif
    }
    return metaCache[imageFileName].dateTime;
}

int Metadata::getYear(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getYear";
    #endif
    }
    return metaCache[imageFileName].year;
}

int Metadata::getMonth(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getMonth";
    #endif
    }
    return metaCache[imageFileName].month;
}

int Metadata::getDay(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getDay";
    #endif
    }
    return metaCache[imageFileName].day;
}

QString Metadata::getCopyFileNamePrefix(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getCopyFileNamePrefix";
    #endif
    }
    return metaCache[imageFileName].copyFileNamePrefix;
}

QString Metadata::getModel(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getModel";
    #endif
    }
    return metaCache[imageFileName].model;
}

QString Metadata::getExposureTime(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getExposureTime";
    #endif
    }
    return metaCache[imageFileName].exposureTime;
}

QString Metadata::getAperture(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getAperture";
    #endif
    }
    return metaCache[imageFileName].aperture;
}

QString Metadata::getISO(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getISO";
    #endif
    }
    return metaCache[imageFileName].ISO;
}

QString Metadata::getFocalLength(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getFocalLength";
    #endif
    }
    return metaCache[imageFileName].focalLength;
}

QString Metadata::getTitle(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getFocalLength";
    #endif
    }
    return metaCache[imageFileName].title;
}

QString Metadata::getShootingInfo(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getShootingInfo";
    #endif
    }
    return metaCache[imageFileName].shootingInfo;
}

QString Metadata::getErr(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getErr";
    #endif
    }
    return metaCache[imageFileName].err;
}

void Metadata::setErr(const QString &imageFileName, const QString &err)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getErr";
    #endif
    }
    metaCache[imageFileName].err = err;
}

int Metadata::getImageOrientation(QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getImageOrientation";
    #endif
    }
//    if (metaCache.contains(imageFileName)|| loadImageMetadata(imageFileName)) {
//        return metaCache[imageFileName].orientation;
//    }
//    return 0;

    if (metaCache.contains(imageFileName)) {
        return metaCache[imageFileName].orientation;
    }
    else return 0;
}

bool Metadata::getPick(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::getPick";
    #endif
    }
    return metaCache[imageFileName].isPicked;
}

void Metadata::setPick(const QString &imageFileName, bool choice)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::setPick";
    #endif
    }
    metaCache[imageFileName].isPicked = choice;
}

void Metadata::clear()
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::clear";
    #endif
    }
    metaCache.clear();
}

bool Metadata::loadImageMetadata(const QFileInfo &fileInfo)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Metadata::loadImageMetadata";
    #endif
    }
    // check if already loaded
//    if (metaCache.contains(fileInfo.filePath())) return true;

    bool result;
    QString shootingInfo;

    ImageMetadata imageMetadata;
    result = readMetadata(false, fileInfo.filePath());

    imageMetadata.isPicked = false;
    imageMetadata.offsetFullJPG = offsetFullJPG;
    imageMetadata.lengthFullJPG = lengthFullJPG;
    imageMetadata.offsetThumbJPG = offsetThumbJPG;
    imageMetadata.lengthThumbJPG = lengthThumbJPG;
    imageMetadata.offsetSmallJPG = offsetSmallJPG;
    imageMetadata.lengthSmallJPG = lengthSmallJPG;
    imageMetadata.width = width;
    imageMetadata.height = height;
    imageMetadata.dateTime = dateTime;
    imageMetadata.model = model;
    imageMetadata.exposureTime = exposureTime;
    imageMetadata.aperture = aperture;
    imageMetadata.ISO = ISO;
    imageMetadata.focalLength = focalLength;
    imageMetadata.title = title;
    imageMetadata.year = dateTime.left(4).toInt();
    imageMetadata.month = dateTime.mid(5,2).toInt();
    imageMetadata.day = dateTime.mid(8,2).toInt();

    QString s = dateTime;
    s.replace(":", "-");
    imageMetadata.copyFileNamePrefix = s.left(10);

    s = imageMetadata.model;
    s += "  " + imageMetadata.focalLength;
    s += "  " + imageMetadata.exposureTime;
    s += (imageMetadata.aperture == "") ? "" : " at " + imageMetadata.aperture;
    s += (imageMetadata.ISO == "") ? "" : ", ISO " + imageMetadata.ISO;
    shootingInfo = s;

    if (shootingInfo.length() > 0) imageMetadata.shootingInfo = shootingInfo;
    if (orientation) imageMetadata.orientation = orientation;
    imageMetadata.isLoaded = true;

    metaCache.insert(fileInfo.filePath(), imageMetadata);

    return result;
}
