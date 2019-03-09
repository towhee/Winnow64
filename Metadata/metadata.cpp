#include "Metadata/metadata.h"
#include <QDebug>
#include "Main/global.h"

Metadata::Metadata(QObject *parent) : QObject(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // some initialization
    initSegCodeHash();
    initExifHash();
    initIfdHash();
    initOrientationHash();
    initNikonMakerHash();
    initCanonMakerHash();
    initSonyMakerHash();
    initFujiMakerHash();
    initPanasonicMakerHash();
    initNikonLensHash();
    initSupportedFiles();
    report = false;
}

/* METADATA NOTES

Endian:

    Big endian = 4D4D and reads bytes from left to right
                 ie 3F2A = 16170
                 ie 00000001 = 1

    Litte endian = 4949 and reads bytes from right to left
                 ie 3f2A gets read as 2A3F = 10815
                 ie 00000001 gets read as 01000000 = 16777216

TIF data types:
    Value	Type
    1       unsigned byte
    2       ascii strings
    3       unsigned short
    4       unsigned long
    5       unsigned rational
    6       signed byte
    7       undefined
    8       signed short
    9       signed long
    10      signed rational
    11      single float
    12      double float

Source:  D:\My Projects\Winnow Project\IFD.xlsx in status tab

Item             :	arw	cr2	jpg	nef	orf	sr2	tif
Created          :			x	x
Modified         :			x
Dimensions       :			x	x
Aperture         :			x	x
Shutterspeed     :			x	x
ISO              :			x	x
Model            :			x	x
Lens             :			x	x
Focal length     :			x	x
Camera SN        :				x
Lens SN          :				x
Shutter count    :				x
Title            :			x
Copyright        :			x
Email            :			x
Url              :			x


*/

void Metadata::initSupportedFiles()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // add raw file types here as they are supported
    rawFormats << "arw" << "cr2" << "dng" << "nef" << "orf" << "raf" << "sr2" << "rw2";
    sidecarFormats << "arw" << "cr2" << "nef" << "orf" << "raf" << "sr2" << "rw2" << "jpg";
    internalXmpFormats << "notyetjpg";
    xmpWriteFormats << "jpg" << "arw" << "cr2" << "nef" << "orf" << "raf" << "sr2" << "rw2";

    supportedFormats << "arw" << "bmp" << "cr2" << "cur" << "dds" << "dng" << "gif" <<
    "icns" << "ico" << "jpeg" << "jpg" << "jp2" << "jpe" << "mng" << "nef" <<
    "orf" << "pbm" << "pgm" << "png" << "ppm" << "raf" << "rw2" << "sr2" << "svg" <<
    "svgz" << "tga" << "tif" << "wbmp" << "webp" << "xbm" << "xpm";
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
    G::track(__FUNCTION__);
    #endif
    }
//    exifHash[293] = "ExposureTime";
//    exifHash[294] = "FNumber";
//    exifHash[295] = "ExposureProgram";
//    exifHash[296] = "ISOSpeedRatings";
//    exifHash[297] = "ExifVersion";
//    exifHash[304] = "DateTimeOriginal";         // 0x0130
//    exifHash[305] = "DateTimeDigitized";        // 0x0131
//    exifHash[306] = "ModifyDate";               // 0x0132
//    exifHash[307] = "CompressedBitsPerPixel";
//    exifHash[308] = "ShutterSpeedValue";
//    exifHash[309] = "ApertureValue";
//    exifHash[310] = "BrightnessValue";
//    exifHash[311] = "ExposureBiasValue";
//    exifHash[312] = "MaxApertureValue";
//    exifHash[313] = "SubjectDistance";
//    exifHash[320] = "MeteringMode";
//    exifHash[321] = "LightSource";
//    exifHash[322] = "Flash";
//    exifHash[323] = "FocalLength";
//    exifHash[324] = "MakerNote";
//    exifHash[325] = "UserComment";
//    exifHash[326] = "FlashPixVersion";
//    exifHash[327] = "ColorSpace";
//    exifHash[328] = "ExifImageWidth";
//    exifHash[329] = "ExifImageHeight";
//    exifHash[336] = "RelatedSoundFile";
//    exifHash[337] = "ExifInteroperabilityOffset";
//    exifHash[338] = "FocalPlaneXResolution";
//    exifHash[339] = "FocalPlaneYResolution";
//    exifHash[340] = "FocalPlaneResolutionUnit";
//    exifHash[341] = "SensingMethod";
//    exifHash[342] = "FileSource";
//    exifHash[343] = "SceneType";
//    exifHash[33434] = "ExposureTime";
//    exifHash[33437] = "FNumber";
//    exifHash[34850] = "ExposureProgram";
//    exifHash[34852] = "SpectralSensitivity";
//    exifHash[34855] = "ISOSpeedRatings";
//    exifHash[34856] = "OECF";
//    exifHash[36864] = "ExifVersion";
//    exifHash[36867] = "DateTimeOriginal";
//    exifHash[36868] = "DateTimeDigitized";
//    exifHash[37121] = "ComponentsConfiguration";
//    exifHash[37122] = "CompressedBitsPerPixel";
//    exifHash[37377] = "ShutterSpeedValue";
//    exifHash[37378] = "ApertureValue";
//    exifHash[37379] = "BrightnessValue";
//    exifHash[37380] = "ExposureBiasValue";
//    exifHash[37381] = "MaxApertureValue";
//    exifHash[37382] = "SubjectDistance";
//    exifHash[37383] = "MeteringMode";
//    exifHash[37384] = "LightSource";
//    exifHash[37385] = "Flash";
//    exifHash[37386] = "FocalLength";
//    exifHash[37396] = "SubjectArea";
//    exifHash[37500] = "MakerNote";
//    exifHash[37510] = "UserComment";
//    exifHash[37520] = "SubsecTime";
//    exifHash[37521] = "SubsecTimeOriginal";
//    exifHash[37522] = "SubsecTimeDigitized";
//    exifHash[40960] = "FlashpixVersion";
//    exifHash[40961] = "ColorSpace";
//    exifHash[40962] = "PixelXDimension";
//    exifHash[40963] = "PixelYDimension";
//    exifHash[40964] = "RelatedSoundFile";
//    exifHash[41483] = "FlashEnergy";
//    exifHash[41484] = "SpatialFrequencyResponse";
//    exifHash[41486] = "FocalPlaneXResolution";
//    exifHash[41487] = "FocalPlaneYResolution";
//    exifHash[41488] = "FocalPlaneResolutionUnit";
//    exifHash[41492] = "SubjectLocation";
//    exifHash[41493] = "ExposureIndex";
//    exifHash[41495] = "SensingMethod";
//    exifHash[41728] = "FileSource";
//    exifHash[41729] = "SceneType";
//    exifHash[41730] = "CFAPattern";
//    exifHash[41985] = "CustomRendered";
//    exifHash[41986] = "ExposureMode";
//    exifHash[41987] = "WhiteBalance";
//    exifHash[41988] = "DigitalZoomRatio";
//    exifHash[41989] = "FocalLengthIn35mmFilm";
//    exifHash[41990] = "SceneCaptureType";
//    exifHash[41991] = "GainControl";
//    exifHash[41992] = "Contrast";
//    exifHash[41993] = "Saturation";
//    exifHash[41994] = "Sharpness";
//    exifHash[41995] = "DeviceSettingDescription";
//    exifHash[41996] = "SubjectDistanceRange";
//    exifHash[42016] = "ImageUniqueID";
//    exifHash[42033] = "Camera serial number";
//    exifHash[42034] = "Lens specification";
//    exifHash[42035] = "Lens make";
//    exifHash[42036] = "Lens model";
//    exifHash[42037] = "Lens serial number";

    exifHash[1] = "InteropIndex";
    exifHash[2] = "InteropVersion";
    exifHash[254] = "SubfileType";
    exifHash[255] = "OldSubfileType";
    exifHash[256] = "ImageWidth";
    exifHash[257] = "ImageHeight";
    exifHash[258] = "BitsPerSample";
    exifHash[259] = "Compression";
    exifHash[262] = "PhotometricInterpretation";
    exifHash[263] = "Thresholding";
    exifHash[264] = "CellWidth";
    exifHash[265] = "CellLength";
    exifHash[266] = "FillOrder";
    exifHash[269] = "DocumentName";
    exifHash[270] = "ImageDescription";
    exifHash[271] = "Make";
    exifHash[272] = "Model";
    exifHash[273] = "StripOffsets ";
    exifHash[274] = "Orientation";
    exifHash[277] = "SamplesPerPixel";
    exifHash[278] = "RowsPerStrip";
    exifHash[279] = "StripByteCounts ";
    exifHash[280] = "MinSampleValue";
    exifHash[281] = "MaxSampleValue";
    exifHash[282] = "XResolution";
    exifHash[283] = "YResolution";
    exifHash[284] = "PlanarConfiguration";
    exifHash[285] = "PageName";
    exifHash[286] = "XPosition";
    exifHash[287] = "YPosition";
    exifHash[288] = "FreeOffsets";
    exifHash[289] = "FreeByteCounts";
    exifHash[290] = "GrayResponseUnit";
    exifHash[291] = "GrayResponseCurve";
    exifHash[292] = "T4Options";
    exifHash[293] = "T6Options";
    exifHash[296] = "ResolutionUnit";
    exifHash[297] = "PageNumber";
    exifHash[300] = "ColorResponseUnit";
    exifHash[301] = "TransferFunction";
    exifHash[305] = "Software";
    exifHash[306] = "ModifyDate";
    exifHash[315] = "Artist";
    exifHash[316] = "HostComputer";
    exifHash[317] = "Predictor";
    exifHash[318] = "WhitePoint";
    exifHash[319] = "PrimaryChromaticities";
    exifHash[320] = "ColorMap";
    exifHash[321] = "HalftoneHints";
    exifHash[322] = "TileWidth";
    exifHash[323] = "TileLength";
    exifHash[324] = "TileOffsets";
    exifHash[325] = "TileByteCounts";
    exifHash[326] = "BadFaxLines";
    exifHash[327] = "CleanFaxData";
    exifHash[328] = "ConsecutiveBadFaxLines";
    exifHash[330] = "SubIFD ";
    exifHash[332] = "InkSet";
    exifHash[333] = "InkNames";
    exifHash[334] = "NumberofInks";
    exifHash[336] = "DotRange";
    exifHash[337] = "TargetPrinter";
    exifHash[338] = "ExtraSamples";
    exifHash[339] = "SampleFormat";
    exifHash[340] = "SMinSampleValue";
    exifHash[341] = "SMaxSampleValue";
    exifHash[342] = "TransferRange";
    exifHash[343] = "ClipPath";
    exifHash[344] = "XClipPathUnits";
    exifHash[345] = "YClipPathUnits";
    exifHash[346] = "Indexed";
    exifHash[347] = "JPEGTables";
    exifHash[351] = "OPIProxy";
    exifHash[400] = "GlobalParametersIFD";
    exifHash[401] = "ProfileType";
    exifHash[402] = "FaxProfile";
    exifHash[403] = "CodingMethods";
    exifHash[404] = "VersionYear";
    exifHash[405] = "ModeNumber";
    exifHash[433] = "Decode";
    exifHash[434] = "DefaultImageColor";
    exifHash[435] = "T82Options";
    exifHash[437] = "JPEGTables";
    exifHash[512] = "JPEGProc";
    exifHash[513] = "ThumbnailOffset ";
    exifHash[514] = "ThumbnailLength ";
    exifHash[515] = "JPEGRestartInterval";
    exifHash[517] = "JPEGLosslessPredictors";
    exifHash[518] = "JPEGPointTransforms";
    exifHash[519] = "JPEGQTables";
    exifHash[520] = "JPEGDCTables";
    exifHash[521] = "JPEGACTables";
    exifHash[529] = "YCbCrCoefficients";
    exifHash[530] = "YCbCrSubSampling";
    exifHash[531] = "YCbCrPositioning";
    exifHash[532] = "ReferenceBlackWhite";
    exifHash[559] = "StripRowCounts";
    exifHash[700] = "XMP offset";       // "ApplicationNotes";
    exifHash[999] = "USPTOMiscellaneous";
    exifHash[4096] = "RelatedImageFileFormat";
    exifHash[4097] = "RelatedImageWidth";
    exifHash[4098] = "RelatedImageHeight";
    exifHash[18246] = "Rating";
    exifHash[18247] = "XP_DIP_XML";
    exifHash[18248] = "StitchInfo";
    exifHash[18249] = "RatingPercent";
    exifHash[28672] = "SonyRawFileType";
    exifHash[28722] = "VignettingCorrParams";
    exifHash[28725] = "ChromaticAberrationCorrParams";
    exifHash[28727] = "DistortionCorrParams";
    exifHash[32781] = "ImageID";
    exifHash[32931] = "WangTag1";
    exifHash[32932] = "WangAnnotation";
    exifHash[32933] = "WangTag3";
    exifHash[32934] = "WangTag4";
    exifHash[32953] = "ImageReferencePoints";
    exifHash[32954] = "RegionXformTackPoint";
    exifHash[32955] = "WarpQuadrilateral";
    exifHash[32956] = "AffineTransformMat";
    exifHash[32995] = "Matteing";
    exifHash[32996] = "DataType";
    exifHash[32997] = "ImageDepth";
    exifHash[32998] = "TileDepth";
    exifHash[33300] = "ImageFullWidth";
    exifHash[33301] = "ImageFullHeight";
    exifHash[33302] = "TextureFormat";
    exifHash[33303] = "WrapModes";
    exifHash[33304] = "FovCot";
    exifHash[33305] = "MatrixWorldToScreen";
    exifHash[33306] = "MatrixWorldToCamera";
    exifHash[33405] = "Model2";
    exifHash[33421] = "CFARepeatPatternDim";
    exifHash[33422] = "CFAPattern2";
    exifHash[33423] = "BatteryLevel";
    exifHash[33424] = "KodakIFD";
    exifHash[33432] = "Copyright";
    exifHash[33434] = "ExposureTime";
    exifHash[33437] = "FNumber";
    exifHash[33445] = "MDFileTag";
    exifHash[33446] = "MDScalePixel";
    exifHash[33447] = "MDColorTable";
    exifHash[33448] = "MDLabName";
    exifHash[33449] = "MDSampleInfo";
    exifHash[33450] = "MDPrepDate";
    exifHash[33451] = "MDPrepTime";
    exifHash[33452] = "MDFileUnits";
    exifHash[33550] = "PixelScale";
    exifHash[33589] = "AdventScale";
    exifHash[33590] = "AdventRevision";
    exifHash[33628] = "UIC1Tag";
    exifHash[33629] = "UIC2Tag";
    exifHash[33630] = "UIC3Tag";
    exifHash[33631] = "UIC4Tag";
    exifHash[33723] = "IPTC-NAA";
    exifHash[33918] = "IntergraphPacketData";
    exifHash[33919] = "IntergraphFlagRegisters";
    exifHash[33920] = "IntergraphMatrix";
    exifHash[33921] = "INGRReserved";
    exifHash[33922] = "ModelTiePoint";
    exifHash[34016] = "Site";
    exifHash[34017] = "ColorSequence";
    exifHash[34018] = "IT8Header";
    exifHash[34019] = "RasterPadding";
    exifHash[34020] = "BitsPerRunLength";
    exifHash[34021] = "BitsPerExtendedRunLength";
    exifHash[34022] = "ColorTable";
    exifHash[34023] = "ImageColorIndicator";
    exifHash[34024] = "BackgroundColorIndicator";
    exifHash[34025] = "ImageColorValue";
    exifHash[34026] = "BackgroundColorValue";
    exifHash[34027] = "PixelIntensityRange";
    exifHash[34028] = "TransparencyIndicator";
    exifHash[34029] = "ColorCharacterization";
    exifHash[34030] = "HCUsage";
    exifHash[34031] = "TrapIndicator";
    exifHash[34032] = "CMYKEquivalent";
    exifHash[34118] = "SEMInfo";
    exifHash[34152] = "AFCP_IPTC";
    exifHash[34232] = "PixelMagicJBIGOptions";
    exifHash[34263] = "JPLCartoIFD";
    exifHash[34264] = "ModelTransform";
    exifHash[34306] = "WB_GRGBLevels";
    exifHash[34310] = "LeafData";
    exifHash[34377] = "PhotoshopSettings";
    exifHash[34665] = "ExifOffset";
    exifHash[34675] = "ICC_Profile";
    exifHash[34687] = "TIFF_FXExtensions";
    exifHash[34688] = "MultiProfiles";
    exifHash[34689] = "SharedData";
    exifHash[34690] = "T88Options";
    exifHash[34732] = "ImageLayer";
    exifHash[34735] = "GeoTiffDirectory";
    exifHash[34736] = "GeoTiffDoubleParams";
    exifHash[34737] = "GeoTiffAsciiParams";
    exifHash[34750] = "JBIGOptions";
    exifHash[34850] = "ExposureProgram";
    exifHash[34852] = "SpectralSensitivity";
    exifHash[34853] = "GPSInfo";
    exifHash[34855] = "ISO";
    exifHash[34856] = "Opto-ElectricConvFactor";
    exifHash[34857] = "Interlace";
    exifHash[34858] = "TimeZoneOffset";
    exifHash[34859] = "SelfTimerMode";
    exifHash[34864] = "SensitivityType";
    exifHash[34865] = "StandardOutputSensitivity";
    exifHash[34866] = "RecommendedExposureIndex";
    exifHash[34867] = "ISOSpeed";
    exifHash[34868] = "ISOSpeedLatitudeyyy";
    exifHash[34869] = "ISOSpeedLatitudezzz";
    exifHash[34908] = "FaxRecvParams";
    exifHash[34909] = "FaxSubAddress";
    exifHash[34910] = "FaxRecvTime";
    exifHash[34929] = "FedexEDR";
    exifHash[34954] = "LeafSubIFD";
    exifHash[36864] = "ExifVersion";
    exifHash[36867] = "DateTimeOriginal";
    exifHash[36868] = "CreateDate";
    exifHash[36873] = "GooglePlusUploadCode";
    exifHash[36880] = "OffsetTime";
    exifHash[36881] = "OffsetTimeOriginal";
    exifHash[36882] = "OffsetTimeDigitized";
    exifHash[37121] = "ComponentsConfiguration";
    exifHash[37122] = "CompressedBitsPerPixel";
    exifHash[37377] = "ShutterSpeedValue";
    exifHash[37378] = "ApertureValue";
    exifHash[37379] = "BrightnessValue";
    exifHash[37380] = "ExposureCompensation";
    exifHash[37381] = "MaxApertureValue";
    exifHash[37382] = "SubjectDistance";
    exifHash[37383] = "MeteringMode";
    exifHash[37384] = "LightSource";
    exifHash[37385] = "Flash";
    exifHash[37386] = "FocalLength";
    exifHash[37387] = "FlashEnergy";
    exifHash[37388] = "SpatialFrequencyResponse";
    exifHash[37389] = "Noise";
    exifHash[37390] = "FocalPlaneXResolution";
    exifHash[37391] = "FocalPlaneYResolution";
    exifHash[37392] = "FocalPlaneResolutionUnit";
    exifHash[37393] = "ImageNumber";
    exifHash[37394] = "SecurityClassification";
    exifHash[37395] = "ImageHistory";
    exifHash[37396] = "SubjectArea";
    exifHash[37397] = "ExposureIndex";
    exifHash[37398] = "TIFF-EPStandardID";
    exifHash[37399] = "SensingMethod";
    exifHash[37434] = "CIP3DataFile";
    exifHash[37435] = "CIP3Sheet";
    exifHash[37436] = "CIP3Side";
    exifHash[37439] = "StoNits";
    exifHash[37500] = "MakerNote";
    exifHash[37510] = "UserComment";
    exifHash[37520] = "SubSecTime";
    exifHash[37521] = "SubSecTimeOriginal";
    exifHash[37522] = "SubSecTimeDigitized";
    exifHash[37679] = "MSDocumentText";
    exifHash[37680] = "MSPropertySetStorage";
    exifHash[37681] = "MSDocumentTextPosition";
    exifHash[37724] = "ImageSourceData";
    exifHash[37888] = "AmbientTemperature";
    exifHash[37889] = "Humidity";
    exifHash[37890] = "Pressure";
    exifHash[37891] = "WaterDepth";
    exifHash[37892] = "Acceleration";
    exifHash[37893] = "CameraElevationAngle";
    exifHash[40091] = "XPTitle";
    exifHash[40092] = "XPComment";
    exifHash[40093] = "XPAuthor";
    exifHash[40094] = "XPKeywords";
    exifHash[40095] = "XPSubject";
    exifHash[40960] = "FlashpixVersion";
    exifHash[40961] = "ColorSpace";
    exifHash[40962] = "ExifImageWidth";
    exifHash[40963] = "ExifImageHeight";
    exifHash[40964] = "RelatedSoundFile";
    exifHash[40965] = "InteropOffset";
    exifHash[40976] = "SamsungRawPointersOffset";
    exifHash[40977] = "SamsungRawPointersLength";
    exifHash[41217] = "SamsungRawByteOrder";
    exifHash[41218] = "SamsungRawUnknown?";
    exifHash[41483] = "FlashEnergy";
    exifHash[41484] = "SpatialFrequencyResponse";
    exifHash[41485] = "Noise";
    exifHash[41486] = "FocalPlaneXResolution";
    exifHash[41487] = "FocalPlaneYResolution";
    exifHash[41488] = "FocalPlaneResolutionUnit";
    exifHash[41489] = "ImageNumber";
    exifHash[41490] = "SecurityClassification";
    exifHash[41491] = "ImageHistory";
    exifHash[41492] = "SubjectLocation";
    exifHash[41493] = "ExposureIndex";
    exifHash[41494] = "TIFF-EPStandardID";
    exifHash[41495] = "SensingMethod";
    exifHash[41728] = "FileSource";
    exifHash[41729] = "SceneType";
    exifHash[41730] = "CFAPattern";
    exifHash[41985] = "CustomRendered";
    exifHash[41986] = "ExposureMode";
    exifHash[41987] = "WhiteBalance";
    exifHash[41988] = "DigitalZoomRatio";
    exifHash[41989] = "FocalLengthIn35mmFormat";
    exifHash[41990] = "SceneCaptureType";
    exifHash[41991] = "GainControl";
    exifHash[41992] = "Contrast";
    exifHash[41993] = "Saturation";
    exifHash[41994] = "Sharpness";
    exifHash[41995] = "DeviceSettingDescription";
    exifHash[41996] = "SubjectDistanceRange";
    exifHash[42016] = "ImageUniqueID";
    exifHash[42032] = "OwnerName";
    exifHash[42033] = "SerialNumber";
    exifHash[42034] = "LensInfo";
    exifHash[42035] = "LensMake";
    exifHash[42036] = "LensModel";
    exifHash[42037] = "LensSerialNumber";
    exifHash[42112] = "GDALMetadata";
    exifHash[42113] = "GDALNoData";
    exifHash[42240] = "Gamma";
    exifHash[44992] = "ExpandSoftware";
    exifHash[44993] = "ExpandLens";
    exifHash[44994] = "ExpandFilm";
    exifHash[44995] = "ExpandFilterLens";
    exifHash[44996] = "ExpandScanner";
    exifHash[44997] = "ExpandFlashLamp";
    exifHash[48129] = "PixelFormat";
    exifHash[48130] = "Transformation";
    exifHash[48131] = "Uncompressed";
    exifHash[48132] = "ImageType";
    exifHash[48256] = "ImageWidth";
    exifHash[48257] = "ImageHeight";
    exifHash[48258] = "WidthResolution";
    exifHash[48259] = "HeightResolution";
    exifHash[48320] = "ImageOffset";
    exifHash[48321] = "ImageByteCount";
    exifHash[48322] = "AlphaOffset";
    exifHash[48323] = "AlphaByteCount";
    exifHash[48324] = "ImageDataDiscard";
    exifHash[48325] = "AlphaDataDiscard";
    exifHash[50215] = "OceScanjobDesc";
    exifHash[50216] = "OceApplicationSelector";
    exifHash[50217] = "OceIDNumber";
    exifHash[50218] = "OceImageLogic";
    exifHash[50255] = "Annotations";
    exifHash[50341] = "PrintIM";
    exifHash[50547] = "OriginalFileName";
    exifHash[50560] = "USPTOOriginalContentType";
    exifHash[50656] = "CR2CFAPattern";
    exifHash[50706] = "DNGVersion";
    exifHash[50707] = "DNGBackwardVersion";
    exifHash[50708] = "UniqueCameraModel";
    exifHash[50709] = "LocalizedCameraModel";
    exifHash[50710] = "CFAPlaneColor";
    exifHash[50711] = "CFALayout";
    exifHash[50712] = "LinearizationTable";
    exifHash[50713] = "BlackLevelRepeatDim";
    exifHash[50714] = "BlackLevel";
    exifHash[50715] = "BlackLevelDeltaH";
    exifHash[50716] = "BlackLevelDeltaV";
    exifHash[50717] = "WhiteLevel";
    exifHash[50718] = "DefaultScale";
    exifHash[50719] = "DefaultCropOrigin";
    exifHash[50720] = "DefaultCropSize";
    exifHash[50721] = "ColorMatrix1";
    exifHash[50722] = "ColorMatrix2";
    exifHash[50723] = "CameraCalibration1";
    exifHash[50724] = "CameraCalibration2";
    exifHash[50725] = "ReductionMatrix1";
    exifHash[50726] = "ReductionMatrix2";
    exifHash[50727] = "AnalogBalance";
    exifHash[50728] = "AsShotNeutral";
    exifHash[50729] = "AsShotWhiteXY";
    exifHash[50730] = "BaselineExposure";
    exifHash[50731] = "BaselineNoise";
    exifHash[50732] = "BaselineSharpness";
    exifHash[50733] = "BayerGreenSplit";
    exifHash[50734] = "LinearResponseLimit";
    exifHash[50735] = "CameraSerialNumber";
    exifHash[50736] = "DNGLensInfo";
    exifHash[50737] = "ChromaBlurRadius";
    exifHash[50738] = "AntiAliasStrength";
    exifHash[50739] = "ShadowScale";
    exifHash[50740] = "SR2Private ";
    exifHash[50741] = "MakerNoteSafety";
    exifHash[50752] = "RawImageSegmentation";
    exifHash[50778] = "CalibrationIlluminant1";
    exifHash[50779] = "CalibrationIlluminant2";
    exifHash[50780] = "BestQualityScale";
    exifHash[50781] = "RawDataUniqueID";
    exifHash[50784] = "AliasLayerMetadata";
    exifHash[50827] = "OriginalRawFileName";
    exifHash[50828] = "OriginalRawFileData";
    exifHash[50829] = "ActiveArea";
    exifHash[50830] = "MaskedAreas";
    exifHash[50831] = "AsShotICCProfile";
    exifHash[50832] = "AsShotPreProfileMatrix";
    exifHash[50833] = "CurrentICCProfile";
    exifHash[50834] = "CurrentPreProfileMatrix";
    exifHash[50879] = "ColorimetricReference";
    exifHash[50885] = "SRawType";
    exifHash[50898] = "PanasonicTitle";
    exifHash[50899] = "PanasonicTitle2";
    exifHash[50931] = "CameraCalibrationSig";
    exifHash[50932] = "ProfileCalibrationSig";
    exifHash[50933] = "ProfileIFD";
    exifHash[50934] = "AsShotProfileName";
    exifHash[50935] = "NoiseReductionApplied";
    exifHash[50936] = "ProfileName";
    exifHash[50937] = "ProfileHueSatMapDims";
    exifHash[50938] = "ProfileHueSatMapData1";
    exifHash[50939] = "ProfileHueSatMapData2";
    exifHash[50940] = "ProfileToneCurve";
    exifHash[50941] = "ProfileEmbedPolicy";
    exifHash[50942] = "ProfileCopyright";
    exifHash[50964] = "ForwardMatrix1";
    exifHash[50965] = "ForwardMatrix2";
    exifHash[50966] = "PreviewApplicationName";
    exifHash[50967] = "PreviewApplicationVersion";
    exifHash[50968] = "PreviewSettingsName";
    exifHash[50969] = "PreviewSettingsDigest";
    exifHash[50970] = "PreviewColorSpace";
    exifHash[50971] = "PreviewDateTime";
    exifHash[50972] = "RawImageDigest";
    exifHash[50973] = "OriginalRawFileDigest";
    exifHash[50974] = "SubTileBlockSize";
    exifHash[50975] = "RowInterleaveFactor";
    exifHash[50981] = "ProfileLookTableDims";
    exifHash[50982] = "ProfileLookTableData";
    exifHash[51008] = "OpcodeList1";
    exifHash[51009] = "OpcodeList2";
    exifHash[51022] = "OpcodeList3";
    exifHash[51041] = "NoiseProfile";
    exifHash[51043] = "TimeCodes";
    exifHash[51044] = "FrameRate";
    exifHash[51058] = "TStop";
    exifHash[51081] = "ReelName";
    exifHash[51089] = "OriginalDefaultFinalSize";
    exifHash[51090] = "OriginalBestQualitySize";
    exifHash[51091] = "OriginalDefaultCropSize";
    exifHash[51105] = "CameraLabel";
    exifHash[51107] = "ProfileHueSatMapEncoding";
    exifHash[51108] = "ProfileLookTableEncoding";
    exifHash[51109] = "BaselineExposureOffset";
    exifHash[51110] = "DefaultBlackRender";
    exifHash[51111] = "NewRawImageDigest";
    exifHash[51112] = "RawToPreviewGain";
    exifHash[51125] = "DefaultUserCrop";
    exifHash[59932] = "Padding";
    exifHash[59933] = "OffsetSchema";
    exifHash[65000] = "OwnerName";
    exifHash[65001] = "SerialNumber";
    exifHash[65002] = "Lens";
    exifHash[65024] = "KDC_IFD";
    exifHash[65100] = "RawFile";
    exifHash[65101] = "Converter";
    exifHash[65102] = "WhiteBalance";
    exifHash[65105] = "Exposure";
    exifHash[65106] = "Shadows";
    exifHash[65107] = "Brightness";
    exifHash[65108] = "Contrast";
    exifHash[65109] = "Saturation";
    exifHash[65110] = "Sharpness";
    exifHash[65111] = "Smoothness";
    exifHash[65112] = "MoireFilter";

}

void Metadata::initIfdHash()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    ifdHash[2] = "SensorWidth";
    ifdHash[3] = "SensorHeight";
    ifdHash[4] = "SensorTopBorder";
    ifdHash[5] = "SensorLeftBorder";
    ifdHash[6] = "SensorBottomBorder";
    ifdHash[7] = "SensorRightBorder";
    ifdHash[11] = "ProcessingSoftware";
    ifdHash[23] = "ISO";
    ifdHash[46] = "JpgOffset";
    ifdHash[49] = "PanasonicImageHeight";
    ifdHash[50] = "PanasonicImageWidth";
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

void Metadata::initOrientationHash()
{
    orientationDescription[1] = "Horizontal";
    orientationDescription[2] = "Mirrow horizontal";
    orientationDescription[3] = "Rotate 180";
    orientationDescription[4] = "Mirror vertical";
    orientationDescription[5] = "Mirror horizontal and rotate 270";
    orientationDescription[6] = "Rotate 90";
    orientationDescription[7] = "Mirror horizontal and rotate 90";
    orientationDescription[8] = "Rotate 270";

    orientationToDegrees[1] = 0;
    orientationToDegrees[3] = 180;
    orientationToDegrees[6] = 90;
    orientationToDegrees[8] = 270;

    orientationFromDegrees[0] = 1;
    orientationFromDegrees[90] = 6;
    orientationFromDegrees[180] = 3;
    orientationFromDegrees[270] = 8;
}

void Metadata::initSonyMakerHash()
{
    sonyMakerHash[258] = "Image quality";
    sonyMakerHash[260] = "Flash exposure compensation in EV";
    sonyMakerHash[261] = "Teleconverter Model";
    sonyMakerHash[274] = "White Balance Fine Tune Value";
    sonyMakerHash[276] = "Camera Settings";
    sonyMakerHash[277] = "White balance";
    sonyMakerHash[278] = "Unknown";
    sonyMakerHash[3584] = "PrintIM information";
    sonyMakerHash[4096] = "Multi Burst Mode";
    sonyMakerHash[4097] = "Multi Burst Image Width";
    sonyMakerHash[4098] = "Multi Burst Image Height";
    sonyMakerHash[4099] = "Panorama";
    sonyMakerHash[8192] = "Unknown";
    sonyMakerHash[8193] = "JPEG preview image";
    sonyMakerHash[8194] = "Unknown";
    sonyMakerHash[8195] = "Unknown";
    sonyMakerHash[8196] = "Contrast";
    sonyMakerHash[8197] = "Saturation";
    sonyMakerHash[8198] = "Unknown";
    sonyMakerHash[8199] = "Unknown";
    sonyMakerHash[8200] = "Unknown";
    sonyMakerHash[8201] = "Unknown";
    sonyMakerHash[8202] = "High Definition Range Mode";
    sonyMakerHash[8203] = "Multi Frame Noise Reduction";
    sonyMakerHash[12288] = "Shot Information IFD";
    sonyMakerHash[45056] = "File Format";
    sonyMakerHash[45057] = "Sony Model ID";
    sonyMakerHash[45088] = "Color Reproduction";
    sonyMakerHash[45089] = "Color Temperature";
    sonyMakerHash[45090] = "Color Compensation Filter";
    sonyMakerHash[45091] = "Scene Mode";
    sonyMakerHash[45092] = "Zone Matching";
    sonyMakerHash[45093] = "Dynamic Range Optimizer";
    sonyMakerHash[45094] = "Image stabilization";
    sonyMakerHash[45095] = "Lens identifier";
    sonyMakerHash[45096] = "Minolta MakerNote";
    sonyMakerHash[45097] = "Color Mode";
    sonyMakerHash[45099] = "Full Image Size";
    sonyMakerHash[45100] = "Preview image size";
    sonyMakerHash[45120] = "Macro";
    sonyMakerHash[45121] = "Exposure Mode";
    sonyMakerHash[45122] = "Focus Mode";
    sonyMakerHash[45123] = "AF Mode";
    sonyMakerHash[45124] = "AF Illuminator";
    sonyMakerHash[45127] = "JPEG Quality";
    sonyMakerHash[45128] = "Flash Level";
    sonyMakerHash[45129] = "Release Mode";
    sonyMakerHash[45130] = "Shot number in continuous burst mode";
    sonyMakerHash[45131] = "Anti-Blur";
    sonyMakerHash[45134] = "Long Exposure Noise Reduction";
    sonyMakerHash[45135] = "Dynamic Range Optimizer";
    sonyMakerHash[45138] = "Intelligent Auto";
    sonyMakerHash[45140] = "White balance 2";
}

void Metadata::initFujiMakerHash()
{
    fujiMakerHash[0] = "Version";
    fujiMakerHash[16] = "InternalSerialNumber";
    fujiMakerHash[4096] = "Quality";
    fujiMakerHash[4097] = "Sharpness";
    fujiMakerHash[4098] = "WhiteBalance";
    fujiMakerHash[4099] = "Saturation";
    fujiMakerHash[4100] = "Contrast";
    fujiMakerHash[4101] = "ColorTemperature";
    fujiMakerHash[4102] = "Contrast";
    fujiMakerHash[4106] = "WhiteBalanceFineTune";
    fujiMakerHash[4107] = "NoiseReduction";
    fujiMakerHash[4110] = "HighISONoiseReduction";
    fujiMakerHash[4112] = "FujiFlashMode";
    fujiMakerHash[4113] = "FlashExposureComp";
    fujiMakerHash[4128] = "Macro";
    fujiMakerHash[4129] = "FocusMode";
    fujiMakerHash[4130] = "AFMode";
    fujiMakerHash[4131] = "FocusPixel";
    fujiMakerHash[4144] = "SlowSync";
    fujiMakerHash[4145] = "PictureMode";
    fujiMakerHash[4146] = "ExposureCount";
    fujiMakerHash[4147] = "EXRAuto";
    fujiMakerHash[4148] = "EXRMode";
    fujiMakerHash[4160] = "ShadowTone";
    fujiMakerHash[4161] = "HighlightTone";
    fujiMakerHash[4164] = "DigitalZoom";
    fujiMakerHash[4176] = "ShutterType";
    fujiMakerHash[4352] = "AutoBracketing";
    fujiMakerHash[4353] = "SequenceNumber";
    fujiMakerHash[4435] = "PanoramaAngle";
    fujiMakerHash[4436] = "PanoramaDirection";
    fujiMakerHash[4609] = "AdvancedFilter";
    fujiMakerHash[4624] = "ColorMode";
    fujiMakerHash[4864] = "BlurWarning";
    fujiMakerHash[4865] = "FocusWarning";
    fujiMakerHash[4866] = "ExposureWarning";
    fujiMakerHash[4868] = "GEImageSize";
    fujiMakerHash[5120] = "DynamicRange";
    fujiMakerHash[5121] = "FilmMode";
    fujiMakerHash[5122] = "DynamicRangeSetting";
    fujiMakerHash[5123] = "DevelopmentDynamicRange";
    fujiMakerHash[5124] = "MinFocalLength";
    fujiMakerHash[5125] = "MaxFocalLength";
    fujiMakerHash[5126] = "MaxApertureAtMinFocal";
    fujiMakerHash[5127] = "MaxApertureAtMaxFocal";
    fujiMakerHash[5131] = "AutoDynamicRange";
    fujiMakerHash[5154] = "ImageStabilization";
    fujiMakerHash[5157] = "SceneRecognition";
    fujiMakerHash[5169] = "Rating";
    fujiMakerHash[5174] = "ImageGeneration";
    fujiMakerHash[5176] = "ImageCount";
    fujiMakerHash[14368] = "FrameRate";
    fujiMakerHash[14369] = "FrameWidth";
    fujiMakerHash[14370] = "FrameHeight";
    fujiMakerHash[16640] = "FacesDetected";
    fujiMakerHash[16643] = "FacePositions";
    fujiMakerHash[16896] = "NumFaceElements";
    fujiMakerHash[16897] = "FaceElementTypes";
    fujiMakerHash[16899] = "FaceElementPositions";
    fujiMakerHash[17026] = "FaceRecInfo";
    fujiMakerHash[32768] = "FileSource";
    fujiMakerHash[32770] = "OrderNumber";
    fujiMakerHash[32771] = "FrameNumber";
    fujiMakerHash[45585] = "Parallax";
}

void Metadata::initPanasonicMakerHash()
{
    panasonicMakerHash[1] = "ImageQuality";
    panasonicMakerHash[2] = "FirmwareVersion";
    panasonicMakerHash[3] = "WhiteBalance";
    panasonicMakerHash[7] = "FocusMode";
    panasonicMakerHash[15] = "AFAreaMode";
    panasonicMakerHash[26] = "ImageStabilization";
    panasonicMakerHash[28] = "MacroMode";
    panasonicMakerHash[31] = "ShootingMode";
    panasonicMakerHash[32] = "Audio";
    panasonicMakerHash[33] = "DataDump";
    panasonicMakerHash[35] = "WhiteBalanceBias";
    panasonicMakerHash[36] = "FlashBias";
    panasonicMakerHash[37] = "InternalSerialNumber";
    panasonicMakerHash[38] = "PanasonicExifVersion";
    panasonicMakerHash[40] = "ColorEffect";
    panasonicMakerHash[41] = "TimeSincePowerOn";
    panasonicMakerHash[42] = "BurstMode";
    panasonicMakerHash[43] = "SequenceNumber";
    panasonicMakerHash[44] = "ContrastMode";
    panasonicMakerHash[45] = "NoiseReduction";
    panasonicMakerHash[46] = "SelfTimer";
    panasonicMakerHash[48] = "Rotation";
    panasonicMakerHash[49] = "AFAssistLamp";
    panasonicMakerHash[50] = "ColorMode";
    panasonicMakerHash[51] = "BabyAge";
    panasonicMakerHash[52] = "OpticalZoomMode";
    panasonicMakerHash[53] = "ConversionLens";
    panasonicMakerHash[54] = "TravelDay";
    panasonicMakerHash[57] = "Contrast";
    panasonicMakerHash[58] = "WorldTimeLocation";
    panasonicMakerHash[59] = "TextStamp";
    panasonicMakerHash[60] = "ProgramISO";
    panasonicMakerHash[61] = "AdvancedSceneType";
    panasonicMakerHash[62] = "TextStamp";
    panasonicMakerHash[63] = "FacesDetected";
    panasonicMakerHash[64] = "Saturation";
    panasonicMakerHash[65] = "Sharpness";
    panasonicMakerHash[66] = "FilmMode";
    panasonicMakerHash[67] = "JPEGQuality";
    panasonicMakerHash[68] = "ColorTempKelvin";
    panasonicMakerHash[69] = "BracketSettings";
    panasonicMakerHash[70] = "WBShiftAB";
    panasonicMakerHash[71] = "WBShiftGM";
    panasonicMakerHash[72] = "FlashCurtain";
    panasonicMakerHash[73] = "LongExposureNoiseReduction";
    panasonicMakerHash[75] = "PanasonicImageWidth";
    panasonicMakerHash[76] = "PanasonicImageHeight";
    panasonicMakerHash[77] = "AFPointPosition";
    panasonicMakerHash[78] = "FaceDetInfo";
    panasonicMakerHash[81] = "LensType";
    panasonicMakerHash[82] = "LensSerialNumber";
    panasonicMakerHash[83] = "AccessoryType";
    panasonicMakerHash[84] = "AccessorySerialNumber";
    panasonicMakerHash[89] = "Transform";
    panasonicMakerHash[93] = "IntelligentExposure";
    panasonicMakerHash[96] = "LensFirmwareVersion";
    panasonicMakerHash[97] = "FaceRecInfo";
    panasonicMakerHash[98] = "FlashWarning";
    panasonicMakerHash[99] = "RecognizedFaceFlags?";
    panasonicMakerHash[101] = "Title";
    panasonicMakerHash[102] = "BabyName";
    panasonicMakerHash[103] = "Location";
    panasonicMakerHash[105] = "Country";
    panasonicMakerHash[107] = "State";
    panasonicMakerHash[109] = "City";
    panasonicMakerHash[111] = "Landmark";
    panasonicMakerHash[112] = "IntelligentResolution";
    panasonicMakerHash[118] = "HDRShot";
    panasonicMakerHash[119] = "BurstSpeed";
    panasonicMakerHash[121] = "IntelligentD-Range";
    panasonicMakerHash[124] = "ClearRetouch";
    panasonicMakerHash[128] = "City2";
    panasonicMakerHash[134] = "ManometerPressure";
    panasonicMakerHash[137] = "PhotoStyle";
    panasonicMakerHash[138] = "ShadingCompensation";
    panasonicMakerHash[139] = "WBShiftIntelligentAuto";
    panasonicMakerHash[140] = "AccelerometerZ";
    panasonicMakerHash[141] = "AccelerometerX";
    panasonicMakerHash[142] = "AccelerometerY";
    panasonicMakerHash[143] = "CameraOrientation";
    panasonicMakerHash[144] = "RollAngle";
    panasonicMakerHash[145] = "PitchAngle";
    panasonicMakerHash[146] = "WBShiftCreativeControl";
    panasonicMakerHash[147] = "SweepPanoramaDirection";
    panasonicMakerHash[148] = "SweepPanoramaFieldOfView";
    panasonicMakerHash[150] = "TimerRecording";
    panasonicMakerHash[157] = "InternalNDFilter";
    panasonicMakerHash[158] = "HDR";
    panasonicMakerHash[159] = "ShutterType";
    panasonicMakerHash[163] = "ClearRetouchValue";
    panasonicMakerHash[167] = "OutputLUT";
    panasonicMakerHash[171] = "TouchAE";
    panasonicMakerHash[173] = "HighlightShadow";
    panasonicMakerHash[175] = "TimeStamp";
    panasonicMakerHash[180] = "MultiExposure";
    panasonicMakerHash[185] = "RedEyeRemoval";
    panasonicMakerHash[187] = "VideoBurstMode";
    panasonicMakerHash[188] = "DiffractionCorrection";
    panasonicMakerHash[3584] = "PrintIM";
    panasonicMakerHash[8195] = "TimeInfo";
    panasonicMakerHash[32768] = "MakerNoteVersion";
    panasonicMakerHash[32769] = "SceneMode";
}

void Metadata::initCanonMakerHash()
{
    canonMakerHash[0] = "Unknown";
    canonMakerHash[1] = "Various camera settings";
    canonMakerHash[2] = "Focal length";
    canonMakerHash[3] = "Unknown";
    canonMakerHash[4] = "Shot information";
    canonMakerHash[5] = "Panorama";
    canonMakerHash[6] = "Image type";
    canonMakerHash[7] = "Firmware version";
    canonMakerHash[8] = "File number";
    canonMakerHash[9] = "Owner Name";
    canonMakerHash[12] = "Camera serial number";
    canonMakerHash[13] = "Camera info";
    canonMakerHash[15] = "Custom Functions";
    canonMakerHash[16] = "Model ID";
    canonMakerHash[18] = "Picture info";
    canonMakerHash[19] = "Thumbnail image valid area";
    canonMakerHash[21] = "Serial number format";
    canonMakerHash[26] = "Super macro";
    canonMakerHash[38] = "AF info";
    canonMakerHash[131] = "Original decision data offset";
    canonMakerHash[147] = "FileInfo tags";
    canonMakerHash[149] = "Lens model";
    canonMakerHash[150] = "Internal serial number";
    canonMakerHash[151] = "Dust removal data";
    canonMakerHash[153] = "Custom functions";
    canonMakerHash[160] = "Processing info";
    canonMakerHash[164] = "White balance table";
    canonMakerHash[170] = "Measured color";
    canonMakerHash[180] = "ColorSpace";
    canonMakerHash[181] = "Unknown";
    canonMakerHash[192] = "Unknown";
    canonMakerHash[193] = "Unknown";
    canonMakerHash[208] = "VRD offset";
    canonMakerHash[224] = "Sensor info";
    canonMakerHash[16385] = "Color data";
    canonMakerHash[16386] = "CRWParam?";
    canonMakerHash[16387] = "ColorInfo Tags";
    canonMakerHash[16389] = "(unknown 49kB block, not copied to JPEG images)";
    canonMakerHash[16392] = "PictureStyleUserDef";
    canonMakerHash[16393] = "PictureStylePC";
    canonMakerHash[16400] = "CustomPictureStyleFileName";
    canonMakerHash[16403] = "AFMicroAdj";
    canonMakerHash[16405] = "VignettingCorr ";
    canonMakerHash[16406] = "Canon VignettingCorr2 Tags";
    canonMakerHash[16408] = "Canon LightingOpt Tags";
    canonMakerHash[16409] = "Canon LensInfo Tags";
    canonMakerHash[16416] = "Canon Ambience Tags";
    canonMakerHash[16417] = "Canon MultiExp Tags";
    canonMakerHash[16420] = "Canon FilterInfo Tags";
    canonMakerHash[16421] = "Canon HDRInfo Tags";
    canonMakerHash[16424] = "Canon AFConfig Tags";

    canonFileInfoHash[1] = "File number / Shutter count";
    canonFileInfoHash[3] = "Bracket mode";
    canonFileInfoHash[4] = "Bracket value";
    canonFileInfoHash[5] = "Bracket shot number";
    canonFileInfoHash[6] = "Raw JPG quality";
    canonFileInfoHash[7] = "Raw JPG size";
    canonFileInfoHash[8] = "Long exposure noise reduction";
    canonFileInfoHash[9] = "WB bracket mode";
    canonFileInfoHash[12] = "WB bracket value AB";
    canonFileInfoHash[13] = "WB bracket value GM";
    canonFileInfoHash[14] = "Filter effect";
    canonFileInfoHash[15] = "Toning effect";
    canonFileInfoHash[16] = "Macro magnification";
    canonFileInfoHash[19] = "Live view shooting";
    canonFileInfoHash[20] = "Focus distance upper";
    canonFileInfoHash[21] = "Focus distance lower";
    canonFileInfoHash[25] = "Flash exposure lock";
}

void Metadata::initNikonMakerHash()
{
    nikonMakerHash[1] = "Nikon Makernote version";
    nikonMakerHash[2] = "ISO speed setting";
    nikonMakerHash[3] = "Color mode";
    nikonMakerHash[4] = "Image quality setting";
    nikonMakerHash[5] = "White balance";
    nikonMakerHash[6] = "Image sharpening setting";
    nikonMakerHash[7] = "Focus mode";
    nikonMakerHash[8] = "Flash setting";
    nikonMakerHash[9] = "Flash device";
    nikonMakerHash[10] = "Unknown";
    nikonMakerHash[11] = "White balance bias";
    nikonMakerHash[12] = "WB RB levels";
    nikonMakerHash[13] = "Program shift";
    nikonMakerHash[14] = "Exposure difference";
    nikonMakerHash[15] = "ISO selection";
    nikonMakerHash[16] = "Data dump";
    nikonMakerHash[17] = "Offset to an IFD containing a preview image";
    nikonMakerHash[18] = "Flash compensation setting";
    nikonMakerHash[19] = "ISO setting";
    nikonMakerHash[22] = "Image boundary";
    nikonMakerHash[23] = "Flash exposure comp";
    nikonMakerHash[24] = "Flash bracket compensation applied";
    nikonMakerHash[25] = "AE bracket compensation applied";
    nikonMakerHash[26] = "Image processing";
    nikonMakerHash[27] = "Crop high speed";
    nikonMakerHash[28] = "Exposure tuning";
    nikonMakerHash[29] = "Serial Number";
    nikonMakerHash[30] = "Color space";
    nikonMakerHash[31] = "VR info";
    nikonMakerHash[32] = "Image authentication";
    nikonMakerHash[34] = "ActiveD-lighting";
    nikonMakerHash[35] = "Picture control";
    nikonMakerHash[36] = "World time";
    nikonMakerHash[37] = "ISO info";
    nikonMakerHash[42] = "Vignette control";
    nikonMakerHash[128] = "Image adjustment setting";
    nikonMakerHash[129] = "Tone compensation";
    nikonMakerHash[130] = "Auxiliary lens adapter";
    nikonMakerHash[131] = "Lens type";
    nikonMakerHash[132] = "Lens";
    nikonMakerHash[133] = "Manual focus distance";
    nikonMakerHash[134] = "Digital zoom setting";
    nikonMakerHash[135] = "Mode of flash used";
    nikonMakerHash[136] = "AF info";
    nikonMakerHash[137] = "Shooting mode";
    nikonMakerHash[138] = "Auto bracket release";
    nikonMakerHash[139] = "Lens FStops";
    nikonMakerHash[140] = "Contrast curve";
    nikonMakerHash[141] = "Color hue";
    nikonMakerHash[143] = "Scene mode";
    nikonMakerHash[144] = "Light source";
    nikonMakerHash[145] = "Shot info";
    nikonMakerHash[146] = "Hue adjustment";
    nikonMakerHash[147] = "NEF compression";
    nikonMakerHash[148] = "Saturation";
    nikonMakerHash[149] = "Noise reduction";
    nikonMakerHash[150] = "Linearization table";
    nikonMakerHash[151] = "Color balance";
    nikonMakerHash[152] = "Lens data settings";
    nikonMakerHash[153] = "Raw image center";
    nikonMakerHash[154] = "Sensor pixel size";
    nikonMakerHash[155] = "Unknown";
    nikonMakerHash[156] = "Scene assist";
    nikonMakerHash[158] = "Retouch history";
    nikonMakerHash[159] = "Unknown";
    nikonMakerHash[160] = "Camera serial number";
    nikonMakerHash[162] = "Image data size";
    nikonMakerHash[163] = "Unknown";
    nikonMakerHash[165] = "Image count";
    nikonMakerHash[166] = "Deleted image count";
    nikonMakerHash[167] = "Number of shots taken by camera";
    nikonMakerHash[168] = "Flash info";
    nikonMakerHash[169] = "Image optimization";
    nikonMakerHash[170] = "Saturation";
    nikonMakerHash[171] = "Program variation";
    nikonMakerHash[172] = "Image stabilization";
    nikonMakerHash[173] = "AF response";
    nikonMakerHash[176] = "Multi exposure";
    nikonMakerHash[177] = "High ISO Noise Reduction";
    nikonMakerHash[179] = "Toning effect";
    nikonMakerHash[183] = "AF info 2";
    nikonMakerHash[184] = "File info";
    nikonMakerHash[185] = "AF tune";
    nikonMakerHash[3584] = "PrintIM information";
    nikonMakerHash[3585] = "Capture data";
    nikonMakerHash[3593] = "Capture version";
    nikonMakerHash[3598] = "Capture offsets";
    nikonMakerHash[3600] = "Scan IFD";
    nikonMakerHash[3613] = "ICC profile";
    nikonMakerHash[3614] = "Capture output";
}

void Metadata::initNikonLensHash()
{
    nikonLensHash["0000000000000001"] = "Manual Lens No CPU";
    nikonLensHash["000000000000E112"] = "TC-17E II";
    nikonLensHash["000000000000F10C"] = "TC-14E [II] or Sigma APO Tele Converter 1.4x EX DG or Kenko Teleplus PRO 300 DG 1.4x";
    nikonLensHash["000000000000F218"] = "TC-20E [II] or Sigma APO Tele Converter 2x EX DG or Kenko Teleplus PRO 300 DG 2.0x";
    nikonLensHash["0000484853530001"] = "Loreo 40mm F11-22 3D Lens in a Cap 9005";
    nikonLensHash["00361C2D343C0006"] = "Tamron SP AF 11-18mm f/4.5-5.6 Di II LD Aspherical (IF) (A13)";
    nikonLensHash["003C1F3730300006"] = "Tokina AT-X 124 AF PRO DX (AF 12-24mm f/4)";
    nikonLensHash["003C2B4430300006"] = "Tokina AT-X 17-35 F4 PRO FX (AF 17-35mm f/4)";
    nikonLensHash["003C5C803030000E"] = "Tokina AT-X 70-200 F4 FX VCM-S (AF 70-200mm f/4)";
    nikonLensHash["003E80A0383F0002"] = "Tamron SP AF 200-500mm f/5-6.3 Di LD (IF) (A08)";
    nikonLensHash["003F2D802B400006"] = "Tamron AF 18-200mm f/3.5-6.3 XR Di II LD Aspherical (IF) (A14)";
    nikonLensHash["003F2D802C400006"] = "Tamron AF 18-200mm f/3.5-6.3 XR Di II LD Aspherical (IF) Macro (A14)";
    nikonLensHash["003F80A0383F0002"] = "Tamron SP AF 200-500mm f/5-6.3 Di (A08)";
    nikonLensHash["004011112C2C0000"] = "Samyang 8mm f/3.5 Fish-Eye";
    nikonLensHash["0040182B2C340006"] = "Tokina AT-X 107 AF DX Fisheye (AF 10-17mm f/3.5-4.5)";
    nikonLensHash["00402A722C3C0006"] = "Tokina AT-X 16.5-135 DX (AF 16.5-135mm F3.5-5.6)";
    nikonLensHash["00402B2B2C2C0002"] = "Tokina AT-X 17 AF PRO (AF 17mm f/3.5)";
    nikonLensHash["00402D2D2C2C0000"] = "Carl Zeiss Distagon T* 3.5/18 ZF.2";
    nikonLensHash["00402D802C400006"] = "Tamron AF 18-200mm f/3.5-6.3 XR Di II LD Aspherical (IF) Macro (A14NII)";
    nikonLensHash["00402D882C400006"] = "Tamron AF 18-250mm f/3.5-6.3 Di II LD Aspherical (IF) Macro (A18NII)";
    nikonLensHash["00402D882C406206"] = "Tamron AF 18-250mm f/3.5-6.3 Di II LD Aspherical (IF) Macro (A18)";
    nikonLensHash["004031312C2C0000"] = "Voigtlander Color Skopar 20mm F3.5 SLII Aspherical";
    nikonLensHash["004037802C3C0002"] = "Tokina AT-X 242 AF (AF 24-200mm f/3.5-5.6)";
    nikonLensHash["004064642C2C0000"] = "Voigtlander APO-Lanthar 90mm F3.5 SLII Close Focus";
    nikonLensHash["00446098343C0002"] = "Tokina AT-X 840 D (AF 80-400mm f/4.5-5.6)";
    nikonLensHash["0047101024240000"] = "Fisheye Nikkor 8mm f/2.8 AiS";
    nikonLensHash["0047252524240002"] = "Tamron SP AF 14mm f/2.8 Aspherical (IF) (69E)";
    nikonLensHash["00473C3C24240000"] = "Nikkor 28mm f/2.8 AiS";
    nikonLensHash["0047444424240006"] = "Tokina AT-X M35 PRO DX (AF 35mm f/2.8 Macro)";
    nikonLensHash["00475380303C0006"] = "Tamron AF 55-200mm f/4-5.6 Di II LD (A15)";
    nikonLensHash["00481C2924240006"] = "Tokina AT-X 116 PRO DX (AF 11-16mm f/2.8)";
    nikonLensHash["0048293C24240006"] = "Tokina AT-X 16-28 AF PRO FX (AF 16-28mm f/2.8)";
    nikonLensHash["0048295024240006"] = "Tokina AT-X 165 PRO DX (AF 16-50mm f/2.8)";
    nikonLensHash["0048323224240000"] = "Carl Zeiss Distagon T* 2.8/21 ZF.2";
    nikonLensHash["0048375C24240006"] = "Tokina AT-X 24-70 F2.8 PRO FX (AF 24-70mm f/2.8)";
    nikonLensHash["00483C3C24240000"] = "Voigtlander Color Skopar 28mm F2.8 SL II";
    nikonLensHash["00483C6024240002"] = "Tokina AT-X 280 AF PRO (AF 28-80mm f/2.8)";
    nikonLensHash["00483C6A24240002"] = "Tamron SP AF 28-105mm f/2.8 LD Aspherical IF (176D)";
    nikonLensHash["0048505018180000"] = "Nikkor H 50mm f/2";
    nikonLensHash["0048507224240006"] = "Tokina AT-X 535 PRO DX (AF 50-135mm f/2.8)";
    nikonLensHash["00485C803030000E"] = "Tokina AT-X 70-200 F4 FX VCM-S (AF 70-200mm f/4)";
    nikonLensHash["00485C8E303C0006"] = "Tamron AF 70-300mm f/4-5.6 Di LD Macro 1:2 (A17NII)";
    nikonLensHash["0048686824240000"] = "Series E 100mm f/2.8";
    nikonLensHash["0048808030300000"] = "Nikkor 200mm f/4 AiS";
    nikonLensHash["00493048222B0002"] = "Tamron SP AF 20-40mm f/2.7-3.5 (166D)";
    nikonLensHash["004C6A6A20200000"] = "Nikkor 105mm f/2.5 AiS";
    nikonLensHash["004C7C7C2C2C0002"] = "Tamron SP AF 180mm f/3.5 Di Model (B01)";
    nikonLensHash["00532B5024240006"] = "Tamron SP AF 17-50mm f/2.8 XR Di II LD Aspherical (IF) (A16)";
    nikonLensHash["00542B5024240006"] = "Tamron SP AF 17-50mm f/2.8 XR Di II LD Aspherical (IF) (A16NII)";
    nikonLensHash["0054383818180000"] = "Carl Zeiss Distagon T* 2/25 ZF.2";
    nikonLensHash["00543C3C18180000"] = "Carl Zeiss Distagon T* 2/28 ZF.2";
    nikonLensHash["005444440C0C0000"] = "Carl Zeiss Distagon T* 1.4/35 ZF.2";
    nikonLensHash["0054444418180000"] = "Carl Zeiss Distagon T* 2/35 ZF.2";
    nikonLensHash["0054484818180000"] = "Voigtlander Ultron 40mm F2 SLII Aspherical";
    nikonLensHash["005450500C0C0000"] = "Carl Zeiss Planar T* 1.4/50 ZF.2";
    nikonLensHash["0054505018180000"] = "Carl Zeiss Makro-Planar T* 2/50 ZF.2";
    nikonLensHash["005453530C0C0000"] = "Zeiss Otus 1.4/55";
    nikonLensHash["005455550C0C0000"] = "Voigtlander Nokton 58mm F1.4 SLII";
    nikonLensHash["0054565630300000"] = "Coastal Optical Systems 60mm 1:4 UV-VIS-IR Macro Apo";
    nikonLensHash["005462620C0C0000"] = "Carl Zeiss Planar T* 1.4/85 ZF.2";
    nikonLensHash["0054686818180000"] = "Carl Zeiss Makro-Planar T* 2/100 ZF.2";
    nikonLensHash["0054686824240002"] = "Tokina AT-X M100 AF PRO D (AF 100mm f/2.8 Macro)";
    nikonLensHash["0054727218180000"] = "Carl Zeiss Apo Sonnar T* 2/135 ZF.2";
    nikonLensHash["00548E8E24240002"] = "Tokina AT-X 300 AF PRO (AF 300mm f/2.8)";
    nikonLensHash["0057505014140000"] = "Nikkor 50mm f/1.8 AI";
    nikonLensHash["0058646420200000"] = "Soligor C/D Macro MC 90mm f/2.5";
    nikonLensHash["0100000000000200"] = "TC-16A";
    nikonLensHash["0100000000000800"] = "TC-16A";
    nikonLensHash["015462620C0C0000"] = "Zeiss Otus 1.4/85";
    nikonLensHash["0158505014140200"] = "AF Nikkor 50mm f/1.8";
    nikonLensHash["0158505014140500"] = "AF Nikkor 50mm f/1.8";
    nikonLensHash["022F98983D3D0200"] = "Sigma APO 400mm F5.6";
    nikonLensHash["0234A0A044440200"] = "Sigma APO 500mm F7.2";
    nikonLensHash["02375E8E353D0200"] = "Sigma 75-300mm F4.5-5.6 APO";
    nikonLensHash["0237A0A034340200"] = "Sigma APO 500mm F4.5";
    nikonLensHash["023A3750313D0200"] = "Sigma 24-50mm F4-5.6 UC";
    nikonLensHash["023A5E8E323D0200"] = "Sigma 75-300mm F4.0-5.6";
    nikonLensHash["023B4461303D0200"] = "Sigma 35-80mm F4-5.6";
    nikonLensHash["023CB0B03C3C0200"] = "Sigma APO 800mm F5.6";
    nikonLensHash["023F24242C2C0200"] = "Sigma 14mm F3.5";
    nikonLensHash["023F3C5C2D350200"] = "Sigma 28-70mm F3.5-4.5 UC";
    nikonLensHash["0240445C2C340200"] = "Exakta AF 35-70mm 1:3.5-4.5 MC";
    nikonLensHash["024044732B360200"] = "Sigma 35-135mm F3.5-4.5 a";
    nikonLensHash["02405C822C350200"] = "Sigma APO 70-210mm F3.5-4.5";
    nikonLensHash["0242445C2A340200"] = "AF Zoom-Nikkor 35-70mm f/3.3-4.5";
    nikonLensHash["0242445C2A340800"] = "AF Zoom-Nikkor 35-70mm f/3.3-4.5";
    nikonLensHash["0246373725250200"] = "Sigma 24mm F2.8 Super Wide II Macro";
    nikonLensHash["02463C5C25250200"] = "Sigma 28-70mm F2.8";
    nikonLensHash["02465C8225250200"] = "Sigma 70-210mm F2.8 APO";
    nikonLensHash["0248505024240200"] = "Sigma Macro 50mm F2.8";
    nikonLensHash["0248656524240200"] = "Sigma Macro 90mm F2.8";
    nikonLensHash["03435C8135350200"] = "Soligor AF C/D Zoom UMCS 70-210mm 1:4.5";
    nikonLensHash["03485C8130300200"] = "AF Zoom-Nikkor 70-210mm f/4";
    nikonLensHash["04483C3C24240300"] = "AF Nikkor 28mm f/2.8";
    nikonLensHash["055450500C0C0400"] = "AF Nikkor 50mm f/1.4";
    nikonLensHash["063F68682C2C0600"] = "Cosina AF 100mm F3.5 Macro";
    nikonLensHash["0654535324240600"] = "AF Micro-Nikkor 55mm f/2.8";
    nikonLensHash["07363D5F2C3C0300"] = "Cosina AF Zoom 28-80mm F3.5-5.6 MC Macro";
    nikonLensHash["073E30432D350300"] = "Soligor AF Zoom 19-35mm 1:3.5-4.5 MC";
    nikonLensHash["07402F442C340302"] = "Tamron AF 19-35mm f/3.5-4.5 (A10)";
    nikonLensHash["074030452D350302"] = "Tamron AF 19-35mm f/3.5-4.5 (A10)";
    nikonLensHash["07403C5C2C350300"] = "Tokina AF 270 II (AF 28-70mm f/3.5-4.5)";
    nikonLensHash["07403C622C340300"] = "AF Zoom-Nikkor 28-85mm f/3.5-4.5";
    nikonLensHash["07462B4424300302"] = "Tamron SP AF 17-35mm f/2.8-4 Di LD Aspherical (IF) (A05)";
    nikonLensHash["07463D6A252F0300"] = "Cosina AF Zoom 28-105mm F2.8-3.8 MC";
    nikonLensHash["07473C5C25350300"] = "Tokina AF 287 SD (AF 28-70mm f/2.8-4.5)";
    nikonLensHash["07483C5C24240300"] = "Tokina AT-X 287 AF (AF 28-70mm f/2.8)";
    nikonLensHash["0840446A2C340400"] = "AF Zoom-Nikkor 35-105mm f/3.5-4.5";
    nikonLensHash["0948373724240400"] = "AF Nikkor 24mm f/2.8";
    nikonLensHash["0A488E8E24240300"] = "AF Nikkor 300mm f/2.8 IF-ED";
    nikonLensHash["0A488E8E24240500"] = "AF Nikkor 300mm f/2.8 IF-ED N";
    nikonLensHash["0B3E3D7F2F3D0E00"] = "Tamron AF 28-200mm f/3.8-5.6 (71D)";
    nikonLensHash["0B3E3D7F2F3D0E02"] = "Tamron AF 28-200mm f/3.8-5.6D (171D)";
    nikonLensHash["0B487C7C24240500"] = "AF Nikkor 180mm f/2.8 IF-ED";
    nikonLensHash["0D4044722C340700"] = "AF Zoom-Nikkor 35-135mm f/3.5-4.5";
    nikonLensHash["0E485C8130300500"] = "AF Zoom-Nikkor 70-210mm f/4";
    nikonLensHash["0E4A3148232D0E02"] = "Tamron SP AF 20-40mm f/2.7-3.5 (166D)";
    nikonLensHash["0F58505014140500"] = "AF Nikkor 50mm f/1.8 N";
    nikonLensHash["103D3C602C3CD202"] = "Tamron AF 28-80mm f/3.5-5.6 Aspherical (177D)";
    nikonLensHash["10488E8E30300800"] = "AF Nikkor 300mm f/4 IF-ED";
    nikonLensHash["1148445C24240800"] = "AF Zoom-Nikkor 35-70mm f/2.8";
    nikonLensHash["1148445C24241500"] = "AF Zoom-Nikkor 35-70mm f/2.8";
    nikonLensHash["12365C81353D0900"] = "Cosina AF Zoom 70-210mm F4.5-5.6 MC Macro";
    nikonLensHash["1236699735420900"] = "Soligor AF Zoom 100-400mm 1:4.5-6.7 MC";
    nikonLensHash["1238699735420902"] = "Promaster Spectrum 7 100-400mm F4.5-6.7";
    nikonLensHash["12395C8E343D0802"] = "Cosina AF Zoom 70-300mm F4.5-5.6 MC Macro";
    nikonLensHash["123B688D3D430902"] = "Cosina AF Zoom 100-300mm F5.6-6.7 MC Macro";
    nikonLensHash["123B98983D3D0900"] = "Tokina AT-X 400 AF SD (AF 400mm f/5.6)";
    nikonLensHash["123D3C802E3CDF02"] = "Tamron AF 28-200mm f/3.8-5.6 AF Aspherical LD (IF) (271D)";
    nikonLensHash["12445E8E343C0900"] = "Tokina AF 730 (AF 75-300mm F4.5-5.6)";
    nikonLensHash["12485C81303C0900"] = "AF Nikkor 70-210mm f/4-5.6";
    nikonLensHash["124A5C81313D0900"] = "Soligor AF C/D Auto Zoom+Macro 70-210mm 1:4-5.6 UMCS";
    nikonLensHash["134237502A340B00"] = "AF Zoom-Nikkor 24-50mm f/3.3-4.5";
    nikonLensHash["1448608024240B00"] = "AF Zoom-Nikkor 80-200mm f/2.8 ED";
    nikonLensHash["1448688E30300B00"] = "Tokina AT-X 340 AF (AF 100-300mm f/4)";
    nikonLensHash["1454608024240B00"] = "Tokina AT-X 828 AF (AF 80-200mm f/2.8)";
    nikonLensHash["154C626214140C00"] = "AF Nikkor 85mm f/1.8";
    nikonLensHash["173CA0A030300F00"] = "Nikkor 500mm f/4 P ED IF";
    nikonLensHash["173CA0A030301100"] = "Nikkor 500mm f/4 P ED IF";
    nikonLensHash["184044722C340E00"] = "AF Zoom-Nikkor 35-135mm f/3.5-4.5 N";
    nikonLensHash["1A54444418181100"] = "AF Nikkor 35mm f/2";
    nikonLensHash["1B445E8E343C1000"] = "AF Zoom-Nikkor 75-300mm f/4.5-5.6";
    nikonLensHash["1C48303024241200"] = "AF Nikkor 20mm f/2.8";
    nikonLensHash["1D42445C2A341200"] = "AF Zoom-Nikkor 35-70mm f/3.3-4.5 N";
    nikonLensHash["1E54565624241300"] = "AF Micro-Nikkor 60mm f/2.8";
    nikonLensHash["1E5D646420201300"] = "Tamron SP AF 90mm f/2.5 (52E)";
    nikonLensHash["1F546A6A24241400"] = "AF Micro-Nikkor 105mm f/2.8";
    nikonLensHash["203C80983D3D1E02"] = "Tamron AF 200-400mm f/5.6 LD IF (75D)";
    nikonLensHash["2048608024241500"] = "AF Zoom-Nikkor 80-200mm f/2.8 ED";
    nikonLensHash["205A646420201400"] = "Tamron SP AF 90mm f/2.5 Macro (152E)";
    nikonLensHash["21403C5C2C341600"] = "AF Zoom-Nikkor 28-70mm f/3.5-4.5";
    nikonLensHash["21568E8E24241400"] = "Tamron SP AF 300mm f/2.8 LD-IF (60E)";
    nikonLensHash["2248727218181600"] = "AF DC-Nikkor 135mm f/2";
    nikonLensHash["225364642424E002"] = "Tamron SP AF 90mm f/2.8 Macro 1:1 (72E)";
    nikonLensHash["2330BECA3C481700"] = "Zoom-Nikkor 1200-1700mm f/5.6-8 P ED IF";
    nikonLensHash["24446098343C1A02"] = "Tokina AT-X 840 AF-II (AF 80-400mm f/4.5-5.6)";
    nikonLensHash["2448608024241A02"] = "AF Zoom-Nikkor 80-200mm f/2.8D ED";
    nikonLensHash["2454608024241A02"] = "Tokina AT-X 828 AF PRO (AF 80-200mm f/2.8)";
    nikonLensHash["2544448E34421B02"] = "Tokina AF 353 (AF 35-300mm f/4.5-6.7)";
    nikonLensHash["25483C5C24241B02"] = "Tokina AT-X 287 AF PRO SV (AF 28-70mm f/2.8)";
    nikonLensHash["2548445C24241B02"] = "AF Zoom-Nikkor 35-70mm f/2.8D";
    nikonLensHash["2548445C24243A02"] = "AF Zoom-Nikkor 35-70mm f/2.8D";
    nikonLensHash["2548445C24245202"] = "AF Zoom-Nikkor 35-70mm f/2.8D";
    nikonLensHash["263C5480303C1C06"] = "Sigma 55-200mm F4-5.6 DC";
    nikonLensHash["263C5C82303C1C02"] = "Sigma 70-210mm F4-5.6 UC-II";
    nikonLensHash["263C5C8E303C1C02"] = "Sigma 70-300mm F4-5.6 DG Macro";
    nikonLensHash["263C98983C3C1C02"] = "Sigma APO Tele Macro 400mm F5.6";
    nikonLensHash["263D3C802F3D1C02"] = "Sigma 28-300mm F3.8-5.6 Aspherical";
    nikonLensHash["263E3C6A2E3C1C02"] = "Sigma 28-105mm F3.8-5.6 UC-III Aspherical IF";
    nikonLensHash["2640273F2C341C02"] = "Sigma 15-30mm F3.5-4.5 EX DG Aspherical DF";
    nikonLensHash["26402D442B341C02"] = "Sigma 18-35mm F3.5-4.5 Aspherical";
    nikonLensHash["26402D502C3C1C06"] = "Sigma 18-50mm F3.5-5.6 DC";
    nikonLensHash["26402D702B3C1C06"] = "Sigma 18-125mm F3.5-5.6 DC";
    nikonLensHash["26402D802C401C06"] = "Sigma 18-200mm F3.5-6.3 DC";
    nikonLensHash["2640375C2C3C1C02"] = "Sigma 24-70mm F3.5-5.6 Aspherical HF";
    nikonLensHash["26403C5C2C341C02"] = "AF Zoom-Nikkor 28-70mm f/3.5-4.5D";
    nikonLensHash["26403C602C3C1C02"] = "Sigma 28-80mm F3.5-5.6 Mini Zoom Macro II Aspherical";
    nikonLensHash["26403C652C3C1C02"] = "Sigma 28-90mm F3.5-5.6 Macro";
    nikonLensHash["26403C802B3C1C02"] = "Sigma 28-200mm F3.5-5.6 Compact Aspherical Hyperzoom Macro";
    nikonLensHash["26403C802C3C1C02"] = "Sigma 28-200mm F3.5-5.6 Compact Aspherical Hyperzoom Macro";
    nikonLensHash["26403C8E2C401C02"] = "Sigma 28-300mm F3.5-6.3 Macro";
    nikonLensHash["26407BA034401C02"] = "Sigma APO 170-500mm F5-6.3 Aspherical RF";
    nikonLensHash["26413C8E2C401C02"] = "Sigma 28-300mm F3.5-6.3 DG Macro";
    nikonLensHash["26447398343C1C02"] = "Sigma 135-400mm F4.5-5.6 APO Aspherical";
    nikonLensHash["2648111130301C02"] = "Sigma 8mm F4 EX Circular Fisheye";
    nikonLensHash["2648272724241C02"] = "Sigma 15mm F2.8 EX Diagonal Fisheye";
    nikonLensHash["26482D5024241C06"] = "Sigma 18-50mm F2.8 EX DC";
    nikonLensHash["2648314924241C02"] = "Sigma 20-40mm F2.8";
    nikonLensHash["2648375624241C02"] = "Sigma 24-60mm F2.8 EX DG";
    nikonLensHash["26483C5C24241C06"] = "Sigma 28-70mm F2.8 EX DG";
    nikonLensHash["26483C5C24301C02"] = "Sigma 28-70mm F2.8-4 DG";
    nikonLensHash["26483C6A24301C02"] = "Sigma 28-105mm F2.8-4 Aspherical";
    nikonLensHash["26488E8E30301C02"] = "Sigma APO Tele Macro 300mm F4";
    nikonLensHash["26542B4424301C02"] = "Sigma 17-35mm F2.8-4 EX Aspherical";
    nikonLensHash["2654375C24241C02"] = "Sigma 24-70mm F2.8 EX DG Macro";
    nikonLensHash["2654377324341C02"] = "Sigma 24-135mm F2.8-4.5";
    nikonLensHash["26543C5C24241C02"] = "Sigma 28-70mm F2.8 EX";
    nikonLensHash["2658313114141C02"] = "Sigma 20mm F1.8 EX DG Aspherical RF";
    nikonLensHash["2658373714141C02"] = "Sigma 24mm F1.8 EX DG Aspherical Macro";
    nikonLensHash["26583C3C14141C02"] = "Sigma 28mm F1.8 EX DG Aspherical Macro";
    nikonLensHash["27488E8E24241D02"] = "AF-I Nikkor 300mm f/2.8D IF-ED";
    nikonLensHash["27488E8E2424E102"] = "AF-I Nikkor 300mm f/2.8D IF-ED + TC-17E";
    nikonLensHash["27488E8E2424F102"] = "AF-I Nikkor 300mm f/2.8D IF-ED + TC-14E";
    nikonLensHash["27488E8E2424F202"] = "AF-I Nikkor 300mm f/2.8D IF-ED + TC-20E";
    nikonLensHash["27488E8E30301D02"] = "Tokina AT-X 304 AF (AF 300mm f/4.0)";
    nikonLensHash["27548E8E24241D02"] = "Tamron SP AF 300mm f/2.8 LD-IF (360E)";
    nikonLensHash["283CA6A630301D02"] = "AF-I Nikkor 600mm f/4D IF-ED";
    nikonLensHash["283CA6A63030E102"] = "AF-I Nikkor 600mm f/4D IF-ED + TC-17E";
    nikonLensHash["283CA6A63030F102"] = "AF-I Nikkor 600mm f/4D IF-ED + TC-14E";
    nikonLensHash["283CA6A63030F202"] = "AF-I Nikkor 600mm f/4D IF-ED + TC-20E";
    nikonLensHash["2A543C3C0C0C2602"] = "AF Nikkor 28mm f/1.4D";
    nikonLensHash["2B3C4460303C1F02"] = "AF Zoom-Nikkor 35-80mm f/4-5.6D";
    nikonLensHash["2C486A6A18182702"] = "AF DC-Nikkor 105mm f/2D";
    nikonLensHash["2D48808030302102"] = "AF Micro-Nikkor 200mm f/4D IF-ED";
    nikonLensHash["2E485C82303C2202"] = "AF Nikkor 70-210mm f/4-5.6D";
    nikonLensHash["2E485C82303C2802"] = "AF Nikkor 70-210mm f/4-5.6D";
    nikonLensHash["2F4030442C342902"] = "Tokina AF 235 II (AF 20-35mm f/3.5-4.5)";
    nikonLensHash["2F48304424242902"] = "AF Zoom-Nikkor 20-35mm f/2.8D IF";
    nikonLensHash["3048989824242402"] = "AF-I Nikkor 400mm f/2.8D IF-ED";
    nikonLensHash["304898982424E102"] = "AF-I Nikkor 400mm f/2.8D IF-ED + TC-17E";
    nikonLensHash["304898982424F102"] = "AF-I Nikkor 400mm f/2.8D IF-ED + TC-14E";
    nikonLensHash["304898982424F202"] = "AF-I Nikkor 400mm f/2.8D IF-ED + TC-20E";
    nikonLensHash["3154565624242502"] = "AF Micro-Nikkor 60mm f/2.8D";
    nikonLensHash["3253646424243502"] = "Tamron SP AF 90mm f/2.8 [Di] Macro 1:1 (172E/272E)";
    nikonLensHash["3254505024243502"] = "Sigma Macro 50mm F2.8 EX DG";
    nikonLensHash["32546A6A24243502"] = "AF Micro-Nikkor 105mm f/2.8D";
    nikonLensHash["33482D2D24243102"] = "AF Nikkor 18mm f/2.8D";
    nikonLensHash["33543C5E24246202"] = "Tamron SP AF 28-75mm f/2.8 XR Di LD Aspherical (IF) Macro (A09)";
    nikonLensHash["3448292924243202"] = "AF Fisheye Nikkor 16mm f/2.8D";
    nikonLensHash["353CA0A030303302"] = "AF-I Nikkor 500mm f/4D IF-ED";
    nikonLensHash["353CA0A03030E102"] = "AF-I Nikkor 500mm f/4D IF-ED + TC-17E";
    nikonLensHash["353CA0A03030F102"] = "AF-I Nikkor 500mm f/4D IF-ED + TC-14E";
    nikonLensHash["353CA0A03030F202"] = "AF-I Nikkor 500mm f/4D IF-ED + TC-20E";
    nikonLensHash["3648373724243402"] = "AF Nikkor 24mm f/2.8D";
    nikonLensHash["3748303024243602"] = "AF Nikkor 20mm f/2.8D";
    nikonLensHash["384C626214143702"] = "AF Nikkor 85mm f/1.8D";
    nikonLensHash["3A403C5C2C343902"] = "AF Zoom-Nikkor 28-70mm f/3.5-4.5D";
    nikonLensHash["3B48445C24243A02"] = "AF Zoom-Nikkor 35-70mm f/2.8D N";
    nikonLensHash["3C48608024243B02"] = "AF Zoom-Nikkor 80-200mm f/2.8D ED";
    nikonLensHash["3D3C4460303C3E02"] = "AF Zoom-Nikkor 35-80mm f/4-5.6D";
    nikonLensHash["3E483C3C24243D02"] = "AF Nikkor 28mm f/2.8D";
    nikonLensHash["3F40446A2C344502"] = "AF Zoom-Nikkor 35-105mm f/3.5-4.5D";
    nikonLensHash["41487C7C24244302"] = "AF Nikkor 180mm f/2.8D IF-ED";
    nikonLensHash["4254444418184402"] = "AF Nikkor 35mm f/2D";
    nikonLensHash["435450500C0C4602"] = "AF Nikkor 50mm f/1.4D";
    nikonLensHash["44446080343C4702"] = "AF Zoom-Nikkor 80-200mm f/4.5-5.6D";
    nikonLensHash["453D3C602C3C4802"] = "Tamron AF 28-80mm f/3.5-5.6 Aspherical (177D)";
    nikonLensHash["45403C602C3C4802"] = "AF Zoom-Nikkor 28-80mm f/3.5-5.6D";
    nikonLensHash["454137722C3C4802"] = "Tamron SP AF 24-135mm f/3.5-5.6 AD Aspherical (IF) Macro (190D)";
    nikonLensHash["463C4460303C4902"] = "AF Zoom-Nikkor 35-80mm f/4-5.6D N";
    nikonLensHash["474237502A344A02"] = "AF Zoom-Nikkor 24-50mm f/3.3-4.5D";
    nikonLensHash["48381F37343C4B06"] = "Sigma 12-24mm F4.5-5.6 EX DG Aspherical HSM";
    nikonLensHash["483C1931303C4B06"] = "Sigma 10-20mm F4-5.6 EX DC HSM";
    nikonLensHash["483C50A030404B02"] = "Sigma 50-500mm F4-6.3 EX APO RF HSM";
    nikonLensHash["483C8EB03C3C4B02"] = "Sigma APO 300-800mm F5.6 EX DG HSM";
    nikonLensHash["483CB0B03C3C4B02"] = "Sigma APO 800mm F5.6 EX HSM";
    nikonLensHash["4844A0A034344B02"] = "Sigma APO 500mm F4.5 EX HSM";
    nikonLensHash["4848242424244B02"] = "Sigma 14mm F2.8 EX Aspherical HSM";
    nikonLensHash["48482B4424304B06"] = "Sigma 17-35mm F2.8-4 EX DG Aspherical HSM";
    nikonLensHash["4848688E30304B02"] = "Sigma APO 100-300mm F4 EX IF HSM";
    nikonLensHash["4848767624244B06"] = "Sigma APO Macro 150mm F2.8 EX DG HSM";
    nikonLensHash["48488E8E24244B02"] = "300mm f/2.8D IF-ED";
    nikonLensHash["48488E8E2424E102"] = "300mm f/2.8D IF-ED + TC-17E";
    nikonLensHash["48488E8E2424F102"] = "300mm f/2.8D IF-ED + TC-14E";
    nikonLensHash["48488E8E2424F202"] = "300mm f/2.8D IF-ED + TC-20E";
    nikonLensHash["484C7C7C2C2C4B02"] = "Sigma APO Macro 180mm F3.5 EX DG HSM";
    nikonLensHash["484C7D7D2C2C4B02"] = "Sigma APO Macro 180mm F3.5 EX DG HSM";
    nikonLensHash["48543E3E0C0C4B06"] = "Sigma 30mm F1.4 EX DC HSM";
    nikonLensHash["48545C8024244B02"] = "Sigma 70-200mm F2.8 EX APO IF HSM";
    nikonLensHash["48546F8E24244B02"] = "Sigma APO 120-300mm F2.8 EX DG HSM";
    nikonLensHash["48548E8E24244B02"] = "Sigma APO 300mm F2.8 EX DG HSM";
    nikonLensHash["493CA6A630304C02"] = "600mm f/4D IF-ED";
    nikonLensHash["493CA6A63030E102"] = "600mm f/4D IF-ED + TC-17E";
    nikonLensHash["493CA6A63030F102"] = "600mm f/4D IF-ED + TC-14E";
    nikonLensHash["493CA6A63030F202"] = "600mm f/4D IF-ED + TC-20E";
    nikonLensHash["4A4011112C0C4D02"] = "Samyang 8mm f/3.5 Fish-Eye CS";
    nikonLensHash["4A481E1E240C4D02"] = "Samyang 12mm f/2.8 ED AS NCS Fish-Eye";
    nikonLensHash["4A482424240C4D02"] = "Samyang AE 14mm f/2.8 ED AS IF UMC";
    nikonLensHash["4A542929180C4D02"] = "Samyang 16mm f/2.0 ED AS UMC CS";
    nikonLensHash["4A5462620C0C4D02"] = "AF Nikkor 85mm f/1.4D IF";
    nikonLensHash["4A6036360C0C4D02"] = "Samyang 24mm f/1.4 ED AS UMC";
    nikonLensHash["4A6044440C0C4D02"] = "Samyang 35mm f/1.4 AS UMC";
    nikonLensHash["4A6062620C0C4D02"] = "Samyang AE 85mm f/1.4 AS IF UMC";
    nikonLensHash["4B3CA0A030304E02"] = "500mm f/4D IF-ED";
    nikonLensHash["4B3CA0A03030E102"] = "500mm f/4D IF-ED + TC-17E";
    nikonLensHash["4B3CA0A03030F102"] = "500mm f/4D IF-ED + TC-14E";
    nikonLensHash["4B3CA0A03030F202"] = "500mm f/4D IF-ED + TC-20E";
    nikonLensHash["4C40376E2C3C4F02"] = "AF Zoom-Nikkor 24-120mm f/3.5-5.6D IF";
    nikonLensHash["4D3E3C802E3C6202"] = "Tamron AF 28-200mm f/3.8-5.6 XR Aspherical (IF) Macro (A03N)";
    nikonLensHash["4D403C802C3C6202"] = "AF Zoom-Nikkor 28-200mm f/3.5-5.6D IF";
    nikonLensHash["4D413C8E2B406202"] = "Tamron AF 28-300mm f/3.5-6.3 XR Di LD Aspherical (IF) (A061)";
    nikonLensHash["4D413C8E2C406202"] = "Tamron AF 28-300mm f/3.5-6.3 XR LD Aspherical (IF) (185D)";
    nikonLensHash["4E48727218185102"] = "AF DC-Nikkor 135mm f/2D";
    nikonLensHash["4F40375C2C3C5306"] = "IX-Nikkor 24-70mm f/3.5-5.6";
    nikonLensHash["5048567C303C5406"] = "IX-Nikkor 60-180mm f/4-5.6";
    nikonLensHash["5254444418180000"] = "Zeiss Milvus 35mm f/2";
    nikonLensHash["5348608024245702"] = "AF Zoom-Nikkor 80-200mm f/2.8D ED";
    nikonLensHash["5348608024246002"] = "AF Zoom-Nikkor 80-200mm f/2.8D ED";
    nikonLensHash["535450500C0C0000"] = "Zeiss Milvus 50mm f/1.4";
    nikonLensHash["54445C7C343C5802"] = "AF Zoom-Micro Nikkor 70-180mm f/4.5-5.6D ED";
    nikonLensHash["54445C7C343C6102"] = "AF Zoom-Micro Nikkor 70-180mm f/4.5-5.6D ED";
    nikonLensHash["5454505018180000"] = "Zeiss Milvus 50mm f/2 Macro";
    nikonLensHash["555462620C0C0000"] = "Zeiss Milvus 85mm f/1.4";
    nikonLensHash["563C5C8E303C1C02"] = "Sigma 70-300mm F4-5.6 APO Macro Super II";
    nikonLensHash["56485C8E303C5A02"] = "AF Zoom-Nikkor 70-300mm f/4-5.6D ED";
    nikonLensHash["5654686818180000"] = "Zeiss Milvus 100mm f/2 Macro";
    nikonLensHash["5948989824245D02"] = "400mm f/2.8D IF-ED";
    nikonLensHash["594898982424E102"] = "400mm f/2.8D IF-ED + TC-17E";
    nikonLensHash["594898982424F102"] = "400mm f/2.8D IF-ED + TC-14E";
    nikonLensHash["594898982424F202"] = "400mm f/2.8D IF-ED + TC-20E";
    nikonLensHash["5A3C3E56303C5E06"] = "IX-Nikkor 30-60mm f/4-5.6";
    nikonLensHash["5B44567C343C5F06"] = "IX-Nikkor 60-180mm f/4.5-5.6";
    nikonLensHash["5D483C5C24246302"] = "AF-S Zoom-Nikkor 28-70mm f/2.8D IF-ED";
    nikonLensHash["5E48608024246402"] = "AF-S Zoom-Nikkor 80-200mm f/2.8D IF-ED";
    nikonLensHash["5F403C6A2C346502"] = "AF Zoom-Nikkor 28-105mm f/3.5-4.5D IF";
    nikonLensHash["60403C602C3C6602"] = "AF Zoom-Nikkor 28-80mm f/3.5-5.6D";
    nikonLensHash["61445E86343C6702"] = "AF Zoom-Nikkor 75-240mm f/4.5-5.6D";
    nikonLensHash["63482B4424246802"] = "17-35mm f/2.8D IF-ED";
    nikonLensHash["6400626224246A02"] = "PC Micro-Nikkor 85mm f/2.8D";
    nikonLensHash["65446098343C6B0A"] = "AF VR Zoom-Nikkor 80-400mm f/4.5-5.6D ED";
    nikonLensHash["66402D442C346C02"] = "AF Zoom-Nikkor 18-35mm f/3.5-4.5D IF-ED";
    nikonLensHash["6748376224306D02"] = "AF Zoom-Nikkor 24-85mm f/2.8-4D IF";
    nikonLensHash["6754375C24241C02"] = "Sigma 24-70mm F2.8 EX DG Macro";
    nikonLensHash["68423C602A3C6E06"] = "AF Zoom-Nikkor 28-80mm f/3.3-5.6G";
    nikonLensHash["69475C8E303C0002"] = "Tamron AF 70-300mm f/4-5.6 Di LD Macro 1:2";
    nikonLensHash["69485C8E303C6F02"] = "Tamron AF 70-300mm f/4-5.6 LD Macro 1:2";
    nikonLensHash["69485C8E303C6F06"] = "AF Zoom-Nikkor 70-300mm f/4-5.6G";
    nikonLensHash["6A488E8E30307002"] = "300mm f/4D IF-ED";
    nikonLensHash["6B48242424247102"] = "AF Nikkor ED 14mm f/2.8D";
    nikonLensHash["6D488E8E24247302"] = "300mm f/2.8D IF-ED II";
    nikonLensHash["6E48989824247402"] = "400mm f/2.8D IF-ED II";
    nikonLensHash["6F3CA0A030307502"] = "500mm f/4D IF-ED II";
    nikonLensHash["703CA6A630307602"] = "600mm f/4D IF-ED II VR";
    nikonLensHash["72484C4C24247700"] = "Nikkor 45mm f/2.8 P";
    nikonLensHash["744037622C347806"] = "AF-S Zoom-Nikkor 24-85mm f/3.5-4.5G IF-ED";
    nikonLensHash["75403C682C3C7906"] = "AF Zoom-Nikkor 28-100mm f/3.5-5.6G";
    nikonLensHash["7658505014147A02"] = "AF Nikkor 50mm f/1.8D";
    nikonLensHash["77446098343C7B0E"] = "Sigma 80-400mm f4.5-5.6 APO DG D OS";
    nikonLensHash["77446198343C7B0E"] = "Sigma 80-400mm F4.5-5.6 EX OS";
    nikonLensHash["77485C8024247B0E"] = "AF-S VR Zoom-Nikkor 70-200mm f/2.8G IF-ED";
    nikonLensHash["7840376E2C3C7C0E"] = "AF-S VR Zoom-Nikkor 24-120mm f/3.5-5.6G IF-ED";
    nikonLensHash["794011112C2C1C06"] = "Sigma 8mm F3.5 EX Circular Fisheye";
    nikonLensHash["79403C802C3C7F06"] = "AF Zoom-Nikkor 28-200mm f/3.5-5.6G IF-ED";
    nikonLensHash["79483C5C24241C06"] = "Sigma 28-70mm F2.8 EX DG";
    nikonLensHash["79485C5C24241C06"] = "Sigma Macro 70mm F2.8 EX DG";
    nikonLensHash["795431310C0C4B06"] = "Sigma 20mm F1.4 DG HSM | A";
    nikonLensHash["7A3B5380303C4B06"] = "Sigma 55-200mm F4-5.6 DC HSM";
    nikonLensHash["7A3C1F3730307E06"] = "AF-S DX Zoom-Nikkor 12-24mm f/4G IF-ED";
    nikonLensHash["7A3C1F3C30307E06"] = "Tokina AT-X 12-28 PRO DX (AF 12-28mm f/4)";
    nikonLensHash["7A402D502C3C4B06"] = "Sigma 18-50mm F3.5-5.6 DC HSM";
    nikonLensHash["7A402D802C404B0E"] = "Sigma 18-200mm F3.5-6.3 DC OS HSM";
    nikonLensHash["7A472B5C24344B06"] = "Sigma 17-70mm F2.8-4.5 DC Macro Asp. IF HSM";
    nikonLensHash["7A47507624244B06"] = "Sigma 50-150mm F2.8 EX APO DC HSM";
    nikonLensHash["7A481C2924247E06"] = "Tokina AT-X 116 PRO DX II (AF 11-16mm f/2.8)";
    nikonLensHash["7A481C3024247E06"] = "Tokina AT-X 11-20 F2.8 PRO DX (AF 11-20mm f/2.8)";
    nikonLensHash["7A482B5C24344B06"] = "Sigma 17-70mm F2.8-4.5 DC Macro Asp. IF HSM";
    nikonLensHash["7A482D5024244B06"] = "Sigma 18-50mm F2.8 EX DC Macro";
    nikonLensHash["7A485C8024244B06"] = "Sigma 70-200mm F2.8 EX APO DG Macro HSM II";
    nikonLensHash["7A546E8E24244B02"] = "Sigma APO 120-300mm F2.8 EX DG HSM";
    nikonLensHash["7B4880983030800E"] = "AF-S VR Zoom-Nikkor 200-400mm f/4G IF-ED";
    nikonLensHash["7D482B5324248206"] = "AF-S DX Zoom-Nikkor 17-55mm f/2.8G IF-ED";
    nikonLensHash["7F402D5C2C348406"] = "AF-S DX Zoom-Nikkor 18-70mm f/3.5-4.5G IF-ED";
    nikonLensHash["7F482B5C24341C06"] = "Sigma 17-70mm F2.8-4.5 DC Macro Asp. IF";
    nikonLensHash["7F482D5024241C06"] = "Sigma 18-50mm F2.8 EX DC Macro";
    nikonLensHash["80481A1A24248506"] = "AF DX Fisheye-Nikkor 10.5mm f/2.8G ED";
    nikonLensHash["813476A638404B0E"] = "Sigma 150-600mm F5-6.3 DG OS HSM | S";
    nikonLensHash["815480801818860E"] = "AF-S Nikkor 200mm f/2G IF-ED VR";
    nikonLensHash["823476A638404B0E"] = "Sigma 150-600mm F5-6.3 DG OS HSM | C";
    nikonLensHash["82488E8E2424870E"] = "AF-S VR Nikkor 300mm f/2.8G IF-ED";
    nikonLensHash["8300B0B05A5A8804"] = "FSA-L2, EDG 65, 800mm F13 G";
    nikonLensHash["885450500C0C4B06"] = "Sigma 50mm F1.4 DG HSM | A";
    nikonLensHash["893C5380303C8B06"] = "AF-S DX Zoom-Nikkor 55-200mm f/4-5.6G ED";
    nikonLensHash["8A3C376A30304B0E"] = "Sigma 24-105mm F4 DG OS HSM";
    nikonLensHash["8A546A6A24248C0E"] = "AF-S VR Micro-Nikkor 105mm f/2.8G IF-ED";
    nikonLensHash["8B402D802C3C8D0E"] = "AF-S DX VR Zoom-Nikkor 18-200mm f/3.5-5.6G IF-ED";
    nikonLensHash["8B402D802C3CFD0E"] = "AF-S DX VR Zoom-Nikkor 18-200mm f/3.5-5.6G IF-ED [II]";
    nikonLensHash["8B4C2D4414144B06"] = "Sigma 18-35mm F1.8 DC HSM";
    nikonLensHash["8C402D532C3C8E06"] = "AF-S DX Zoom-Nikkor 18-55mm f/3.5-5.6G ED";
    nikonLensHash["8D445C8E343C8F0E"] = "AF-S VR Zoom-Nikkor 70-300mm f/4.5-5.6G IF-ED";
    nikonLensHash["8E3C2B5C24304B0E"] = "Sigma 17-70mm F2.8-4 DC Macro OS HSM | C";
    nikonLensHash["8F402D722C3C9106"] = "AF-S DX Zoom-Nikkor 18-135mm f/3.5-5.6G IF-ED";
    nikonLensHash["8F482B5024244B0E"] = "Sigma 17-50mm F2.8 EX DC OS HSM";
    nikonLensHash["903B5380303C920E"] = "AF-S DX VR Zoom-Nikkor 55-200mm f/4-5.6G IF-ED";
    nikonLensHash["90402D802C404B0E"] = "Sigma 18-200mm F3.5-6.3 II DC OS HSM";
    nikonLensHash["915444440C0C4B06"] = "Sigma 35mm f/1.4 DG HSM Art";
    nikonLensHash["922C2D882C404B0E"] = "Sigma 18-250mm F3.5-6.3 DC Macro OS HSM";
    nikonLensHash["9248243724249406"] = "AF-S Zoom-Nikkor 14-24mm f/2.8G ED";
    nikonLensHash["9348375C24249506"] = "AF-S Zoom-Nikkor 24-70mm f/2.8G ED";
    nikonLensHash["94402D532C3C9606"] = "AF-S DX Zoom-Nikkor 18-55mm f/3.5-5.6G ED II";
    nikonLensHash["94487C7C24244B0E"] = "Sigma 180mm F2.8 APO Macro EX DG OS";
    nikonLensHash["950037372C2C9706"] = "PC-E Nikkor 24mm f/3.5D ED";
    nikonLensHash["954C37372C2C9702"] = "PC-E Nikkor 24mm f/3.5D ED";
    nikonLensHash["96381F37343C4B06"] = "Sigma 12-24mm F4.5-5.6 II DG HSM";
    nikonLensHash["964898982424980E"] = "AF-S VR Nikkor 400mm f/2.8G ED";
    nikonLensHash["973CA0A03030990E"] = "AF-S VR Nikkor 500mm f/4G ED";
    nikonLensHash["97486A6A24244B0E"] = "Sigma Macro 105mm F2.8 EX DG OS HSM";
    nikonLensHash["983CA6A630309A0E"] = "AF-S VR Nikkor 600mm f/4G ED";
    nikonLensHash["9848507624244B0E"] = "Sigma 50-150mm F2.8 EX APO DC OS HSM";
    nikonLensHash["994029622C3C9B0E"] = "AF-S DX VR Zoom-Nikkor 16-85mm f/3.5-5.6G ED";
    nikonLensHash["9948767624244B0E"] = "Sigma APO Macro 150mm F2.8 EX DG OS HSM";
    nikonLensHash["9A402D532C3C9C0E"] = "AF-S DX VR Zoom-Nikkor 18-55mm f/3.5-5.6G";
    nikonLensHash["9B004C4C24249D06"] = "PC-E Micro Nikkor 45mm f/2.8D ED";
    nikonLensHash["9B544C4C24249D02"] = "PC-E Micro Nikkor 45mm f/2.8D ED";
    nikonLensHash["9B5462620C0C4B06"] = "Sigma 85mm F1.4 EX DG HSM";
    nikonLensHash["9C485C8024244B0E"] = "Sigma 70-200mm F2.8 EX DG OS HSM";
    nikonLensHash["9C54565624249E06"] = "AF-S Micro Nikkor 60mm f/2.8G ED";
    nikonLensHash["9D00626224249F06"] = "PC-E Micro Nikkor 85mm f/2.8D";
    nikonLensHash["9D482B5024244B0E"] = "Sigma 17-50mm F2.8 EX DC OS HSM";
    nikonLensHash["9D54626224249F02"] = "PC-E Micro Nikkor 85mm f/2.8D";
    nikonLensHash["9E381129343C4B06"] = "Sigma 8-16mm F4.5-5.6 DC HSM";
    nikonLensHash["9E402D6A2C3CA00E"] = "AF-S DX VR Zoom-Nikkor 18-105mm f/3.5-5.6G ED";
    nikonLensHash["9F3750A034404B0E"] = "Sigma 50-500mm F4.5-6.3 DG OS HSM";
    nikonLensHash["9F5844441414A106"] = "AF-S DX Nikkor 35mm f/1.8G";
    nikonLensHash["A0402D532C3CCA0E"] = "AF-P DX Nikkor 18-55mm f/3.5-5.6G VR";
    nikonLensHash["A0402D532C3CCA8E"] = "AF-P DX Nikkor 18-55mm f/3.5-5.6G";
    nikonLensHash["A0402D742C3CBB0E"] = "AF-S DX Nikkor 18-140mm f/3.5-5.6G ED VR";
    nikonLensHash["A0482A5C24304B0E"] = "Sigma 17-70mm F2.8-4 DC Macro OS HSM";
    nikonLensHash["A05450500C0CA206"] = "50mm f/1.4G";
    nikonLensHash["A14018372C34A306"] = "AF-S DX Nikkor 10-24mm f/3.5-4.5G ED";
    nikonLensHash["A14119312C2C4B06"] = "Sigma 10-20mm F3.5 EX DC HSM";
    nikonLensHash["A15455550C0CBC06"] = "58mm f/1.4G";
    nikonLensHash["A2402D532C3CBD0E"] = "AF-S DX Nikkor 18-55mm f/3.5-5.6G VR II";
    nikonLensHash["A2485C802424A40E"] = "70-200mm f/2.8G ED VR II";
    nikonLensHash["A33C29443030A50E"] = "16-35mm f/4G ED VR";
    nikonLensHash["A33C5C8E303C4B0E"] = "Sigma 70-300mm F4-5.6 DG OS";
    nikonLensHash["A4402D8E2C40BF0E"] = "AF-S DX Nikkor 18-300mm f/3.5-6.3G ED VR";
    nikonLensHash["A4472D5024344B0E"] = "Sigma 18-50mm F2.8-4.5 DC OS HSM";
    nikonLensHash["A4485C802424CF0E"] = "70-200mm f/2.8E FL ED VR";
    nikonLensHash["A45437370C0CA606"] = "24mm f/1.4G ED";
    nikonLensHash["A5402D882C404B0E"] = "Sigma 18-250mm F3.5-6.3 DC OS HSM";
    nikonLensHash["A5403C8E2C3CA70E"] = "28-300mm f/3.5-5.6G ED VR";
    nikonLensHash["A54C44441414C006"] = "35mm f/1.8G ED";
    nikonLensHash["A5546A6A0C0CD006"] = "105mm f/1.4E ED";
    nikonLensHash["A5546A6A0C0CD046"] = "105mm f/1.4E ED";
    nikonLensHash["A648375C24244B06"] = "Sigma 24-70mm F2.8 IF EX DG HSM";
    nikonLensHash["A6488E8E2424A80E"] = "AF-S VR Nikkor 300mm f/2.8G IF-ED II";
    nikonLensHash["A64898982424C10E"] = "400mm f/2.8E FL ED VR";
    nikonLensHash["A73C5380303CC20E"] = "AF-S DX Nikkor 55-200mm f/4-5.6G ED VR II";
    nikonLensHash["A74980A024244B06"] = "Sigma APO 200-500mm F2.8 EX DG";
    nikonLensHash["A74B62622C2CA90E"] = "AF-S DX Micro Nikkor 85mm f/3.5G ED VR";
    nikonLensHash["A8381830343CD38E"] = "AF-P DX Nikkor 10-20mm f/4.5-5.6G VR";
    nikonLensHash["A84880983030AA0E"] = "AF-S VR Zoom-Nikkor 200-400mm f/4G IF-ED II";
    nikonLensHash["A8488E8E3030C30E"] = "300mm f/4E PF ED VR";
    nikonLensHash["A8488E8E3030C34E"] = "300mm f/4E PF ED VR";
    nikonLensHash["A94C31311414C406"] = "20mm f/1.8G ED";
    nikonLensHash["A95480801818AB0E"] = "200mm f/2G ED VR II";
    nikonLensHash["AA3C376E3030AC0E"] = "24-120mm f/4G ED VR";
    nikonLensHash["AA48375C2424C54E"] = "24-70mm f/2.8E ED VR";
    nikonLensHash["AB3CA0A03030C64E"] = "500mm f/4E FL ED VR";
    nikonLensHash["AC38538E343CAE0E"] = "AF-S DX VR Nikkor 55-300mm f/4.5-5.6G ED";
    nikonLensHash["AC3CA6A63030C74E"] = "600mm f/4E FL ED VR";
    nikonLensHash["AD3C2D8E2C3CAF0E"] = "AF-S DX Nikkor 18-300mm f/3.5-5.6G ED VR";
    nikonLensHash["AD4828602430C80E"] = "AF-S DX Nikkor 16-80mm f/2.8-4E ED VR";
    nikonLensHash["AD4828602430C84E"] = "AF-S DX Nikkor 16-80mm f/2.8-4E ED VR";
    nikonLensHash["AE3C80A03C3CC90E"] = "200-500mm f/5.6E ED VR";
    nikonLensHash["AE3C80A03C3CC94E"] = "200-500mm f/5.6E ED VR";
    nikonLensHash["AE5462620C0CB006"] = "85mm f/1.4G";
    nikonLensHash["AF4C37371414CC06"] = "24mm f/1.8G ED";
    nikonLensHash["AF5444440C0CB106"] = "35mm f/1.4G";
    nikonLensHash["B04C50501414B206"] = "50mm f/1.8G";
    nikonLensHash["B14848482424B306"] = "AF-S DX Micro Nikkor 40mm f/2.8G";
    nikonLensHash["B2485C803030B40E"] = "70-200mm f/4G ED VR";
    nikonLensHash["B34C62621414B506"] = "85mm f/1.8G";
    nikonLensHash["B44037622C34B60E"] = "AF-S VR Zoom-Nikkor 24-85mm f/3.5-4.5G IF-ED";
    nikonLensHash["B54C3C3C1414B706"] = "28mm f/1.8G";
    nikonLensHash["B63CB0B03C3CB80E"] = "AF-S VR Nikkor 800mm f/5.6E FL ED";
    nikonLensHash["B63CB0B03C3CB84E"] = "AF-S VR Nikkor 800mm f/5.6E FL ED";
    nikonLensHash["B648375624241C02"] = "Sigma 24-60mm F2.8 EX DG";
    nikonLensHash["B7446098343CB90E"] = "80-400mm f/4.5-5.6G ED VR";
    nikonLensHash["B8402D442C34BA06"] = "18-35mm f/3.5-4.5G ED";
    nikonLensHash["BF3C1B1B30300104"] = "Irix 11mm f/4 Firefly";
    nikonLensHash["BF4E26261E1E0104"] = "Irix 15mm f/2.4 Firefly";
    nikonLensHash["C334689838404B4E"] = "Sigma 100-400mm F5-6.3 DG OS HSM | C";
    nikonLensHash["CC4C506814144B06"] = "Sigma 50-100mm F1.8 DC HSM | A";
    nikonLensHash["CD3D2D702E3C4B0E"] = "Sigma 18-125mm F3.8-5.6 DC OS HSM";
    nikonLensHash["CE3476A038404B0E"] = "Sigma 150-500mm F5-6.3 DG OS APO HSM";
    nikonLensHash["CF386E98343C4B0E"] = "Sigma APO 120-400mm F4.5-5.6 DG OS HSM";
    nikonLensHash["DC48191924244B06"] = "Sigma 10mm F2.8 EX DC HSM Fisheye";
    nikonLensHash["DE5450500C0C4B06"] = "Sigma 50mm F1.4 EX DG HSM";
    nikonLensHash["E03C5C8E303C4B06"] = "Sigma 70-300mm F4-5.6 APO DG Macro HSM";
    nikonLensHash["E158373714141C02"] = "Sigma 24mm F1.8 EX DG Aspherical Macro";
    nikonLensHash["E34076A63840DF4E"] = "Tamron SP 150-600mm f/5-6.3 Di VC USD G2";
    nikonLensHash["E354505024243502"] = "Sigma Macro 50mm F2.8 EX DG";
    nikonLensHash["E45464642424DF0E"] = "Tamron SP 90mm f/2.8 Di VC USD Macro 1:1 (F017)";
    nikonLensHash["E5546A6A24243502"] = "Sigma Macro 105mm F2.8 EX DG";
    nikonLensHash["E6402D802C40DF0E"] = "Tamron AF 18-200mm f/3.5-6.3 Di II VC (B018)";
    nikonLensHash["E6413C8E2C401C02"] = "Sigma 28-300mm F3.5-6.3 DG Macro";
    nikonLensHash["E74C4C4C1414DF0E"] = "Tamron SP 45mm f/1.8 Di VC USD (F013)";
    nikonLensHash["E84C44441414DF0E"] = "Tamron SP 35mm f/1.8 Di VC USD (F012)";
    nikonLensHash["E948273E2424DF0E"] = "Tamron SP 15-30mm f/2.8 Di VC USD (A012)";
    nikonLensHash["E954375C24241C02"] = "Sigma 24-70mm F2.8 EX DG Macro";
    nikonLensHash["EA40298E2C40DF0E"] = "Tamron AF 16-300mm f/3.5-6.3 Di II VC PZD (B016)";
    nikonLensHash["EA48272724241C02"] = "Sigma 15mm F2.8 EX Diagonal Fisheye";
    nikonLensHash["EB4076A63840DF0E"] = "Tamron SP AF 150-600mm f/5-6.3 VC USD (A011)";
    nikonLensHash["ED402D802C404B0E"] = "Sigma 18-200mm F3.5-6.3 DC OS HSM";
    nikonLensHash["EE485C8024244B06"] = "Sigma 70-200mm F2.8 EX APO DG Macro HSM II";
    nikonLensHash["F0381F37343C4B06"] = "Sigma 12-24mm F4.5-5.6 EX DG Aspherical HSM";
    nikonLensHash["F03F2D8A2C40DF0E"] = "Tamron AF 18-270mm f/3.5-6.3 Di II VC PZD (B008)";
    nikonLensHash["F144A0A034344B02"] = "Sigma APO 500mm F4.5 EX DG HSM";
    nikonLensHash["F1475C8E303CDF0E"] = "Tamron SP 70-300mm f/4-5.6 Di VC USD (A005)";
    nikonLensHash["F348688E30304B02"] = "Sigma APO 100-300mm F4 EX IF HSM";
    nikonLensHash["F3542B502424840E"] = "Tamron SP AF 17-50mm f/2.8 XR Di II VC LD Aspherical (IF) (B005)";
    nikonLensHash["F454565618188406"] = "Tamron SP AF 60mm f/2.0 Di II Macro 1:1 (G005)";
    nikonLensHash["F5402C8A2C40400E"] = "Tamron AF 18-270mm f/3.5-6.3 Di II VC LD Aspherical (IF) Macro (B003)";
    nikonLensHash["F548767624244B06"] = "Sigma APO Macro 150mm F2.8 EX DG HSM";
    nikonLensHash["F63F18372C348406"] = "Tamron SP AF 10-24mm f/3.5-4.5 Di II LD Aspherical (IF) (B001)";
    nikonLensHash["F63F18372C34DF06"] = "Tamron SP AF 10-24mm f/3.5-4.5 Di II LD Aspherical (IF) (B001)";
    nikonLensHash["F6482D5024244B06"] = "Sigma 18-50mm F2.8 EX DC Macro";
    nikonLensHash["F7535C8024244006"] = "Tamron SP AF 70-200mm f/2.8 Di LD (IF) Macro (A001)";
    nikonLensHash["F7535C8024248406"] = "Tamron SP AF 70-200mm f/2.8 Di LD (IF) Macro (A001)";
    nikonLensHash["F8543E3E0C0C4B06"] = "Sigma 30mm F1.4 EX DC HSM";
    nikonLensHash["F85464642424DF06"] = "Tamron SP AF 90mm f/2.8 Di Macro 1:1 (272NII)";
    nikonLensHash["F855646424248406"] = "Tamron SP AF 90mm f/2.8 Di Macro 1:1 (272NII)";
    nikonLensHash["F93C1931303C4B06"] = "Sigma 10-20mm F4-5.6 EX DC HSM";
    nikonLensHash["F9403C8E2C40400E"] = "Tamron AF 28-300mm f/3.5-6.3 XR Di VC LD Aspherical (IF) Macro (A20)";
    nikonLensHash["FA543C5E24248406"] = "Tamron SP AF 28-75mm f/2.8 XR Di LD Aspherical (IF) Macro (A09NII)";
    nikonLensHash["FA543C5E2424DF06"] = "Tamron SP AF 28-75mm f/2.8 XR Di LD Aspherical (IF) Macro (A09NII)";
    nikonLensHash["FA546E8E24244B02"] = "Sigma APO 120-300mm F2.8 EX DG HSM";
    nikonLensHash["FB542B5024248406"] = "Tamron SP AF 17-50mm f/2.8 XR Di II LD Aspherical (IF) (A16NII)";
    nikonLensHash["FB548E8E24244B02"] = "Sigma APO 300mm F2.8 EX DG HSM";
    nikonLensHash["FC402D802C40DF06"] = "Tamron AF 18-200mm f/3.5-6.3 XR Di II LD Aspherical (IF) Macro (A14NII)";
    nikonLensHash["FD47507624244B06"] = "Sigma 50-150mm F2.8 EX APO DC HSM II";
    nikonLensHash["FE47000024244B06"] = "Sigma 4.5mm F2.8 EX DC HSM Circular Fisheye";
    nikonLensHash["FE48375C2424DF0E"] = "Tamron SP 24-70mm f/2.8 Di VC USD (A007)";
    nikonLensHash["FE535C8024248406"] = "Tamron SP AF 70-200mm f/2.8 Di LD (IF) Macro (A001)";
    nikonLensHash["FE545C802424DF0E"] = "Tamron SP 70-200mm f/2.8 Di VC USD (A009)";
    nikonLensHash["FE5464642424DF0E"] = "Tamron SP 90mm f/2.8 Di VC USD Macro 1:1 (F004)";
    nikonLensHash["FF402D802C404B06"] = "Sigma 18-200mm F3.5-6.3 DC";

}

QByteArray Metadata::nikonDecrypt(QByteArray bData, uint32_t count, uint32_t serial)
{
    quint8 xlat[2][256] = {
       { 0xc1,0xbf,0x6d,0x0d,0x59,0xc5,0x13,0x9d,0x83,0x61,0x6b,0x4f,0xc7,0x7f,0x3d,0x3d,
         0x53,0x59,0xe3,0xc7,0xe9,0x2f,0x95,0xa7,0x95,0x1f,0xdf,0x7f,0x2b,0x29,0xc7,0x0d,
         0xdf,0x07,0xef,0x71,0x89,0x3d,0x13,0x3d,0x3b,0x13,0xfb,0x0d,0x89,0xc1,0x65,0x1f,
         0xb3,0x0d,0x6b,0x29,0xe3,0xfb,0xef,0xa3,0x6b,0x47,0x7f,0x95,0x35,0xa7,0x47,0x4f,
         0xc7,0xf1,0x59,0x95,0x35,0x11,0x29,0x61,0xf1,0x3d,0xb3,0x2b,0x0d,0x43,0x89,0xc1,
         0x9d,0x9d,0x89,0x65,0xf1,0xe9,0xdf,0xbf,0x3d,0x7f,0x53,0x97,0xe5,0xe9,0x95,0x17,
         0x1d,0x3d,0x8b,0xfb,0xc7,0xe3,0x67,0xa7,0x07,0xf1,0x71,0xa7,0x53,0xb5,0x29,0x89,
         0xe5,0x2b,0xa7,0x17,0x29,0xe9,0x4f,0xc5,0x65,0x6d,0x6b,0xef,0x0d,0x89,0x49,0x2f,
         0xb3,0x43,0x53,0x65,0x1d,0x49,0xa3,0x13,0x89,0x59,0xef,0x6b,0xef,0x65,0x1d,0x0b,
         0x59,0x13,0xe3,0x4f,0x9d,0xb3,0x29,0x43,0x2b,0x07,0x1d,0x95,0x59,0x59,0x47,0xfb,
         0xe5,0xe9,0x61,0x47,0x2f,0x35,0x7f,0x17,0x7f,0xef,0x7f,0x95,0x95,0x71,0xd3,0xa3,
         0x0b,0x71,0xa3,0xad,0x0b,0x3b,0xb5,0xfb,0xa3,0xbf,0x4f,0x83,0x1d,0xad,0xe9,0x2f,
         0x71,0x65,0xa3,0xe5,0x07,0x35,0x3d,0x0d,0xb5,0xe9,0xe5,0x47,0x3b,0x9d,0xef,0x35,
         0xa3,0xbf,0xb3,0xdf,0x53,0xd3,0x97,0x53,0x49,0x71,0x07,0x35,0x61,0x71,0x2f,0x43,
         0x2f,0x11,0xdf,0x17,0x97,0xfb,0x95,0x3b,0x7f,0x6b,0xd3,0x25,0xbf,0xad,0xc7,0xc5,
         0xc5,0xb5,0x8b,0xef,0x2f,0xd3,0x07,0x6b,0x25,0x49,0x95,0x25,0x49,0x6d,0x71,0xc7 },
       { 0xa7,0xbc,0xc9,0xad,0x91,0xdf,0x85,0xe5,0xd4,0x78,0xd5,0x17,0x46,0x7c,0x29,0x4c,
         0x4d,0x03,0xe9,0x25,0x68,0x11,0x86,0xb3,0xbd,0xf7,0x6f,0x61,0x22,0xa2,0x26,0x34,
         0x2a,0xbe,0x1e,0x46,0x14,0x68,0x9d,0x44,0x18,0xc2,0x40,0xf4,0x7e,0x5f,0x1b,0xad,
         0x0b,0x94,0xb6,0x67,0xb4,0x0b,0xe1,0xea,0x95,0x9c,0x66,0xdc,0xe7,0x5d,0x6c,0x05,
         0xda,0xd5,0xdf,0x7a,0xef,0xf6,0xdb,0x1f,0x82,0x4c,0xc0,0x68,0x47,0xa1,0xbd,0xee,
         0x39,0x50,0x56,0x4a,0xdd,0xdf,0xa5,0xf8,0xc6,0xda,0xca,0x90,0xca,0x01,0x42,0x9d,
         0x8b,0x0c,0x73,0x43,0x75,0x05,0x94,0xde,0x24,0xb3,0x80,0x34,0xe5,0x2c,0xdc,0x9b,
         0x3f,0xca,0x33,0x45,0xd0,0xdb,0x5f,0xf5,0x52,0xc3,0x21,0xda,0xe2,0x22,0x72,0x6b,
         0x3e,0xd0,0x5b,0xa8,0x87,0x8c,0x06,0x5d,0x0f,0xdd,0x09,0x19,0x93,0xd0,0xb9,0xfc,
         0x8b,0x0f,0x84,0x60,0x33,0x1c,0x9b,0x45,0xf1,0xf0,0xa3,0x94,0x3a,0x12,0x77,0x33,
         0x4d,0x44,0x78,0x28,0x3c,0x9e,0xfd,0x65,0x57,0x16,0x94,0x6b,0xfb,0x59,0xd0,0xc8,
         0x22,0x36,0xdb,0xd2,0x63,0x98,0x43,0xa1,0x04,0x87,0x86,0xf7,0xa6,0x26,0xbb,0xd6,
         0x59,0x4d,0xbf,0x6a,0x2e,0xaa,0x2b,0xef,0xe6,0x78,0xb6,0x4e,0xe0,0x2f,0xdc,0x7c,
         0xbe,0x57,0x19,0x32,0x7e,0x2a,0xd0,0xb8,0xba,0x29,0x00,0x3c,0x52,0x7d,0xa8,0x49,
         0x3b,0x2d,0xeb,0x25,0x49,0xfa,0xa3,0xaa,0x39,0xa7,0xc5,0xa7,0x50,0x11,0x36,0xfb,
         0xc6,0x67,0x4a,0xf5,0xa5,0x12,0x65,0x7e,0xb0,0xdf,0xaf,0x4e,0xb3,0x61,0x7f,0x2f }
   };

   quint8 key = 0;
   for (int i = 0; i < 4; ++i) {
       key ^= (count >> (i*8)) & 0xff;
   }
   quint8 ci = xlat[0][serial & 0xff];
   quint8 cj = xlat[1][key];
   quint8 ck = 0x60;
   // 1st 4 bytes not encrypted
   for (int i = 4; i < bData.size(); ++i) {
       cj += ci * ck++;
       quint8 x = bData[i];
       x ^= cj;
       bData[i] = x;
   }
   return bData;
}

void Metadata::reportMetadata()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString createdExif = createdDate.toString("yyyy-MM-dd hh:mm:ss");
    rpt << "\n";
    rpt.reset();
    rpt.setFieldAlignment(QTextStream::AlignLeft);

    rpt.setFieldWidth(25); rpt << "offsetFullJPG"       << offsetFullJPG;       rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "lengthFullJPG"       << lengthFullJPG;       rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "offsetThumbJPG"      << offsetThumbJPG;      rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "lengthThumbJPG"      << lengthThumbJPG;      rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "offsetSmallJPG"      << offsetSmallJPG;      rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "lengthSmallJPG"      << lengthSmallJPG;      rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "offsetXMPSeg"        << xmpSegmentOffset;    rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "offsetNextXMPSegment"<< xmpNextSegmentOffset;rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "orientation"         << orientation;         rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "width"               << width;               rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "height"              << height;              rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "created"             << createdExif;         rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "make"                << make;                rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "model"               << model;               rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "exposureTime"        << exposureTime;        rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "aperture"            << aperture;            rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "ISO"                 << ISO;                 rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "focalLength"         << focalLength;         rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "title"               << title;               rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "lens"                << lens;                rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "creator"             << creator;             rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "copyright"           << copyright;           rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "email"               << email;               rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "url"                 << url;                 rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "rating"              << rating;              rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "label"               << label;               rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "cameraSN"            << cameraSN;            rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "lensSN"              << lensSN;              rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "shutterCount"        << shutterCount;        rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "nikonLensCode"       << nikonLensCode;       rpt.setFieldWidth(0); rpt << "\n";

    if (isXmp && xmpString.length() > 0) {
        rpt << "\nXMP Extract:\n\n";
//        if (!okToReadXmp) xmpBa = extractXmp(xmpSegmentOffset);
//        QString xmpString = QTextCodec::codecForMib(106)->toUnicode(xmpBa);
//        Xmp xmp(file, xmpSegmentOffset, xmpNextSegmentOffset);
//        QString xmpString = xmp.metaAsString();
        QXmlQuery query;
        query.setQuery(xmpString);

        // Set up the output device
        QByteArray outArray;
        QBuffer buffer(&outArray);
        buffer.open(QIODevice::ReadWrite);

        // format xmp
        QXmlFormatter formatter(query, &buffer);
        query.evaluateTo(&formatter);

        QString xmpStr = QTextCodec::codecForMib(106)->toUnicode(outArray);
        rpt << xmpStr;
    }

    QDialog *dlg = new QDialog;
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    md.textBrowser->setText(reportString);
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
    dlg->show();
    std::cout << reportString.toStdString() << std::flush;
}

void Metadata::reportMetadataHeader(QString title)
{
    int hdrWidth = 90;
    int titleWidth = title.size() + 4;
    int padWidth = (hdrWidth - titleWidth) / 2;
    rpt.reset();
    rpt << "\n";
    rpt.setPadChar('-');
    rpt.setFieldWidth(padWidth);
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt << "-";
    rpt.reset();
    rpt.setFieldWidth(titleWidth);
    rpt.setFieldAlignment(QTextStream::AlignCenter);
    rpt << title;
    rpt.setPadChar('-');
    rpt.setFieldWidth(padWidth);
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt << "-";
    rpt.reset();
    rpt << "\n";
}

void Metadata::reportMetadataAllFiles()
{
/*
Not used
*/
    QMapIterator<QString, ImageMetadata> i(metaCache);
    while (i.hasNext())  {
        i.next();
        offsetFullJPG = i.value().offsetFullJPG;
        lengthFullJPG = i.value().lengthFullJPG;
        offsetThumbJPG = i.value().offsetThumbJPG;
        lengthThumbJPG = i.value().lengthThumbJPG;
        offsetSmallJPG = i.value().offsetSmallJPG;
        lengthSmallJPG = i.value().lengthSmallJPG;
        width = i.value().width;
        height = i.value().height;
//        created = i.value().created;
        createdDate = i.value().createdDate;
        model = i.value().model;
        exposureTime = i.value().exposureTime;
        exposureTimeNum = i.value().exposureTimeNum;
        aperture = i.value().aperture;
        apertureNum = i.value().apertureNum;
        ISO = i.value().ISO;
        ISONum = i.value().ISONum;
        focalLength = i.value().focalLength;
        focalLengthNum = i.value().focalLengthNum;
        title = i.value().title;
        lens = i.value().lens;
        creator = i.value().creator;
        copyright = i.value().copyright;
        email = i.value().email;
        url = i.value().url;

        rpt << "\n";
        std::cout << "FILE: " << i.key().toStdString()<< "\n";
        reportMetadata();
    }
}

void Metadata::reportMetadataCache(const QString &imageFileName)
{
/*
Reports the ImageMetadata for imageFileName (the image absolute path), which is
used as the key for the QMap of ImageMetadata.  The metadata items are updated
in setMetadata to make them more convenient to use.  For example, returning the
rating value from metaCache

    metaCache[imageFileName].rating;

is reduced to

    rating

in setMetadata.


*/
    setMetadata(imageFileName);
    rpt.flush();
    reportString = "";
    rpt.setString(&reportString);
    rpt << "\nFile name = " << imageFileName << "\n";
    reportMetadata();
}

void Metadata::track(QString fPath, QString msg)
{
    if (G::isThreadTrackingOn) qDebug() << G::t.restart() << "\t" << "• Metadata Caching" << fPath << msg;
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
     if (order == 0x4D4D) return static_cast<unsigned long>
             ((c[0]&0xFF) << 8 | (c[1]&0xFF));
     else return static_cast<unsigned int>
             ((c[0]&0xFF) | (c[1]&0xFF) << 8);
}

ulong Metadata::get4(QByteArray c)
{
    if (order == 0x4D4D) return static_cast<unsigned long>
            ((c[0]&0xFF) << 24 | (c[1]&0xFF) << 16 | (c[2]&0xFF) << 8 | (c[3]&0xFF));
    else return static_cast<unsigned long>
            ((c[0]&0xFF) | (c[1]&0xFF) << 8 | (c[2]&0xFF) << 16 | (c[3]&0xFF) << 24);
}

ulong Metadata::get2(long offset)
{
    file.seek(offset);
    return get2(file.read(2));
}

ulong Metadata::get4(long offset)
{
    file.seek(offset);
    return get4(file.read(4));
}

float Metadata::getReal(long offset)
{
/*
In IFD type 5, 10, 11, 12 = rational = real/float
*/
    file.seek(offset);
    ulong a = get4(file.read(4));
    ulong b = get4(file.read(4));
//    qDebug() << G::t.restart() << "\t" << "a =" << a << "b =" << b << "a/b =" << (float)a/b;
    if (b == 0) return 0;
    float x = (float)a / b;
    return x;
}

QString Metadata::getString(ulong offset, ulong length)
{
/*
In IFD type 2 = string
*/
    file.seek(offset);
    return(file.read(length));
}

QByteArray Metadata::getByteArray(ulong offset, ulong length)
{
    file.seek(offset);
    return(file.read(length));
}

int Metadata::getNewOrientation(int orientation, int rotation)
{
    int degrees = orientationToDegrees[orientation];
    degrees += rotation;
    if (degrees > 270) degrees -= 360;
    return orientationFromDegrees[degrees];
}

bool Metadata::writeMetadata(const QString &fPath, QByteArray &buffer)
{
/*
Called from ingest (Ingestdlg).  If it is a supported image type a copy of the
image file is made and any metadata changes are updated in buffer.  If it is a
raw file in the sidecarFormats hash then the xmp data for existing and changed
metadata is written to buffer and the original image file is copied unchanged.
*/
    // is xmp supported for this file
    QFileInfo info(fPath);
    QString suffix = info.suffix().toLower();
    if (!xmpWriteFormats.contains(suffix)) return false;

//    qDebug() << "Metadata::writeMetadata fPath =" << fPath;
    // set locals to image data  ie title = metaCache[fPath].title
    setMetadata(fPath);
//    reportMetadataCache(fPath);

    bool useSidecar = sidecarFormats.contains(suffix);

    // new orientation
    int newOrientation = getNewOrientation(orientation, rotationDegrees);

    // has metadata been edited? ( _ is original data)
    bool ratingChanged = rating != ""; //_rating;
    bool labelChanged = label != "";  //_label;
    bool titleChanged = title != _title;
    bool creatorChanged = creator != _creator;
    bool copyrightChanged = copyright != _copyright;
    bool emailChanged = email != _email;
    bool urlChanged = url != _url;
    bool orientationChanged = orientation != newOrientation;
    if (   !ratingChanged
        && !labelChanged
        && !titleChanged
        && !creatorChanged
        && !copyrightChanged
        && !emailChanged
        && !urlChanged
        && !orientationChanged ) return false;

    // data edited, open image file
    file.setFileName(fPath);
    file.open(QIODevice::ReadOnly);

    // update xmp data
    Xmp xmp(file, xmpSegmentOffset, xmpNextSegmentOffset, useSidecar);

    // orientation is written to xmp sidecars only
    if (orientationChanged && useSidecar) {
        QString s = QString::number(newOrientation);
        xmp.setItem("Orientation", s.toLatin1());
    }
    if (urlChanged) xmp.setItem("CiUrlWork", url.toLatin1());
    if (emailChanged) xmp.setItem("CiEmailWork", email.toLatin1());
    if (copyrightChanged) xmp.setItem("rights", copyright.toLatin1());
    if (creatorChanged) xmp.setItem("creator", creator.toLatin1());
    if (titleChanged) xmp.setItem("title", title.toLatin1());
    if (labelChanged) xmp.setItem("Label", label.toLatin1());
    if (ratingChanged) xmp.setItem("Rating", rating.toLatin1());

    QString modifyDate = QDateTime::currentDateTime().toOffsetFromUtc
        (QDateTime::currentDateTime().offsetFromUtc()).toString(Qt::ISODate);
    xmp.setItem("ModifyDate", modifyDate.toLatin1());

    // get the buffer to write to a new file
    if (suffix == "jpg") {
        file.seek(0);
        buffer = file.readAll();
        /* Update orientation first, as orientation is written to EXIF, not
        XMP, for known formats. Writing subsequent xmp could change file length
        and make the orientationOffset incorrect.
        */
        if (orientationChanged && orientationOffset > 0) {
            QChar c = newOrientation & 0xFF;
            QByteArray ba;
            ba.append(c);
            buffer.replace(orientationOffset, 1, ba);
        }
        xmp.writeJPG(buffer);
    }
    if (useSidecar)
        xmp.writeSidecar(buffer);

    return true;
}

ulong Metadata::findInFile(QString s, ulong offset, ulong range)
{
/*
Returns the file offset to the start of the search string. If not
found returns -1.

QFile file must be assigned and open.
*/
    uint firstCharCode = static_cast<unsigned int>(s[0].unicode());
    file.seek(offset);
    for (ulong i = offset; i < offset + range; i++) {
        if (get1(file.read(1)) == firstCharCode) {
            bool rejected = false;
            for (int j = 1; j < s.length(); j++) {
                uint nextCharCode = static_cast<unsigned int>(s[j].unicode());
                uint byte = get1(file.read(1));
                if (byte != nextCharCode) rejected = true;
                if (rejected) break;
            }
            if (!rejected) return file.pos() - s.length();
        }
    }
    return -1;
}

bool Metadata::readXMP(ulong offset)
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    order = 0x4D4D;                  // only IFD/EXIF can be little endian

    // skip over APP FFE1 bytes
    file.seek(offset);
    ulong xmpOffsetStart;
    // find endo of XMP block at next FF
    bool xmpFound = false;
    QByteArray xmpByteArray;

//    qDebug() << G::t.restart() << "\t" << "Starting readXMP";
    // look for the start of XMP block = "<?xpacket begin" ...
    QString s = "<?xpacket begin";
    xmpOffsetStart = findInFile(s, offset, 200);
    if (xmpOffsetStart == -1) return false;
    s = "<?xpacket end";
    ulong xmpOffsetEnd = findInFile(s, xmpOffsetStart, 64555);
    if (xmpOffsetEnd == -1) return false;
    xmpOffsetEnd = findInFile(">", xmpOffsetEnd, 100) + 2;  // 38306
    ulong xmpLength = xmpOffsetEnd - xmpOffsetStart;
    if (xmpLength > 64555) return false;
    file.seek(xmpOffsetStart);
    xmpByteArray = file.read(xmpLength);
    xmpFound = true;

    QString xmpTitle;
    if (!xmpFound) return false;

    QXmlStreamReader xmpBa(xmpByteArray);
//    bool err = xmpBa.hasError();

    // search xmp for data
    while(!xmpBa.atEnd() && !xmpBa.hasError()) {
        QXmlStreamReader::TokenType token = xmpBa.readNext();
        // lots of stuff to ignore
        if (token != QXmlStreamReader::StartElement &&
            token != QXmlStreamReader::QXmlStreamReader::Characters) continue;
        if (xmpBa.isWhitespace()) continue;
        if (xmpBa.qualifiedName() == "rdf:li") continue;
        if (xmpBa.qualifiedName() == "rdf:Seq") continue;
        if (xmpBa.qualifiedName() == "rdf:Alt") continue;
        if (xmpBa.text() == "" && xmpBa.qualifiedName() == "") continue;

        // get lens
        if (xmpBa.qualifiedName() == "rdf:Description") {
            for(int i = 0; i < xmpBa.attributes().size(); i++) {
                if (xmpBa.attributes().at(i).name() == "Lens") {
                    lens = xmpBa.attributes().at(i).value().toString();
                    break;
                }
            }
        }

        // get creator
        if (xmpBa.qualifiedName() == "dc:creator") {
            int count = 0;
            bool notFound = true;
            while(notFound || count < 4) {
                count++;
                xmpBa.readNext();
                if(xmpBa.qualifiedName() == "" && !xmpBa.isWhitespace()) {
//                    qDebug() << G::t.restart() << "\t" << xmpBa.qualifiedName() << xmpBa.text();
                    creator = xmpBa.text().toString();
                    notFound = false;
                }
            }
        }

        // get xmpTitle (already tried in IPTC so this is redundent
        if (xmpBa.qualifiedName() == "dc:title") {
            int count = 0;
            bool notFound = true;
            while(notFound || count < 4) {
                count++;
                xmpBa.readNext();
                if(xmpBa.qualifiedName() == "" && !xmpBa.isWhitespace()) {
//                    qDebug() << G::t.restart() << "\t" << xmpBa.qualifiedName() << xmpBa.text();
                    xmpTitle = xmpBa.text().toString();
                    notFound = false;
                }
            }
        }

        // get copyright
        if (xmpBa.qualifiedName() == "dc:rights") {
            int count = 0;
            bool notFound = true;
            while(notFound || count < 4) {
                count++;
                xmpBa.readNext();
                if(xmpBa.qualifiedName() == "" && !xmpBa.isWhitespace()) {
//                    qDebug() << G::t.restart() << "\t" << xmpBa.qualifiedName() << xmpBa.text();
                    copyright = xmpBa.text().toString();
                    notFound = false;
                }
            }
        }

        // get email and url
        if (xmpBa.qualifiedName() == "Iptc4xmpCore:CreatorContactInfo") {
            for(int i = 0; i < xmpBa.attributes().size(); i++) {
                if (xmpBa.attributes().at(i).name() == "CiEmailWork")
                    email = xmpBa.attributes().at(i).value().toString();
                if (xmpBa.attributes().at(i).name() == "CiUrlWork")
                    url = xmpBa.attributes().at(i).value().toString();
            }
        }
    }
//    qDebug() << G::t.restart() << "\t" << "Lens:" << lens;
//    qDebug() << G::t.restart() << "\t" << "Creator" << creator;
//    qDebug() << G::t.restart() << "\t" << "Copyright" << copyright;
//    qDebug() << G::t.restart() << "\t" << "xmpTitle" << xmpTitle;
//    qDebug() << G::t.restart() << "\t" << "email" << email;
//    qDebug() << G::t.restart() << "\t" << "url" << url;

    return false;
}


void Metadata::readIPTC(ulong offset)
{
/*
If the IPTC data is in a JPG segment then there is some header info to get
through to get to the start of the actual IPTC data blocks.

If the IPTC data is in a photoshop TIF file, then the IPTC offset is directly
to the first IPTC data block so we can skip the search req'd if it was JPG.
*/
    order = 0x4D4D;                  // only IFD/EXIF can be little endian
    bool foundIPTC = false;
    file.seek(offset);

    // check to see if the offset is a JPG segment
    ulong marker = get2(file.peek(2));
    if (marker == 0xFFED) {
        // skip the APP marker FFED and length bytes
        file.seek(offset + 2);
        ulong segmentLength = get2(file.read(2));
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
        if (!foundIPTC) return;

        // calc pascal-style string length
        int pasStrLen = file.read(1).toInt() + 1;
        pasStrLen = (pasStrLen % 2) ? pasStrLen + 1: pasStrLen;
        file.seek(file.pos() + pasStrLen - 1);
        // read size of resource data
//        ulong resourceDataSize = get4(file.read(4));
//        qint64 endResourceData = file.pos() + resourceDataSize - 4;
    }

    // read IPTC data blocks searching for title (0x05)
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
//            qDebug() << G::t.restart() << "\t" << "IPTC title length =" << dataLength
//                     << "title =" << title;
            foundTitle = true;
        }
        else file.seek(file.pos() + dataLength);
    }
}

bool Metadata::readIRB(ulong offset)
{
/*
Read a Image Resource Block looking for embedded thumb
    - see https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
This is a recursive function, iterating through consecutive resource blocks until
the embedded jpg preview is found (irbID == 1036)
*/
    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
//    #endif
    }

    // Photoshop IRBs use big endian
    long oldOrder = order;
    order = 0x4D4D;

    // check signature to make sure this is the start of an IRB
    file.seek(offset);
    QByteArray irbSignature("8BIM");
    QByteArray signature = file.read(4);
    if (signature != irbSignature) {
        order = oldOrder;
        return foundTifThumb;
    }

    // Get the IRB ID (we're looking for 1036 = thumb)
    uint irbID = get2(file.read(2));
    if (irbID == 1036) foundTifThumb = true;

    // read the pascal string which we don't care about
    uint pascalStringLength = get2(file.read(2));
    if (pascalStringLength > 0) file.read(pascalStringLength);

    // get the length of the IRB data block
    ulong dataBlockLength = get4(file.read(4));
    // round to even 2 bytes
    dataBlockLength % 2 == 0 ? dataBlockLength : dataBlockLength++;

    // reset order as going to return or recurse next
    order = oldOrder;

    // found the thumb, collect offsets and return
    if (foundTifThumb) {
        offsetThumbJPG = file.pos() + 28;
        lengthThumbJPG = dataBlockLength - 28;
        return foundTifThumb;
    }

    // did not find the thumb, try again
    file.read(dataBlockLength);
    offset = file.pos();
    readIRB(offset);

    // make the compiler happy
    return false;
}

ulong Metadata::readIFD(QString hdr, ulong offset)
{
    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
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
        reportMetadataHeader(hdr);
        rpt << "IFDOffset  Hex: "
            << QString::number(offset, 16).toUpper()
            << "   Dec: " << offset << "\n"
            << "Num  Offset     hex  tagId   hex  tagType  tagCount  tagValue   tagDescription\n";
    }
    ulong pos;
    QString tagDescription;
    for (int i = 0; i < tags; i++){
//        if (report) pos = QString::number(file.pos(), 16).toUpper();
        pos = file.pos();
        tagId = get2(file.read(2));
        tagType = get2(file.read(2));
        tagCount = get4(file.read(4));
        // check for orientation and save offset for subsequent writing
        if (hdr == "IFD0" && tagId == 274) orientationOffset = file.pos();
        if (tagType == 3) tagValue = get2(file.read(4));
        else tagValue = get4(file.read(4));

        ifdData.tagType = tagType;
        ifdData.tagCount = tagCount;
        ifdData.tagValue = tagValue;

        ifdDataHash.insert(tagId, ifdData);

        if (report) {
            if (hdr == "IFD Exif")
                (exifHash.contains(tagId)) ? tagDescription = exifHash.value(tagId)
                    : tagDescription = "Undefined tag";
            else if (hdr == "IFD Nikon Maker Note")
                (nikonMakerHash.contains(tagId)) ? tagDescription = nikonMakerHash.value(tagId)
                    : tagDescription = "Undefined tag";
            else if (hdr == "IFD Canon Maker Note")
                (canonMakerHash.contains(tagId)) ? tagDescription = canonMakerHash.value(tagId)
                    : tagDescription = "Undefined tag";
            else if (hdr == "IFD Canon FileInfo")
                (canonFileInfoHash.contains(tagId)) ? tagDescription = canonFileInfoHash.value(tagId)
                    : tagDescription = "Undefined tag";
            else if (hdr == "IFD Sony Maker Note")
                (sonyMakerHash.contains(tagId)) ? tagDescription = sonyMakerHash.value(tagId)
                    : tagDescription = "Undefined tag";
            else if (hdr == "IFD Fuji Maker Note")
                (fujiMakerHash.contains(tagId)) ? tagDescription = fujiMakerHash.value(tagId)
                    : tagDescription = "Undefined tag";
            else if (hdr == "IFD Panasonic Maker Note")
                (panasonicMakerHash.contains(tagId)) ? tagDescription = panasonicMakerHash.value(tagId)
                    : tagDescription = "Undefined tag";

            else if (hdr.left(3) == "Sub" || hdr.left(3) == "IFD")
                (ifdHash.contains(tagId)) ? tagDescription = ifdHash.value(tagId)
                    : tagDescription = "Undefined tag";

            //            else if (hdr.left(3) == "Sub" || hdr.left(3) == "IFD")
//                (ifdHash.contains(tagId)) ? tagDescription = ifdHash.value(tagId)
//                    : tagDescription = "Undefined tag";

            rpt.setFieldWidth(3);
            rpt.setFieldAlignment(QTextStream::AlignRight);
            rpt << QString::number(i, 10).toUpper();
            rpt.setFieldWidth(8);
            rpt.setFieldAlignment(QTextStream::AlignRight);
            rpt << QString::number(pos, 10).toUpper();
            rpt << QString::number(pos, 16).toUpper();
            rpt.setFieldWidth(7);
            rpt << tagId;
            rpt.setFieldWidth(6);
            rpt << QString::number(tagId, 16).toUpper();
            rpt.setFieldWidth(9);
            rpt << tagType;
            rpt.setFieldWidth(10);
            rpt << tagCount << tagValue;
            rpt.setFieldWidth(3);
            rpt << "   ";
            rpt.setFieldWidth(50);
            rpt.setFieldAlignment(QTextStream::AlignLeft);
            rpt << tagDescription;
            rpt.reset();
            rpt << "\n";

        }
        // quit if more than 200 tags - prob error
        if (i>200) break;
    }
    ulong nextIFDOffset = get4(file.read(4));
    if (report) {
        rpt.setFieldWidth(8);
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt << QString::number(file.pos(), 10).toUpper();
        rpt << QString::number(file.pos(), 16).toUpper();
        rpt.reset();
        rpt << " nextIFDOffset = ";
        rpt << QString::number(nextIFDOffset, 10).toUpper();
        rpt << " / " << QString::number(nextIFDOffset, 16).toUpper();
        rpt << "\n";
    }
    return(nextIFDOffset);
}

QList<ulong> Metadata::getSubIfdOffsets(ulong subIFDaddr, int count)
{
    {
        #ifdef ISDEBUG
        G::track(__FUNCTION__);
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
/*
The JPG file structure is based around a series of file segments.  The marker at
the start of each segment is FFC0 to FFFE (see initSegCodeHash). The next two
bytes are the incremental offset to the next segment.

This function walks through all the segments and records their global offsets in
segmentHash.  segmentHash is used by formatJPG to access the EXIF, IPTC and XMP
segments.

In addition, the XMP offset and nextOffset are set to facilitate editing XMP data.
*/
    segmentHash.clear();
    isXmp = false;
    order = 0x4D4D;                  // only IFD/EXIF can be little endian
    uint marker = 0xFFFF;
    while (marker > 0xFFBF) {
        file.seek(offset);           // APP1 FFE*
        marker = get2(file.read(2));
        if (marker < 0xFFC0) break;
        ulong pos = file.pos();
        ulong nex = get2(file.read(2));
        ulong nextOffset = pos + nex;
        if (marker == 0xFFE1) {
            QString segName = file.read(4);
            if (segName == "Exif") segmentHash["EXIF"] = offset;
            if (segName == "http") segName += file.read(15);
            if (segName == "http://ns.adobe.com") {
                segmentHash["XMP"] = offset;
                xmpSegmentOffset = offset;
                xmpNextSegmentOffset = nextOffset;
                isXmp = true;
            }
        }
        else if (segCodeHash.contains(marker)) {
            segmentHash[segCodeHash[marker]] = offset;
        }
        offset = nextOffset;
    }
    if (report) {
        reportMetadataHeader("JPG Segment Hash");
        rpt << "Segment\tOffset\t\tHex\n";

        QHashIterator<QString, ulong> i(segmentHash);
        while (i.hasNext()) {
            i.next();
            rpt << i.key() << ":\t\t" << i.value() << "\t\t"
                << QString::number(i.value(), 16).toUpper() << "\n";
        }
    }
}

bool Metadata::getDimensions(ulong jpgOffset)
{
    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
//    #endif
    }
    long order1 = order;
    order = 0x4D4D;                  // only IFD/EXIF can be little endian
    ulong marker = 0;
    ulong offset = jpgOffset + 2;
    while (marker != 0xFFC0) {
        file.seek(offset);           // APP1 FFE*
        marker = get2(file.read(2));
        if (marker < 0xFF01) {
            order = order1;
            return false;
        }
        offset = get2(file.peek(2)) + file.pos();
    }
    file.seek(file.pos()+3);
    height = get2(file.read(2));
    width = get2(file.read(2));
//    width = get2(file.read(2));
//    height = get2(file.read(2));
    order = order1;
    return true;
}

void Metadata::verifyEmbeddedJpg(ulong &offset, ulong &length)
{
/*
JPEGs start with FFD8 and end with FFD9.  This function confirms the embedded
JPEG is correct.  If it is not then the function sets the offset and length
to zero.  At the end of the readMetadata function the offsets and lengths are
checked to make sure there is valid data.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    file.seek(offset);
    if (get2(file.peek(2)) == 0xFFD8) {
        file.seek(offset + length - 2);
        if (get2(file.peek(2)) == 0xFFD9) {
            // all is well
            return;
        }
    }
    // problem - set to zero
    offset = 0;
    length = 0;
}

void Metadata::formatNikon()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // moved file.open to readMetadata

    // get endian
    order = get2(file.read(4));
    // get offset to first IFD and read it
    ulong offsetIfd0 = get4(file.read(4));

    // Nikon does not chain IFDs
    readIFD("IFD0", offsetIfd0);

    // pull data reqd from IFD0
    make = getString(ifdDataHash.value(271).tagValue, ifdDataHash.value(271).tagCount);
    model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount);
    orientation = ifdDataHash.value(274).tagValue;
    creator = getString(ifdDataHash.value(315).tagValue, ifdDataHash.value(315).tagCount);

    // xmp offset
    xmpSegmentOffset = ifdDataHash.value(700).tagValue;
    // xmpNextSegmentOffset used to later calc available room in xmp
    xmpNextSegmentOffset = ifdDataHash.value(700).tagCount + xmpSegmentOffset;
    if (xmpSegmentOffset) isXmp = true;
    else isXmp = false;

    ulong offsetEXIF = 0;
    offsetEXIF = ifdDataHash.value(34665).tagValue;

//    reportMetadata();

    /* NIkon provides an offset in IFD0 to the offsets for all the subIFDs
    in subIFD0.   Models including the D2H and before have a different structure
    with only one subIFD and an additional preview IFD identified in maker
    notes */
    QList<ulong> ifdOffsets;
    if(ifdDataHash.contains(330)) {
        if (ifdDataHash.value(330).tagCount > 1)
            ifdOffsets = getSubIfdOffsets(ifdDataHash.value(330).tagValue,
                                      ifdDataHash.value(330).tagCount);
        else ifdOffsets.append(ifdDataHash.value(330).tagValue);

        QString hdr;
        // SubIFD1 contains full size jpg offset and length
        if (ifdOffsets.count() > 0) {
            hdr = "SubIFD1";
            readIFD(hdr, ifdOffsets[0]);
            // pull data reqd from SubIFD1
            offsetFullJPG = ifdDataHash.value(513).tagValue;
            lengthFullJPG = ifdDataHash.value(514).tagValue;
//            if (lengthFullJPG) verifyEmbeddedJpg(offsetFullJPG, lengthFullJPG);
            // D2H and older
            width = ifdDataHash.value(256).tagValue;
            height = ifdDataHash.value(257).tagValue;
        }

        // pull data reqd from SubIFD2
        // SubIFD2 contains image width and height

        if (ifdOffsets.count() > 1) {
            hdr = "SubIFD2";
            readIFD(hdr, ifdOffsets[1]);
            width = ifdDataHash.value(256).tagValue;
            height = ifdDataHash.value(257).tagValue;
        }

        // SubIFD3 contains small size jpg offset and length
        if (ifdOffsets.count() > 2) {
            hdr = "SubIFD3";
            readIFD(hdr, ifdOffsets[2]);
            offsetSmallJPG = ifdDataHash.value(513).tagValue;
            lengthSmallJPG = ifdDataHash.value(514).tagValue;
//            if (lengthSmallJPG) verifyEmbeddedJpg(offsetSmallJPG, lengthSmallJPG);
        }
    }

    // read ExifIFD
    readIFD("IFD Exif", offsetEXIF);

    // EXIF: created datetime
    QString createdExif;
    createdExif = getString(ifdDataHash.value(36868).tagValue,
        ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // Exif: get shutter speed
    if (ifdDataHash.contains(33434)) {
        float x = getReal(ifdDataHash.value(33434).tagValue);
        if (x <1 ) {
            uint t = qRound(1/x);
            exposureTime = "1/" + QString::number(t);
            exposureTimeNum = x;
        } else {
            uint t = (uint)x;
            exposureTime = QString::number(t);
            exposureTimeNum = t;
        }
        exposureTime += " sec";
    } else {
        exposureTime = "";
    }
    // Exif: aperture
    if (ifdDataHash.contains(33437)) {
        float x = getReal(ifdDataHash.value(33437).tagValue);
        aperture = "f/" + QString::number(x, 'f', 1);
        apertureNum = qRound(x * 10) / 10.0;
    } else {
        aperture = "";
        apertureNum = 0;
    }
    // Exif: ISO
    if (ifdDataHash.contains(34855)) {
        ulong x = ifdDataHash.value(34855).tagValue;
        ISONum = static_cast<int>(x);
        ISO = QString::number(ISONum);
    } else {
        ISO = "";
        ISONum = 0;
    }
    // Exif: focal length
    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue);
        focalLengthNum = static_cast<int>(x);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "";
        focalLengthNum = 0;
    }

    // Exif: read makernoteIFD
    ulong makerOffset = ifdDataHash.value(37500).tagValue;
    if (ifdDataHash.contains(37500)) {

        /* The makerOffsetBase starts 10 or 12 bits after the offset at endian
        4949 for all recent Nikons. Earlier Nikons, D2H and before, use an endian
        4D4D.  All offsets in the makernoteIFD are relative to makerOffsetBase.
        */

        long endian;
        bool foundEndian = false;
        file.seek(makerOffset);
        while (!foundEndian) {
            endian = get2(file.read(2));
            if (endian == 0x4949 || endian == 0x4D4D) foundEndian = true;
            order = endian;
//            if (get2(file.read(2)) == 0x4949) foundEndian = true;
        }
        ulong makerOffsetBase = file.pos() - 2;

        if (report) {
            rpt << "\nMaker Offset = "
                << makerOffset
                << " / " << QString::number(makerOffset, 16)
                << "  Maker offset base = "
                << makerOffsetBase
                << " / " << QString::number(makerOffsetBase, 16);
        }

        readIFD("IFD Nikon Maker Note", makerOffsetBase + 8);
        // Get serial number, shutter count and lens type to decrypt the lens info
        cameraSN = getString(ifdDataHash.value(29).tagValue + makerOffsetBase,
                                       ifdDataHash.value(29).tagCount);
        shutterCount = ifdDataHash.value(167).tagValue;
        uint lensType = 0;
        lensType = ifdDataHash.value(131).tagValue;

        uint32_t serial = cameraSN.toInt();
        uint32_t count = shutterCount;
        QByteArray encryptedLensInfo = "";
        encryptedLensInfo = getByteArray(ifdDataHash.value(152).tagValue + makerOffsetBase,
                                               ifdDataHash.value(152).tagCount);
        QByteArray lensInfo = nikonDecrypt(encryptedLensInfo, count, serial);
        // the byte array code is in the middle of the lensInfo byte stream
        lensInfo.remove(0, 12);
        lensInfo.remove(7, lensInfo.size() - 7);
        lensInfo.append(lensType);
        nikonLensCode = lensInfo.toHex().toUpper();
        lens = nikonLensHash.value(nikonLensCode);
        /*
        or could go with nikonLensHash<QString, QString>
        and use lensInfo.toHex().toUpper() as the key  */

        // Find the preview IFD offset
        if (ifdDataHash.contains(17)) {
            readIFD("IFD Nikon Maker Note: PreviewIFD",
                    ifdDataHash.value(17).tagValue + makerOffsetBase);

            offsetSmallJPG = ifdDataHash.value(513).tagValue + makerOffsetBase;
            lengthSmallJPG = ifdDataHash.value(514).tagValue; // + makerOffsetBase;
//            if (lengthSmallJPG) verifyEmbeddedJpg(offsetSmallJPG, lengthSmallJPG);
        }
    }

    // read XMP
    if (isXmp && okToReadXmp) {
        Xmp xmp(file, xmpSegmentOffset, xmpNextSegmentOffset);
        rating = xmp.getItem("Rating");     // case is important "Rating"
        label = xmp.getItem("Label");       // case is important "Label"
        if (title.isEmpty()) title = xmp.getItem("title");       // case is important "title"
        if (cameraSN.isEmpty()) cameraSN = xmp.getItem("SerialNumber");
        if (lens.isEmpty()) lens = xmp.getItem("Lens");
        if (lensSN.isEmpty()) lensSN = xmp.getItem("LensSerialNumber");
        if (creator.isEmpty()) creator = xmp.getItem("creator");
        if (copyright.isEmpty()) copyright = xmp.getItem("rights");
        if (email.isEmpty()) email = xmp.getItem("CiEmailWork");
        if (url.isEmpty()) url = xmp.getItem("CiUrlWork");

        // save original values so can determine if edited when writing changes
        _title = title;
        _rating = rating;
        _label = label;

        if (report) xmpString = xmp.metaAsString();
    }

    if (report) reportMetadata();

}

void Metadata::formatCanon()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    //file.open in readMetadata
    // get endian
    order = get2(file.read(4));
    // is canon always offset 16 to IFD0 ???
    ulong offsetIfd0 = 16;
    ulong nextIFDOffset = readIFD("IFD0", offsetIfd0);

    // pull data reqd from IFD0
    offsetFullJPG = ifdDataHash.value(273).tagValue;
    lengthFullJPG = ifdDataHash.value(279).tagValue;
//    if (lengthFullJPG) verifyEmbeddedJpg(offsetFullJPG, lengthFullJPG);

    model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount);
    orientation = ifdDataHash.value(274).tagValue;
    creator = getString(ifdDataHash.value(315).tagValue, ifdDataHash.value(315).tagCount);
    copyright = getString(ifdDataHash.value(33432).tagValue, ifdDataHash.value(33432).tagCount);

    // xmp offset
    xmpSegmentOffset = ifdDataHash.value(700).tagValue;
    // xmpNextSegmentOffset used to later calc available room in xmp
    xmpNextSegmentOffset = ifdDataHash.value(700).tagCount + xmpSegmentOffset;
    if (xmpSegmentOffset) isXmp = true;
    else isXmp = false;

    // EXIF IFD offset (to be used in a little)
    ulong offsetEXIF = 0;
    offsetEXIF = ifdDataHash.value(34665).tagValue;

    if (nextIFDOffset) nextIFDOffset = readIFD("IFD1", nextIFDOffset);

    // pull data reqd from IFD1
    offsetThumbJPG = ifdDataHash.value(513).tagValue;
    lengthThumbJPG = ifdDataHash.value(514).tagValue;
//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);

    if (nextIFDOffset) nextIFDOffset = readIFD("IFD2", nextIFDOffset);

    // pull small size jpg from IFD2
    offsetSmallJPG = ifdDataHash.value(273).tagValue;
    lengthSmallJPG = ifdDataHash.value(279).tagValue;

    if (nextIFDOffset) nextIFDOffset = readIFD("IFD3", nextIFDOffset);

    // read ExifIFD
    readIFD("IFD Exif", offsetEXIF);

    // EXIF: created datetime
    QString createdExif;
    createdExif = getString(ifdDataHash.value(36868).tagValue,
        ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // EXIF: shutter speed
    if (ifdDataHash.contains(33434)) {
        float x = getReal(ifdDataHash.value(33434).tagValue);
        if (x <1 ) {
            uint t = qRound(1/x);
            exposureTime = "1/" + QString::number(t);
            exposureTimeNum = x;
        } else {
            uint t = (uint)x;
            exposureTime = QString::number(t);
            exposureTimeNum = t;
        }
        exposureTime += " sec";
    } else {
        exposureTime = "";
    }
    if (ifdDataHash.contains(40962)) {
        width = ifdDataHash.value(40962).tagValue;
    } else {
        width = 0;
    }
    // height
    height = ifdDataHash.value(40963).tagValue;
    // aperture
    if (ifdDataHash.contains(33437)) {
        float x = getReal(ifdDataHash.value(33437).tagValue);
        aperture = "f/" + QString::number(x, 'f', 1);
        apertureNum = qRound(x * 10) / 10.0;
    } else {
        aperture = "";
        apertureNum = 0;
    }
    //ISO
    if (ifdDataHash.contains(34855)) {
        ulong x = ifdDataHash.value(34855).tagValue;
        ISONum = static_cast<int>(x);
        ISO = QString::number(ISONum);
//        ISO = "ISO " + QString::number(x);
    } else {
        ISO = "";
        ISONum = 0;
    }
    // focal length
    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue);
        focalLengthNum = static_cast<int>(x);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "";
        focalLengthNum = 0;
    }
    // IFD Exif: lens
    lens = getString(ifdDataHash.value(42036).tagValue,
            ifdDataHash.value(42036).tagCount);

    // IFD Exif: camera serial number
    cameraSN = getString(ifdDataHash.value(42033).tagValue,
            ifdDataHash.value(42033).tagCount);

    // IFD Exif: lens serial nember
    lensSN = getString(ifdDataHash.value(42037).tagValue,
            ifdDataHash.value(42037).tagCount);

    // Exif: read makernoteIFD

    if (ifdDataHash.contains(37500)) {
        ulong makerOffset = ifdDataHash.value(37500).tagValue;
        readIFD("IFD Canon Maker Note", makerOffset);
    }

//    // Maker tag:  FileInfo IFD
//    if (ifdDataHash.contains(147)) {
//        ulong makerOffset = ifdDataHash.value(147).tagValue;
//        readIFD("IFD Canon FileInfo", makerOffset);
//    }



    // IFD Canon maker note: camera owner
//    (ifdDataHash.contains(9))
//        ? creator = getString(ifdDataHash.value(9).tagValue,
//        ifdDataHash.value(9).tagCount)
//        : creator = "";

    // read next app segment
    if (segmentHash.contains("IPTC")) readIPTC(segmentHash["IPTC"]);

    // read XMP
    if (segmentHash.contains("XMP")) {
        readXMP(segmentHash["XMP"]);
    }

    // read XMP
    if (isXmp && okToReadXmp) {
        Xmp xmp(file, xmpSegmentOffset, xmpNextSegmentOffset);
        rating = xmp.getItem("Rating");     // case is important "Rating"
        label = xmp.getItem("Label");       // case is important "Label"
        title = xmp.getItem("title");       // case is important "title"
        cameraSN = xmp.getItem("SerialNumber");
        if (lens.isEmpty()) lens = xmp.getItem("Lens");
        lensSN = xmp.getItem("LensSerialNumber");
        if (creator.isEmpty()) creator = xmp.getItem("creator");
        copyright = xmp.getItem("rights");
        email = xmp.getItem("CiEmailWork");
        url = xmp.getItem("CiUrlWork");

        // save original values so can determine if edited when writing changes
        _title = title;
        _rating = rating;
        _label = label;

        if (report) xmpString = xmp.metaAsString();
    }

    if (report) reportMetadata();
}

void Metadata::formatOlympus()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    //file.open in readMetadata
    // get endian
    order = get2(file.read(4));
    // get offset to first IFD and read it
    ulong offsetIfd0 = get4(file.read(4));

    readIFD("IFD0", offsetIfd0);

    // pull data reqd from IFD0
    model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount).trimmed();
    orientation = ifdDataHash.value(274).tagValue;
    creator = getString(ifdDataHash.value(315).tagValue, ifdDataHash.value(315).tagCount);
    copyright = getString(ifdDataHash.value(33432).tagValue, ifdDataHash.value(33432).tagCount);

    // get the offset for ExifIFD and read it
    ulong offsetEXIF;
    offsetEXIF = ifdDataHash.value(34665).tagValue;
    readIFD("IFD Exif", offsetEXIF);

    // EXIF: created datetime
    QString createdExif;
    createdExif = getString(ifdDataHash.value(36868).tagValue,
        ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // get shutter speed
    if (ifdDataHash.contains(33434)) {
        float x = getReal(ifdDataHash.value(33434).tagValue);
        if (x <1 ) {
            uint t = qRound(1/x);
            exposureTime = "1/" + QString::number(t);
            exposureTimeNum = x;
        } else {
            uint t = (uint)x;
            exposureTime = QString::number(t);
            exposureTimeNum = t;
        }
        exposureTime += " sec";
    } else {
        exposureTime = "";
    }
    // aperture
    if (ifdDataHash.contains(33437)) {
        float x = getReal(ifdDataHash.value(33437).tagValue);
        aperture = "f/" + QString::number(x, 'f', 1);
        apertureNum = qRound(x * 10) / 10.0;
    } else {
        aperture = "";
        apertureNum = 0;
    }
    //ISO
    if (ifdDataHash.contains(34855)) {
        ulong x = ifdDataHash.value(34855).tagValue;
        ISONum = static_cast<int>(x);
        ISO = QString::number(ISONum);
//        ISO = "ISO " + QString::number(x);
    } else {
        ISO = "";
        ISONum = 0;
    }
    // focal length
    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue);
        focalLengthNum = static_cast<int>(x);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "";
        focalLengthNum = 0;
    }
    // lens
    lens = getString(ifdDataHash.value(42036).tagValue,
                ifdDataHash.value(42036).tagCount);

    // read makernoteIFD
    // Offsets in makernote are relative to the start of the makernotes
    ulong makerOffset = ifdDataHash.value(37500).tagValue;
    if (ifdDataHash.contains(37500)) {
        // The IFD starts 10 or 12 bits after the offset to make room for the
        // string "OLYMPUS II  "
        readIFD("IFD Olympus Maker Note", makerOffset + 12);
        // Get the thumbnail Jpg offset and length
        offsetThumbJPG = ifdDataHash.value(256).tagValue + makerOffset;
        lengthThumbJPG = ifdDataHash.value(256).tagCount;
//        if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);

        // read CameraSettingsIFD
        if (ifdDataHash.contains(8224)) {
            readIFD("IFD Olympus Maker Note: CameraSettingsIFD",
                    ifdDataHash.value(8224).tagValue + makerOffset);
            offsetFullJPG = ifdDataHash.value(257).tagValue + makerOffset;
            lengthFullJPG = ifdDataHash.value(258).tagValue;
//            if (lengthFullJPG) verifyEmbeddedJpg(offsetFullJPG, lengthFullJPG);
            getDimensions(offsetFullJPG);
        }
    }

    // Olympus does not embed xmp in raw files
    // read XMP
//    if (segmentHash.contains("XMP")) {
//        readXMP(segmentHash["XMP"]);
//    }

    isXmp = false;

    // read XMP
    if (isXmp && okToReadXmp) {
        Xmp xmp(file, xmpSegmentOffset, xmpNextSegmentOffset);
        rating = xmp.getItem("Rating");     // case is important "Rating"
        label = xmp.getItem("Label");       // case is important "Label"
        if (title.isEmpty()) title = xmp.getItem("title");       // case is important "title"
        if (cameraSN.isEmpty()) cameraSN = xmp.getItem("SerialNumber");
        if (lens.isEmpty()) lens = xmp.getItem("Lens");
        if (lensSN.isEmpty()) lensSN = xmp.getItem("LensSerialNumber");
        if (creator.isEmpty()) creator = xmp.getItem("creator");
        if (copyright.isEmpty()) copyright = xmp.getItem("rights");
        if (email.isEmpty()) email = xmp.getItem("CiEmailWork");
        if (url.isEmpty()) url = xmp.getItem("CiUrlWork");

        // save original values so can determine if edited when writing changes
        _title = title;
        _rating = rating;
        _label = label;

        if (report) xmpString = xmp.metaAsString();
    }

    if (report) reportMetadata();
}

void Metadata::formatSony()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    //file.open in readMetadata
    // get endian
    order = get2(file.read(4));
    // get offset to first IFD and read it
    ulong offsetIfd0 = get4(file.read(4));

    ulong nextIFDOffset = readIFD("IFD0", offsetIfd0);

    // pull data reqd from IFD0
    offsetFullJPG = ifdDataHash.value(513).tagValue;
    lengthFullJPG = ifdDataHash.value(514).tagValue;
//    if (lengthFullJPG) verifyEmbeddedJpg(offsetFullJPG, lengthFullJPG);
    offsetThumbJPG = ifdDataHash.value(273).tagValue;
    lengthThumbJPG = ifdDataHash.value(279).tagValue;
//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);
    model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount);
    orientation = ifdDataHash.value(274).tagValue;

    ulong offsetEXIF;
    offsetEXIF = ifdDataHash.value(34665).tagValue;

    if (nextIFDOffset) readIFD("IFD1", nextIFDOffset);

    // IFD 1:
    offsetThumbJPG = ifdDataHash.value(513).tagValue;
    lengthThumbJPG = ifdDataHash.value(514).tagValue;
//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);

    // Sony provides an offset in IFD0 to the offsets for
    // all the subIFDs
    // get the offsets for the subIFD and read them
    QList<ulong> ifdOffsets;
    ifdOffsets = getSubIfdOffsets(ifdDataHash.value(330).tagValue,
                                      ifdDataHash.value(330).tagCount);

    // SubIFD1 contains full size jpg offset and length
    QString hdr = "SubIFD0";
//     readIFD(hdr, ifdOffsets[0]);     // req'd??

    // get the offset for ExifIFD and read it
    readIFD("IFD Exif", offsetEXIF);

    // IFD EXIF: dimensions
    width = ifdDataHash.value(40962).tagValue;
    height = ifdDataHash.value(40963).tagValue;

    // EXIF: created datetime
    QString createdExif;
    createdExif = getString(ifdDataHash.value(36868).tagValue,
        ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // Exif: get shutter speed
    if (ifdDataHash.contains(33434)) {
        float x = getReal(ifdDataHash.value(33434).tagValue);
        if (x <1 ) {
            uint t = qRound(1/x);
            exposureTime = "1/" + QString::number(t);
            exposureTimeNum = x;
        } else {
            uint t = (uint)x;
            exposureTime = QString::number(t);
            exposureTimeNum = t;
        }
        exposureTime += " sec";
    } else {
        exposureTime = "";
    }

    // Exif: aperture
    if (ifdDataHash.contains(33437)) {
        float x = getReal(ifdDataHash.value(33437).tagValue);
        aperture = "f/" + QString::number(x, 'f', 1);
        apertureNum = qRound(x * 10) / 10.0;
    } else {
        aperture = "";
        apertureNum = 0;
    }

    //Exif: ISO
    if (ifdDataHash.contains(34855)) {
        ulong x = ifdDataHash.value(34855).tagValue;
        ISONum = static_cast<int>(x);
        ISO = QString::number(ISONum);
//        ISO = "ISO " + QString::number(x);
    } else {
        ISO = "";
        ISONum = 0;
    }

    // Exif: focal length
    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue);
        focalLengthNum = static_cast<int>(x);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "";
        focalLengthNum = 0;
    }

    // Exif: lens
    lens = getString(ifdDataHash.value(42036).tagValue,
        ifdDataHash.value(42036).tagCount);

    // Exif: read makernoteIFD

    if (ifdDataHash.contains(37500)) {
        ulong makerOffset = ifdDataHash.value(37500).tagValue;
        readIFD("IFD Sony Maker Note", makerOffset);
    }

    // Sony does not embed xmp in raw files
    // read XMP
//    if (isXmp && okToReadXmp) {
//        Xmp xmp(file, xmpSegmentOffset, xmpNextSegmentOffset);
//        rating = xmp.getItem("Rating");     // case is important "Rating"
//        label = xmp.getItem("Label");       // case is important "Label"
//        if (title.isEmpty()) title = xmp.getItem("title");       // case is important "title"
//        if (cameraSN.isEmpty()) cameraSN = xmp.getItem("SerialNumber");
//        if (lens.isEmpty()) lens = xmp.getItem("Lens");
//        if (lensSN.isEmpty()) lensSN = xmp.getItem("LensSerialNumber");
//        if (creator.isEmpty()) creator = xmp.getItem("creator");
//        if (copyright.isEmpty()) copyright = xmp.getItem("rights");
//        if (email.isEmpty()) email = xmp.getItem("CiEmailWork");
//        if (url.isEmpty()) url = xmp.getItem("CiUrlWork");

//        // save original values so can determine if edited when writing changes
//        _title = title;
//        _rating = rating;
//        _label = label;

//        if (report) xmpString = xmp.metaAsString();
//    }

    if (report) reportMetadata();

}

bool Metadata::formatFuji()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    //file.open in readMetadata

    /* Fuji File Format

    General structure:
    Fuji information
    Directory of offsets
    JPEG file
    CFA Header
    CFA

    Offset          Bytes   Description
    000 - 015       16      "FUJIFILMCCD-RAW "
    016 - 019       4       Format fersion ie "0201"
    020 - 027       8       Camera ID ie "FF129502"
    028 - 059       32      32 bytes for the camera string, \0 terminated
    Start of offset directory
    060 - 063       4       Directory version ie "0100", "0159", "0135"
    064 - 083       20      Unkonwn
    084 - 087       4       JPEG image offset
    088 - 091       4       JPEG image length
    092 - 095       4       CFA Header Offset (Color Filter Array)
    096 - 099       4       CFA Header Length
    100 - 103       4       CFA offset
    104 - 107       4       CFA length
    108 - (148 or JPG offset) Unknown
    148 - 151 or close = JPEG image offset FF D8 FF E1
    */

    // Fuji is big endian
    order = 0x4D4D;
    ulong startOffset;

    // read first 16 bytes to confirm it is a fuji file
    if (file.read(16) != "FUJIFILMCCD-RAW ") return false;

    // seek JPEG image offset
    file.seek(84);
    offsetFullJPG = get4(file.read(4));
    lengthFullJPG = get4(file.read(4));
//    if (lengthFullJPG) verifyEmbeddedJpg(offsetFullJPG, lengthFullJPG);
    file.seek(offsetFullJPG);

    // start on embedded JPEG
    if (get2(file.read(2)) != 0xFFD8) return false;

    // build a hash of jpg segment offsets
    getSegments(file.pos());

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

    if (report) rpt << "\n startOffset = " << startOffset;

    uint a = get2(file.read(2));  // magic 42
    a = get4(file.read(4));
    ulong offsetIfd0 = a + startOffset;

//    getDimensions(offsetFullJPG);

    // read IFD0:
    ulong nextIFDOffset = readIFD("IFD0", offsetIfd0) + startOffset;
    ulong offsetEXIF = ifdDataHash.value(34665).tagValue + startOffset;
    orientation = ifdDataHash.value(274).tagValue;
    make = getString(ifdDataHash.value(271).tagValue + startOffset,
                     ifdDataHash.value(271).tagCount);
    model = getString(ifdDataHash.value(272).tagValue + startOffset,
                      ifdDataHash.value(272).tagCount);
    copyright = getString(ifdDataHash.value(33432).tagValue + startOffset,
                          ifdDataHash.value(33432).tagCount);

    // read IFD1
    if (nextIFDOffset) nextIFDOffset = readIFD("IFD1", nextIFDOffset);
    offsetThumbJPG = ifdDataHash.value(513).tagValue + startOffset;
    lengthThumbJPG = ifdDataHash.value(514).tagValue + startOffset;
//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);

    // read EXIF IFD
    readIFD("IFD Exif", offsetEXIF);
    width = ifdDataHash.value(40962).tagValue;
    height = ifdDataHash.value(40963).tagValue;
    if (!width || !height) getDimensions(offsetFullJPG);

    // EXIF: shutter speed
    if (ifdDataHash.contains(33434)) {
        float x = getReal(ifdDataHash.value(33434).tagValue + startOffset);
        if (x < 1 ) {
            uint t = qRound(1 / x);
            exposureTime = "1/" + QString::number(t);
            exposureTimeNum = x;
        } else {
            uint t = (uint)x;
            exposureTime = QString::number(t);
            exposureTimeNum = t;
        }
        exposureTime += " sec";
    } else {
        exposureTime = "";
    }

    // EXIF: aperture
    if (ifdDataHash.contains(33437)) {
        float x = getReal(ifdDataHash.value(33437).tagValue + startOffset);
        aperture = "f/" + QString::number(x, 'f', 1);
        apertureNum = qRound(x * 10) / 10.0;
    } else {
        aperture = "";
        apertureNum = 0;
    }

    // EXIF: ISO
    if (ifdDataHash.contains(34855)) {
        ulong x = ifdDataHash.value(34855).tagValue;
        ISONum = static_cast<int>(x);
        ISO = QString::number(ISONum);
    } else {
        ISO = "";
        ISONum = 0;
    }

    // EXIF: focal length
    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue + startOffset);
        focalLengthNum = static_cast<int>(x);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "";
        focalLengthNum = 0;
    }

    // EXIF: lens model
    lens = getString(ifdDataHash.value(42036).tagValue + startOffset,
                     ifdDataHash.value(42036).tagCount);

    // Exif: read makernoteIFD
    if (ifdDataHash.contains(37500)) {
        ulong makerOffset = ifdDataHash.value(37500).tagValue + startOffset + 12;
        readIFD("IFD Fuji Maker Note", makerOffset);
    }

    if (report) reportMetadata();
    return true;
}

bool Metadata::formatDNG()
{
    //file.open happens in readMetadata

    // set arbitrary order
    order = 0x4D4D;
    ulong startOffset = 0;

    // first two bytes is the endian order
    order = get2(file.read(2));
    if (order != 0x4D4D && order != 0x4949) return false;

    // should be magic number 42 next
    if (get2(file.read(2)) != 42) return false;

    // read offset to first IFD
    ulong ifdOffset = get4(file.read(4));
    ulong nextIFDOffset = readIFD("IFD0", ifdOffset);
    nextIFDOffset = 0;  // suppress compiler warning

    lengthFullJPG = 1;  // set arbitrary length to avoid error msg as tif do not
                         // have full size embedded jpg

    // IFD0: *******************************************************************

    // IFD0: Model
    (ifdDataHash.contains(272))
        ? model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount)
        : model = "";

    // IFD0: Make
    (ifdDataHash.contains(271))
        ? make = getString(ifdDataHash.value(271).tagValue + startOffset,
        ifdDataHash.value(271).tagCount)
        : make = "";

    // IFD0: Title (ImageDescription)
    (ifdDataHash.contains(270))
        ? title = getString(ifdDataHash.value(315).tagValue + startOffset,
        ifdDataHash.value(270).tagCount)
        : title = "";

    // IFD0: Creator (artist)
    (ifdDataHash.contains(315))
        ? creator = getString(ifdDataHash.value(315).tagValue + startOffset,
        ifdDataHash.value(315).tagCount)
        : creator = "";

    // IFD0: Copyright
    (ifdDataHash.contains(33432))
            ? copyright = getString(ifdDataHash.value(33432).tagValue + startOffset,
                                  ifdDataHash.value(33432).tagCount)
            : copyright = "";

    // IFD0: width
    (ifdDataHash.contains(256))
        ? width = ifdDataHash.value(256).tagValue
        : width = 0;

    // IFD0: height
    (ifdDataHash.contains(257))
        ? height = ifdDataHash.value(257).tagValue
        : height = 0;

    if (!width || !height) getDimensions(0);

    // IFD0: EXIF offset
    ulong ifdEXIFOffset = 0;
    if (ifdDataHash.contains(34665))
        ifdEXIFOffset = ifdDataHash.value(34665).tagValue;

    // IFD0: Photoshop offset
    ulong ifdPhotoshopOffset = 0;
    if (ifdDataHash.contains(34377))
        ifdPhotoshopOffset = ifdDataHash.value(34377).tagValue;

    // IFD0: IPTC offset
    ulong ifdIPTCOffset = 0;
    if (ifdDataHash.contains(33723))
        ifdIPTCOffset = ifdDataHash.value(33723).tagValue;

    // IFD0: XMP offset
    ulong ifdXMPOffset = 0;
    if (ifdDataHash.contains(700)) {
        isXmp = true;
        ifdXMPOffset = ifdDataHash.value(700).tagValue;
        xmpSegmentOffset = ifdXMPOffset;
        int xmpSegmentLength = ifdDataHash.value(700).tagCount;
        xmpNextSegmentOffset = xmpSegmentOffset + xmpSegmentLength;
    }

    // SubIFDs: ****************************************************************
    /* The DNG subIFDs each contain info for the embedded previews.  We record each
       one and then determine which ones are JPG and assign the smallest as a thumb
       and tghe largest as the full size preview JPG.
       */
    struct JpgInfo {
        ulong width;
        ulong height;
        ulong offset;
        ulong length;
    } jpgInfo;

    QList<JpgInfo> jpgs;
    QList<ulong> ifdOffsets;
    if(ifdDataHash.contains(330)) {
        if (ifdDataHash.value(330).tagCount > 1)
            ifdOffsets = getSubIfdOffsets(ifdDataHash.value(330).tagValue,
                                      ifdDataHash.value(330).tagCount);
        else ifdOffsets.append(ifdDataHash.value(330).tagValue);

        QString hdr;
        int count = 0;
        ulong smallest = 999999;
        ulong largest = 0;
        int smallJpg = 0;
        int largeJpg = 0;

        // iterate subIFDs looking for embedded JPGs
        for(int i = 0; i < ifdOffsets.length(); ++i) {
            hdr = "SubIFD" + QString::number(i + 1);
            readIFD(hdr, ifdOffsets[i]);
            // look for embedded image offset
            if (ifdDataHash.contains(273)) {
                // is it a JPG
                ulong offset = ifdDataHash.value(273).tagValue;
                file.seek(offset);
//                ulong x = get2(file.read(2));
                if (get2(file.read(2)) != 0xD8FF) break;  // order = 4949 so reverse
                // yes it is a JPG
                jpgInfo.offset = offset;
                jpgInfo.length = ifdDataHash.value(279).tagValue;;
                jpgInfo.width = ifdDataHash.value(256).tagValue;
                jpgInfo.height = ifdDataHash.value(257).tagValue;
                jpgs.append(jpgInfo);
                // find smallest and largest
                if (jpgInfo.width < smallest) {
                    smallest = jpgInfo.width;
                    smallJpg = count;
                }
                if (jpgInfo.width > largest) {
                    largest = jpgInfo.width;
                    largeJpg = count;
                }
                count++;
            }
        }
        if (jpgs.length() > 0) {
            width = jpgs.at(largeJpg).width;
            height = jpgs.at(largeJpg).height;
            offsetFullJPG = jpgs.at(largeJpg).offset;
            lengthFullJPG = jpgs.at(largeJpg).length;
            offsetSmallJPG = jpgs.at(smallJpg).offset;
            lengthSmallJPG = jpgs.at(smallJpg).length;
        }
    }


    // EXIF: *******************************************************************

    if (ifdEXIFOffset) readIFD("IFD Exif", ifdEXIFOffset);

    // EXIF: created datetime
    QString createdExif;
    (ifdDataHash.contains(36868))
        ? createdExif = getString(ifdDataHash.value(36868).tagValue,
        ifdDataHash.value(36868).tagCount)
        : createdExif = "";
    if (createdExif.length() > 0) createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // EXIF: shutter speed
    if (ifdDataHash.contains(33434)) {
        float x = getReal(ifdDataHash.value(33434).tagValue);
        if (x < 1 ) {
            uint t = qRound(1 / x);
            exposureTime = "1/" + QString::number(t);
            exposureTimeNum = x;
        } else {
            uint t = (uint)x;
            exposureTime = QString::number(t);
            exposureTimeNum = t;
        }
        exposureTime += " sec";
    } else {
        exposureTime = "";
    }

    // EXIF: aperture
    if (ifdDataHash.contains(33437)) {
        float x = getReal(ifdDataHash.value(33437).tagValue);
        aperture = "f/" + QString::number(x, 'f', 1);
        apertureNum = qRound(x * 10) / 10.0;
    } else {
        aperture = "";
        apertureNum = 0;
    }

    // EXIF: ISO
    if (ifdDataHash.contains(34855)) {
        ulong x = ifdDataHash.value(34855).tagValue;
        ISONum = static_cast<int>(x);
        ISO = QString::number(ISONum);
    } else {
        ISO = "";
        ISONum = 0;
    }

    // EXIF: focal length
    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue);
        focalLengthNum = static_cast<int>(x);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "";
        focalLengthNum = 0;
    }

    // EXIF: lens model
    (ifdDataHash.contains(42036))
            ? lens = getString(ifdDataHash.value(42036).tagValue,
                                  ifdDataHash.value(42036).tagCount)
            : lens = "";

    // Photoshop: **************************************************************
    // Get embedded JPG if available

    foundTifThumb = false;
    if (ifdPhotoshopOffset) readIRB(ifdPhotoshopOffset);

    // IPTC: *******************************************************************
    // Get title if available

    if (ifdIPTCOffset) readIPTC(ifdIPTCOffset);

    // read XMP - no XMP in fuji raw files
    if (isXmp && okToReadXmp) {
        Xmp xmp(file, xmpSegmentOffset, xmpNextSegmentOffset);
        rating = xmp.getItem("Rating");     // case is important "Rating"
        label = xmp.getItem("Label");       // case is important "Label"
        title = xmp.getItem("title");       // case is important "title"
        cameraSN = xmp.getItem("SerialNumber");
        if (lens.isEmpty()) lens = xmp.getItem("Lens");
        lensSN = xmp.getItem("LensSerialNumber");
        if (creator.isEmpty()) creator = xmp.getItem("creator");
        copyright = xmp.getItem("rights");
        email = xmp.getItem("CiEmailWork");
        url = xmp.getItem("CiUrlWork");

        // save original values so can determine if edited when writing changes
        _title = title;
        _rating = rating;
        _label = label;

        if (report) xmpString = xmp.metaAsString();
    }

    if (report) reportMetadata();

    return true;
}

bool Metadata::formatTIF()
{
    //file.open happens in readMetadata

    // set arbitrary order
    order = 0x4D4D;
    ulong startOffset = 0;

    // first two bytes is the endian order
    order = get2(file.read(2));
    if (order != 0x4D4D && order != 0x4949) return false;

    // should be magic number 42 next
    if (get2(file.read(2)) != 42) return false;

    // read offset to first IFD
    ulong ifdOffset = get4(file.read(4));
    ulong nextIFDOffset = readIFD("IFD0", ifdOffset);
    nextIFDOffset = 0;  // suppress compiler warning

    lengthFullJPG = 1;  // set arbitrary length to avoid error msg as tif do not
                         // have full size embedded jpg

    // IFD0: *******************************************************************

    // IFD0: Model
    (ifdDataHash.contains(272))
        ? model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount)
        : model = "";

    // IFD0: Make
    (ifdDataHash.contains(271))
        ? make = getString(ifdDataHash.value(271).tagValue + startOffset,
        ifdDataHash.value(271).tagCount)
        : make = "";

    // IFD0: Title (ImageDescription)
    (ifdDataHash.contains(270))
        ? title = getString(ifdDataHash.value(315).tagValue + startOffset,
        ifdDataHash.value(270).tagCount)
        : title = "";

    // IFD0: Creator (artist)
    (ifdDataHash.contains(315))
        ? creator = getString(ifdDataHash.value(315).tagValue + startOffset,
        ifdDataHash.value(315).tagCount)
        : creator = "";

    // IFD0: Copyright
    (ifdDataHash.contains(33432))
            ? copyright = getString(ifdDataHash.value(33432).tagValue + startOffset,
                                  ifdDataHash.value(33432).tagCount)
            : copyright = "";

    // IFD0: width
    (ifdDataHash.contains(256))
        ? width = ifdDataHash.value(256).tagValue
        : width = 0;

    // IFD0: height
    (ifdDataHash.contains(257))
        ? height = ifdDataHash.value(257).tagValue
        : height = 0;

    if (!width || !height) getDimensions(0);

    // IFD0: EXIF offset
    ulong ifdEXIFOffset = 0;
    if (ifdDataHash.contains(34665))
        ifdEXIFOffset = ifdDataHash.value(34665).tagValue;

    // IFD0: Photoshop offset
    ulong ifdPhotoshopOffset = 0;
    if (ifdDataHash.contains(34377))
        ifdPhotoshopOffset = ifdDataHash.value(34377).tagValue;

    // IFD0: IPTC offset
    ulong ifdIPTCOffset = 0;
    if (ifdDataHash.contains(33723))
        ifdIPTCOffset = ifdDataHash.value(33723).tagValue;

    // IFD0: XMP offset
    ulong ifdXMPOffset = 0;
    if (ifdDataHash.contains(700)) {
        ifdXMPOffset = ifdDataHash.value(700).tagValue;
        xmpSegmentOffset = ifdXMPOffset;
        int xmpSegmentLength = ifdDataHash.value(700).tagCount;
        xmpNextSegmentOffset = xmpSegmentOffset + xmpSegmentLength;
    }

    // EXIF: *******************************************************************

    if (ifdEXIFOffset) readIFD("IFD Exif", ifdEXIFOffset);

    // EXIF: created datetime
    QString createdExif;
    (ifdDataHash.contains(36868))
        ? createdExif = getString(ifdDataHash.value(36868).tagValue,
        ifdDataHash.value(36868).tagCount)
        : createdExif = "";
    if (createdExif.length() > 0) createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // EXIF: shutter speed
    if (ifdDataHash.contains(33434)) {
        float x = getReal(ifdDataHash.value(33434).tagValue);
        if (x < 1 ) {
            uint t = qRound(1 / x);
            exposureTime = "1/" + QString::number(t);
            exposureTimeNum = x;
        } else {
            uint t = (uint)x;
            exposureTime = QString::number(t);
            exposureTimeNum = t;
        }
        exposureTime += " sec";
    } else {
        exposureTime = "";
    }

    // EXIF: aperture
    if (ifdDataHash.contains(33437)) {
        float x = getReal(ifdDataHash.value(33437).tagValue);
        aperture = "f/" + QString::number(x, 'f', 1);
        apertureNum = qRound(x * 10) / 10.0;
    } else {
        aperture = "";
        apertureNum = 0;
    }

    // EXIF: ISO
    if (ifdDataHash.contains(34855)) {
        ulong x = ifdDataHash.value(34855).tagValue;
        ISONum = static_cast<int>(x);
        ISO = QString::number(ISONum);
    } else {
        ISO = "";
        ISONum = 0;
    }

    // EXIF: focal length
    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue);
        focalLengthNum = static_cast<int>(x);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "";
        focalLengthNum = 0;
    }

    // EXIF: lens model
    (ifdDataHash.contains(42036))
            ? lens = getString(ifdDataHash.value(42036).tagValue,
                                  ifdDataHash.value(42036).tagCount)
            : lens = "";

    // Photoshop: **************************************************************
    // Get embedded JPG if available

    foundTifThumb = false;
    if (ifdPhotoshopOffset) readIRB(ifdPhotoshopOffset);

    // IPTC: *******************************************************************
    // Get title if available

    if (ifdIPTCOffset) readIPTC(ifdIPTCOffset);

    // read XMP - no XMP in fuji raw files
    if (isXmp && okToReadXmp) {
        Xmp xmp(file, xmpSegmentOffset, xmpNextSegmentOffset);
        rating = xmp.getItem("Rating");     // case is important "Rating"
        label = xmp.getItem("Label");       // case is important "Label"
        title = xmp.getItem("title");       // case is important "title"
        cameraSN = xmp.getItem("SerialNumber");
        if (lens.isEmpty()) lens = xmp.getItem("Lens");
        lensSN = xmp.getItem("LensSerialNumber");
        if (creator.isEmpty()) creator = xmp.getItem("creator");
        copyright = xmp.getItem("rights");
        email = xmp.getItem("CiEmailWork");
        url = xmp.getItem("CiUrlWork");

        // save original values so can determine if edited when writing changes
        _title = title;
        _rating = rating;
        _label = label;

        if (report) xmpString = xmp.metaAsString();
    }

    if (report) reportMetadata();

    return true;
}

bool Metadata::formatPanasonic()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    /* Panasonic RW2 files

      Note TIFF magic word is 0x55 instead of 0x2a.

    Offset          Bytes   Description
    0000 - 0004     4      Endian
    0005 - 0008     4      Offset to IFD0   // 18 from start of file

    Panasonic starts with an single unchained IFD, which included offsets for an Exif IFD
    and an XMP segment.  It also has an offset to the embedded Jpg.  The Jpg has its own
    IFD0 and IFD1.  The IFD0 has an offset to another Exit IFD which, in turn, has an offset
    to the maker notes IFD.
    */
    //file.open in readMetadata
    // get endian
    order = get2(file.read(4));
    // get offset to first IFD and read it
    ulong offsetIfd0 = get4(file.read(4));

    readIFD("IFD0", offsetIfd0);

    // pull data reqd from main file IFD0
    make = getString(ifdDataHash.value(271).tagValue, ifdDataHash.value(271).tagCount).trimmed();
    model = getString(ifdDataHash.value(272).tagValue, ifdDataHash.value(272).tagCount).trimmed();
    orientation = ifdDataHash.value(274).tagValue;
    creator = getString(ifdDataHash.value(315).tagValue, ifdDataHash.value(315).tagCount);
    copyright = getString(ifdDataHash.value(33432).tagValue, ifdDataHash.value(33432).tagCount);
    offsetFullJPG = ifdDataHash.value(46).tagValue;
    lengthFullJPG = ifdDataHash.value(46).tagCount;
    xmpSegmentOffset = ifdDataHash.value(700).tagValue;
    xmpNextSegmentOffset = ifdDataHash.value(700).tagCount + xmpSegmentOffset;
    height = ifdDataHash.value(49).tagValue;
    width = ifdDataHash.value(50).tagValue;
    if (ifdDataHash.contains(23)) {
        ulong x = ifdDataHash.value(23).tagValue;
        ISONum = static_cast<int>(x);
        ISO = QString::number(ISONum);
    } else {
        ISO = "";
        ISONum = 0;
    }

    // get the offset for ExifIFD and read it
    ulong offsetEXIF;
    offsetEXIF = ifdDataHash.value(34665).tagValue;
    readIFD("IFD Exif", offsetEXIF);

    // EXIF: created datetime
    QString createdExif;
    createdExif = getString(ifdDataHash.value(36868).tagValue,
        ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // shutter speed
    if (ifdDataHash.contains(33434)) {
        float x = getReal(ifdDataHash.value(33434).tagValue);
        if (x <1 ) {
            uint t = qRound(1/x);
            exposureTime = "1/" + QString::number(t);
            exposureTimeNum = x;
        } else {
            uint t = (uint)x;
            exposureTime = QString::number(t);
            exposureTimeNum = t;
        }
        exposureTime += " sec";
    } else {
        exposureTime = "";
    }
    // aperture
    if (ifdDataHash.contains(33437)) {
        float x = getReal(ifdDataHash.value(33437).tagValue);
        aperture = "f/" + QString::number(x, 'f', 1);
        apertureNum = qRound(x * 10) / 10.0;
    } else {
        aperture = "";
        apertureNum = 0;
    }
    // focal length
    if (ifdDataHash.contains(37386)) {
        float x = getReal(ifdDataHash.value(37386).tagValue);
        focalLengthNum = static_cast<int>(x);
        focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        focalLength = "";
        focalLengthNum = 0;
    }

    // check embedded JPG for more metadata (IFD0, IFD1, Exit IFD and Maker notes IFD)
    order = 0x4D4D;
    ulong startOffset = offsetFullJPG;
    file.seek(offsetFullJPG);

    if (get2(file.read(2)) != 0xFFD8) return 0;

    // build a hash of jpg segment offsets
    getSegments(file.pos());

    // read the embedded JPG EXIF data
    if (segmentHash.contains("EXIF")) {
        file.seek(segmentHash["EXIF"]);
        bool foundEndian = false;
        int counter = 0;
        while (!foundEndian) {
            ulong a = get2(file.read(2));
            if (a == 0x4949 || a == 0x4D4D) {
                order = a;
                // offsets are from the endian position in JPEGs
                // therefore must adjust all offsets found in tagValue
                startOffset = file.pos() - 2;
                foundEndian = true;
            }
            // break out if not finding endian
            if(++counter > 30) break;
        }

        if (report) rpt << "\n startOffset = " << startOffset;

        uint a = get2(file.read(2));  // magic 42
        a = get4(file.read(4));
        ulong offsetIfd0 = a + startOffset;

        // read JPG IFD0
        ulong nextIFDOffset = readIFD("IFD0", offsetIfd0) + startOffset;

        offsetEXIF = ifdDataHash.value(34665).tagValue + startOffset;

        // read JPG IFD1
        if (nextIFDOffset) nextIFDOffset = readIFD("IFD1", nextIFDOffset);

        offsetThumbJPG = ifdDataHash.value(513).tagValue + startOffset;
        lengthThumbJPG = ifdDataHash.value(514).tagValue;

        // read JPG Exif IFD
        readIFD("IFD Exif", offsetEXIF);

        // maker note
        if (ifdDataHash.contains(37500)) {
            // get offset from the IFD EXIF in embedded JPG
//            ulong makerOffset = 5948;
            ulong makerOffset = ifdDataHash.value(37500).tagValue;
            readIFD("IFD Panasonic Maker Note", makerOffset + startOffset + 12);
            // get lens
            lens = getString(ifdDataHash.value(81).tagValue + startOffset, ifdDataHash.value(81).tagCount);
            // get lens serial number
            lensSN = getString(ifdDataHash.value(82).tagValue + startOffset, ifdDataHash.value(82).tagCount);
            // get camera serial number
            cameraSN = getString(ifdDataHash.value(37).tagValue + startOffset, ifdDataHash.value(37).tagCount);
        }
    }

    isXmp = true;       // Panasonic has an xmp segment

    // read XMP
    if (isXmp && okToReadXmp && xmpSegmentOffset > 0 && xmpNextSegmentOffset > 0) {
        Xmp xmp(file, xmpSegmentOffset, xmpNextSegmentOffset);
        rating = xmp.getItem("Rating");     // case is important "Rating"
        label = xmp.getItem("Label");       // case is important "Label"
        if (title.isEmpty()) title = xmp.getItem("title");       // case is important "title"
        if (cameraSN.isEmpty()) cameraSN = xmp.getItem("SerialNumber");
        if (lens.isEmpty()) lens = xmp.getItem("Lens");
        if (lensSN.isEmpty()) lensSN = xmp.getItem("LensSerialNumber");
        if (creator.isEmpty()) creator = xmp.getItem("creator");
        if (copyright.isEmpty()) copyright = xmp.getItem("rights");
        if (email.isEmpty()) email = xmp.getItem("CiEmailWork");
        if (url.isEmpty()) url = xmp.getItem("CiUrlWork");

        // save original values so can determine if edited when writing changes
        _title = title;
        _rating = rating;
        _label = label;

        if (report) xmpString = xmp.metaAsString();
    }

    if (report) reportMetadata();
    return true;
}

bool Metadata::formatJPG()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    //file.open happens in readMetadata
    order = 0x4D4D;
    ulong startOffset = 0;
    if (get2(file.read(2)) != 0xFFD8) return 0;

    // build a hash of jpg segment offsets
    getSegments(file.pos());

    // check if JFIF
    if (segmentHash.contains("JFIF")) {
        // it's a jpg so the whole thing is the full length jpg and no other
        // metadata available
        offsetFullJPG = 0;
        lengthFullJPG = file.size();
        return true;
    }

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

    if (report) rpt << "\n startOffset = " << startOffset;

    uint a = get2(file.read(2));  // magic 42
    a = get4(file.read(4));
    ulong offsetIfd0 = a + startOffset;

    // it's a jpg so the whole thing is the full length jpg
    offsetFullJPG = 0;
    lengthFullJPG = file.size();

    // read IFD0
    ulong nextIFDOffset = readIFD("IFD0", offsetIfd0) + startOffset;
    ulong offsetEXIF;
    offsetEXIF = ifdDataHash.value(34665).tagValue + startOffset;
    orientation = ifdDataHash.value(274).tagValue;

    if (readNonEssentialMetadata) {
        make = getString(ifdDataHash.value(271).tagValue + startOffset,
                         ifdDataHash.value(271).tagCount);
        model = getString(ifdDataHash.value(272).tagValue + startOffset,
                          ifdDataHash.value(272).tagCount);
        creator = getString(ifdDataHash.value(315).tagValue + startOffset,
                            ifdDataHash.value(315).tagCount);
        copyright = getString(ifdDataHash.value(33432).tagValue + startOffset,
                              ifdDataHash.value(33432).tagCount);
    }

    // read IFD1
    if (nextIFDOffset) nextIFDOffset = readIFD("IFD1", nextIFDOffset);

    if (readEssentialMetadata) {
        // IFD1: thumbnail offset and length
        offsetThumbJPG = ifdDataHash.value(513).tagValue + 12;
        lengthThumbJPG = ifdDataHash.value(514).tagValue;
    }

    // read EXIF
    readIFD("IFD Exif", offsetEXIF);

    if (readEssentialMetadata) {
        width = ifdDataHash.value(40962).tagValue;
        height = ifdDataHash.value(40963).tagValue;
        if (!width || !height) getDimensions(0);
    }

    if (readNonEssentialMetadata) {

        // EXIF: created datetime
        QString createdExif;
        createdExif = getString(ifdDataHash.value(36868).tagValue + startOffset,
            ifdDataHash.value(36868).tagCount);
        if (createdExif.length() > 0) createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
        // try DateTimeOriginal
        if (createdExif.length() == 0) {
            createdExif = getString(ifdDataHash.value(36867).tagValue + startOffset,
                ifdDataHash.value(36867).tagCount);
            if (createdExif.length() > 0) createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
        }

        // EXIF: shutter speed
        if (ifdDataHash.contains(33434)) {
            float x = getReal(ifdDataHash.value(33434).tagValue + startOffset);
            if (x < 1 ) {
                uint t = qRound(1 / x);
                exposureTime = "1/" + QString::number(t);
                exposureTimeNum = x;
            } else {
                uint t = (uint)x;
                exposureTime = QString::number(t);
                exposureTimeNum = t;
            }
            exposureTime += " sec";
        } else {
            exposureTime = "";
        }

        // EXIF: aperture
        if (ifdDataHash.contains(33437)) {
            float x = getReal(ifdDataHash.value(33437).tagValue + startOffset);
            aperture = "f/" + QString::number(x, 'f', 1);
            apertureNum = qRound(x * 10) / 10.0;
        } else {
            aperture = "";
            apertureNum = 0;
        }

        // EXIF: ISO
        if (ifdDataHash.contains(34855)) {
            ulong x = ifdDataHash.value(34855).tagValue;
            ISONum = static_cast<int>(x);
            ISO = QString::number(ISONum);
        } else {
            ISO = "";
            ISONum = 0;
        }

        // EXIF: focal length
        if (ifdDataHash.contains(37386)) {
            float x = getReal(ifdDataHash.value(37386).tagValue + startOffset);
            focalLengthNum = static_cast<int>(x);
            focalLength = QString::number(x, 'f', 0) + "mm";
        } else {
            focalLength = "";
            focalLengthNum = 0;
        }

        // EXIF: lens model
        lens = getString(ifdDataHash.value(42036).tagValue + startOffset,
                         ifdDataHash.value(42036).tagCount);
    }

    // read IPTC
    if (readNonEssentialMetadata)
        if (segmentHash.contains("IPTC")) readIPTC(segmentHash["IPTC"]);

    // read XMP
    if (isXmp && okToReadXmp) {
        Xmp xmp(file, xmpSegmentOffset, xmpNextSegmentOffset);
        rating = xmp.getItem("Rating");     // case is important "Rating"
        label = xmp.getItem("Label");       // case is important "Label"
        title = xmp.getItem("title");       // case is important "title"
        cameraSN = xmp.getItem("SerialNumber");
        if (lens.isEmpty()) lens = xmp.getItem("Lens");
        lensSN = xmp.getItem("LensSerialNumber");
        if (creator.isEmpty()) creator = xmp.getItem("creator");
        copyright = xmp.getItem("rights");
        email = xmp.getItem("CiEmailWork");
        url = xmp.getItem("CiUrlWork");

        // save original values so can determine if edited when writing changes
        _title = title;
        _rating = rating;
        _label = label;

        if (report) xmpString = xmp.metaAsString();
    }

    if (report) reportMetadata();
    return true;
}

void Metadata::clearMetadata()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    #endif
    }
    offsetFullJPG = 0;
    lengthFullJPG = 0;
    offsetThumbJPG = 0;
    lengthThumbJPG = 0;
    offsetSmallJPG = 0;
    lengthSmallJPG = 0;
    xmpSegmentOffset = 0;
    orientationOffset = 0;
    width = 0;
    height = 0;
    orientation = 1;
//    created = "";
    model = "";
    exposureTime = "";
    exposureTimeNum = 0;
    aperture = "";
    apertureNum = 0;
    ISO = "";
    ISONum = 0;
    focalLength = "";
    focalLengthNum = 0;
    title = "";
    lens = "";
    creator = "";
    copyright = "";
    email = "";
    url = "";
    err = "";
    shutterCount = 0;
    cameraSN = "";
    lensSN = "";
    rating = "";
    label = "";

    ifdDataHash.clear();
    nikonLensCode = "";
}

void Metadata::testNewFileFormat(const QString &path)
{
    report = true;

    if (report) {
        rpt.flush();
        reportString = "";
        rpt.setString(&reportString);
        rpt << "\nFile name = " << path << "\n";
    }
    clearMetadata();
    file.setFileName(path);
    file.open(QIODevice::ReadOnly);

    // edit test format to use:
    formatDNG();
//    reportMetadata();
}

bool Metadata::readMetadata(bool isReport, const QString &path)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    report = isReport;

    if (report) {
        rpt.flush();
        reportString = "";
        rpt.setString(&reportString);
    }
    clearMetadata();
    file.setFileName(path);

    if (report) {
        rpt << "\nFile name = " << path << "\n";
    }
    QFileInfo fileInfo(path);
    QString ext = fileInfo.completeSuffix().toLower();
//    if (G::isThreadTrackingOn) track(fPath, "Reading ");
    bool success = false;
    int totDelay = 50;
    int msDelay = 0;
    int msInc = 1;
    bool fileOpened = false;
    do {
        if (file.open(QIODevice::ReadOnly)) {
            if (ext == "cr2") formatCanon();
            if (ext == "dng") formatDNG();
            if (ext == "raf") formatFuji();
            if (ext == "jpg") formatJPG();
            if (ext == "nef") formatNikon();
            if (ext == "orf") formatOlympus();
            if (ext == "rw2") formatPanasonic();
            if (ext == "arw") formatSony();
            if (ext == "tif") formatTIF();
            fileOpened = true;
            file.close();
            success = true;
        }
        else {
            err = "Could not open file to read metadata";    // try again
            QThread::msleep(msInc);
            msDelay += msInc;
//            if (G::isThreadTrackingOn) track(fPath, err);
        }
    }
    while ((msDelay < totDelay) && !success);

    // not all files have thumb or small jpg embedded
    if (offsetFullJPG == 0 && ext != "jpg" && fileOpened) {
        err = "No embedded JPG found";
//        qDebug() << G::t.restart() << "\t" << "No embedded JPG found for" << fPath;
//        if (G::isThreadTrackingOn) track(fPath, err);
    }

    if (lengthFullJPG == 0) {
        offsetFullJPG = offsetSmallJPG;
        lengthFullJPG = lengthSmallJPG;
    }
    if (lengthSmallJPG == 0) {
        offsetSmallJPG = offsetFullJPG;
        lengthSmallJPG = lengthFullJPG;
    }
    if (lengthThumbJPG == 0) {
        offsetThumbJPG = offsetSmallJPG;
        lengthThumbJPG = lengthSmallJPG;
    }

    // error flags
    thumbUnavailable = imageUnavailable = false;
    if (lengthFullJPG == 0) {
        imageUnavailable = true;
        err = "No embedded preview found or file is corrupted";
    }
    if (lengthThumbJPG == 0) {
        thumbUnavailable = true;
        err = "No embedded thumbnail or preview found or file is corrupted";
    }

    // initialize edited rotation
    rotationDegrees = 0;

    if (success) track(fPath, "Success");
    else {
        track(fPath, "FAILED TO LOAD METADATA");
        qDebug() << G::t.restart() << "\t" << "FAILED TO LOAD METADATA" << fPath;
    }

    #ifdef ISPROFILE
    qDebug() << G::t.restart() << "\t" << "=> Metadata::readMetadata: End" << G::t.nsecsElapsed() << "\t\t\t" << fPath;  G::t.start();
    #endif
    return success;
}

void Metadata::removeImage(QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metaCache.remove(imageFileName);
}

bool Metadata::isLoaded(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].metadataLoaded;
}

bool Metadata::isThumbLoaded(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].isThumbLoaded;
}

ulong Metadata::getOffsetFullJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].offsetFullJPG;
}

ulong Metadata::getLengthFullJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].lengthFullJPG;
}

ulong Metadata::getOffsetThumbJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    #endif
    }
    return metaCache[imageFileName].offsetThumbJPG;
}

ulong Metadata::getLengthThumbJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].lengthThumbJPG;
}

ulong Metadata::getOffsetSmallJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].offsetSmallJPG;
}

ulong Metadata::getLengthSmallJPG(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].lengthSmallJPG;
}

ulong Metadata::getWidth(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].width;
}

ulong Metadata::getHeight(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].height;
}

QString Metadata::getDimensions(const QString &imageFileName)
{
    {
#ifdef ISDEBUG
G::track(__FUNCTION__);
#endif
    }
    return metaCache[imageFileName].dimensions;
}

QDateTime Metadata::getCreatedDate(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].createdDate;
}

//QString Metadata::getCreated(const QString &imageFileName)
//{
//    {
//    #ifdef ISDEBUG
//    qDebug() << G::t.restart() << "\t" << "Metadata::getCreated" << imageFileName;
//    #endif
//    }
//    return "";
////    return metaCache[imageFileName].created;
//}

int Metadata::getYear(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].year;
}

int Metadata::getMonth(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].month;
}

int Metadata::getDay(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].day;
}

QString Metadata::getCopyFileNamePrefix(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].copyFileNamePrefix;
}

QString Metadata::getMake(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].make;
}

QString Metadata::getModel(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].model;
}

QString Metadata::getExposureTime(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].exposureTime;
}

float Metadata::getExposureTimeNum(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].exposureTimeNum;
}

QString Metadata::getAperture(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].aperture;
}

qreal Metadata::getApertureNum(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].apertureNum;
}

QString Metadata::getISO(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].ISO;
}

int Metadata::getISONum(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].ISONum;
}

QString Metadata::getFocalLength(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].focalLength;
}

int Metadata::getFocalLengthNum(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].focalLengthNum;
}

QString Metadata::getTitle(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].title;
}

QString Metadata::getRating(const QString &imageFileName)
{
    return metaCache[imageFileName].rating;
}

QString Metadata::getLabel(const QString &imageFileName)
{
    return metaCache[imageFileName].label;
}

QString Metadata::getLens(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].lens;
}

QString Metadata::getCreator(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].creator;
}

QString Metadata::getCopyright(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].copyright;
}

QString Metadata::getEmail(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].email;
}

QString Metadata::getUrl(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].url;
}

QString Metadata::getShootingInfo(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].shootingInfo;
}

void Metadata::setTitle(const QString &imageFileName, const QString &title)
{
    metaCache[imageFileName].title = title;
}

void Metadata::setCreator(const QString &imageFileName, const QString &creator)
{
    metaCache[imageFileName].creator = creator;
}

void Metadata::setCopyright(const QString &imageFileName, const QString &copyright)
{
    metaCache[imageFileName].copyright = copyright;
}

void Metadata::setEmail(const QString &imageFileName, const QString &email)
{
    metaCache[imageFileName].email = email;
}

void Metadata::setUrl(const QString &imageFileName, const QString &url)
{
    metaCache[imageFileName].url = url;
}

void Metadata::setLabel(const QString &imageFileName, const QString &label)
{
    metaCache[imageFileName].label = label;
}

void Metadata::setRating(const QString &imageFileName, const QString &rating)
{
    metaCache[imageFileName].rating = rating;
}

bool Metadata::getThumbUnavailable(const QString &imageFileName)
{
    return metaCache[imageFileName].thumbUnavailable;
}

bool Metadata::getImageUnavailable(const QString &imageFileName)
{
    return metaCache[imageFileName].imageUnavailable;
}

QString Metadata::getErr(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].err;
}

void Metadata::setErr(const QString &imageFileName, const QString &err)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metaCache[imageFileName].err = err;
}

int Metadata::getOrientation(QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    }
    if (metaCache.contains(imageFileName)) {
        return metaCache[imageFileName].orientation;
    }
    else return 0;
}

int Metadata::getRotation(QString &imageFileName)
{
    {
#ifdef ISDEBUG
G::track(__FUNCTION__);
#endif
    }
    if (metaCache.contains(imageFileName)) {
        return metaCache[imageFileName].rotationDegrees;
    }
    else return 0;
}

void Metadata::setRotation(const QString &imageFileName, const int rotationDegrees)
{
    metaCache[imageFileName].rotationDegrees = rotationDegrees;
}

bool Metadata::getPick(const QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    return metaCache[imageFileName].isPicked;
}

void Metadata::setPick(const QString &imageFileName, bool choice)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metaCache[imageFileName].isPicked = choice;
}

void Metadata::clear()
{
    /*  Clears the QMap that holds the ImageMetadata for each image in the
       datamodel.

       rgh - Look at eliminating this structure and just use datamodel.
    */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metaCache.clear();
}

void Metadata::loadFromThread(QFileInfo &fileInfo)
{
    loadImageMetadata(fileInfo, true, true, false, true);
}

bool Metadata::loadImageMetadata(const QFileInfo &fileInfo,
                                 bool essential, bool nonEssential,
                                 bool isReport, bool isLoadXmp)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // check if already loaded
    fPath = fileInfo.filePath();
    if (metaCache[fPath].metadataLoaded && !isReport) return true;

    // For JPG, readNonEssentialMetadata adds 10-15% time to load
    readEssentialMetadata = essential;
    readNonEssentialMetadata = nonEssential;
    okToReadXmp = isLoadXmp;
    okToReadXmp = true;

//    ImageMetadata imageMetadata;
    bool result = readMetadata(isReport, fileInfo.filePath());

//    if (fPath == "D:/Pictures/_ThumbTest/FujiXT2.RAF")
//        qDebug() << G::t.restart() << "\t" << "Lets break here";

    imageMetadata.isPicked = false;
    imageMetadata.offsetFullJPG = offsetFullJPG;
    imageMetadata.lengthFullJPG = lengthFullJPG;
    imageMetadata.offsetThumbJPG = offsetThumbJPG;
    imageMetadata.lengthThumbJPG = lengthThumbJPG;
    imageMetadata.offsetSmallJPG = offsetSmallJPG;
    imageMetadata.lengthSmallJPG = lengthSmallJPG;
    imageMetadata.xmpSegmentOffset = xmpSegmentOffset;
    imageMetadata.xmpNextSegmentOffset = xmpNextSegmentOffset;
    imageMetadata.isXmp = isXmp;
    imageMetadata.orientationOffset = orientationOffset;
    imageMetadata.width = width;
    imageMetadata.height = height;
    imageMetadata.dimensions = QString::number(width) + "x" + QString::number(height);
    imageMetadata.orientation = orientation;
    imageMetadata.rotationDegrees = rotationDegrees;
    imageMetadata.createdDate = createdDate;
    imageMetadata.model = model.trimmed();
    imageMetadata.exposureTime = exposureTime;
    imageMetadata.exposureTimeNum = exposureTimeNum;
    imageMetadata.aperture = aperture;
    imageMetadata.apertureNum = apertureNum;
    imageMetadata.ISO = ISO;
    imageMetadata.ISONum = ISONum;
    imageMetadata.focalLength = focalLength;
    imageMetadata.focalLengthNum = focalLengthNum;
    imageMetadata.title = title;
    imageMetadata._title = _title;
    imageMetadata.rating = rating;
    imageMetadata._rating = _rating;
    imageMetadata.label = label;
    imageMetadata._label = _label;
    imageMetadata.lens = lens;
    imageMetadata.creator = creator.trimmed();
    imageMetadata._creator = _creator;
    imageMetadata.copyright = copyright;
    imageMetadata._copyright = _copyright;
    imageMetadata.email = email;
    imageMetadata._email = _email;
    imageMetadata.url = url;
    imageMetadata._url = _url;
    imageMetadata.copyFileNamePrefix = createdDate.toString("yyyy-MM-dd");

    QString s = imageMetadata.model;
    s += "  " + imageMetadata.focalLength;
    s += "  " + imageMetadata.exposureTime;
    s += (imageMetadata.aperture == "") ? "" : " at " + imageMetadata.aperture;
    s += (imageMetadata.ISO == "") ? "" : ", ISO " + imageMetadata.ISO;
    imageMetadata.shootingInfo = s;

    imageMetadata.thumbUnavailable = thumbUnavailable;
    imageMetadata.imageUnavailable = imageUnavailable;
    imageMetadata.err = err;

    imageMetadata.metadataLoaded = result;

    metaCache.insert(fileInfo.filePath(), imageMetadata);

    return result;
}

void Metadata::setMetadata(const QString &imageFileName)
{
    isPicked =  metaCache[imageFileName].isPicked;
    offsetFullJPG = metaCache[imageFileName].offsetFullJPG;
    lengthFullJPG = metaCache[imageFileName].lengthFullJPG;
    offsetThumbJPG = metaCache[imageFileName].offsetThumbJPG;
    lengthThumbJPG = metaCache[imageFileName].lengthThumbJPG;
    offsetSmallJPG = metaCache[imageFileName].offsetSmallJPG;
    lengthSmallJPG = metaCache[imageFileName].lengthSmallJPG;
    xmpSegmentOffset = metaCache[imageFileName].xmpSegmentOffset;
    xmpNextSegmentOffset = metaCache[imageFileName].xmpNextSegmentOffset;
    isXmp = metaCache[imageFileName].isXmp;
    orientationOffset = metaCache[imageFileName].orientationOffset;
    width = metaCache[imageFileName].width;
    height = metaCache[imageFileName].height;
    dimensions = metaCache[imageFileName].dimensions;
    rotationDegrees = metaCache[imageFileName].rotationDegrees;
//    created = metaCache[imageFileName].created;
    createdDate = metaCache[imageFileName].createdDate;
    model = metaCache[imageFileName].model;
    exposureTime = metaCache[imageFileName].exposureTime;
    exposureTimeNum = metaCache[imageFileName].exposureTimeNum;
    aperture = metaCache[imageFileName].aperture;
    apertureNum = metaCache[imageFileName].apertureNum;
    ISO = metaCache[imageFileName].ISO;
    ISONum = metaCache[imageFileName].ISONum;
    focalLength = metaCache[imageFileName].focalLength;
    focalLengthNum = metaCache[imageFileName].focalLengthNum;
    title = metaCache[imageFileName].title;
    _title = metaCache[imageFileName]._title;
    rating = metaCache[imageFileName].rating;
    _rating = metaCache[imageFileName]._rating;
    label = metaCache[imageFileName].label;
    _label = metaCache[imageFileName]._label;
    lens = metaCache[imageFileName].lens;
    creator = metaCache[imageFileName].creator;
    _creator = metaCache[imageFileName]._creator;
    copyright = metaCache[imageFileName].copyright;
    _copyright = metaCache[imageFileName]._copyright;
    email = metaCache[imageFileName].email;
    _email = metaCache[imageFileName]._email;
    url = metaCache[imageFileName].url;
    _url = metaCache[imageFileName]._url;
    orientation = metaCache[imageFileName].orientation;
    shootingInfo = metaCache[imageFileName].shootingInfo;
}

// End Metadata
