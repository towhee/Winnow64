#include "nikon.h"

Nikon::Nikon()
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

    /* replaced with updated list
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
    */

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
    nikonLensHash["25483C5C24241B02"] = "AT-X 270 AF PRO II (AF 28-70mm f/2.6-2.8)";
    nikonLensHash["25483C5C24241B02"] = "AT-X 287 AF PRO SV (AF 28-70mm f/2.8)";
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
    nikonLensHash["2F4030442C342902"] = "AF 193 (AF 19-35mm f/3.5-4.5)";
    nikonLensHash["2F4030442C342902"] = "AF 235 II (AF 20-35mm f/3.5-4.5)";
    nikonLensHash["2F4030442C342902"] = "Tokina AF 235 II (AF 20-35mm f/3.5-4.5)";
    nikonLensHash["2F48304424242902"] = "AF Zoom-Nikkor 20-35mm f/2.8D IF";
    nikonLensHash["2F48304424242902"] = "AF Zoom-Nikkor 20-35mm f/2.8D IF";
    nikonLensHash["2F48304424242902"] = "AT-X 235 AF PRO (AF 20-35mm f/2.8)";
    nikonLensHash["3048989824242402"] = "AF-I Nikkor 400mm f/2.8D IF-ED";
    nikonLensHash["304898982424E102"] = "AF-I Nikkor 400mm f/2.8D IF-ED + TC-17E";
    nikonLensHash["304898982424F102"] = "AF-I Nikkor 400mm f/2.8D IF-ED + TC-14E";
    nikonLensHash["304898982424F202"] = "AF-I Nikkor 400mm f/2.8D IF-ED + TC-20E";
    nikonLensHash["3154565624242502"] = "AF Micro-Nikkor 60mm f/2.8D";
    nikonLensHash["3253646424243502"] = "Tamron SP AF 90mm f/2.8 [Di] Macro 1:1 (172E/272E)";
    nikonLensHash["3254505024243502"] = "Sigma Macro 50mm F2.8 EX DG";
    nikonLensHash["32546A6A24243502"] = "AF Micro-Nikkor 105mm f/2.8D";
    nikonLensHash["32546A6A24243502"] = "AF Micro-Nikkor 105mm f/2.8D";
    nikonLensHash["32546A6A24243502"] = "Macro 105mm F2.8 EX DG";
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
    nikonLensHash["4A4C24241E6C4D06"] = "14mm f/2.4 Premium";
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
    nikonLensHash["7A3C1F3730307E06"] = "AF-S DX Zoom-Nikkor 12-24mm f/4G IF-ED";
    nikonLensHash["7A3C1F3730307E06"] = "AT-X 124 AF PRO DX II (AF 12-24mm f/4)";
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
    nikonLensHash["92392D882C404B0E"] = "18-250mm F3.5-6.3 DC OS Macro HSM";
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
    nikonLensHash["983CA6A630309A00"] = "AF-S VR Nikkor 600mm f/4G ED VR";
    nikonLensHash["983CA6A630309A0E"] = "AF-S VR Nikkor 600mm f/4G ED";
    nikonLensHash["9848507624244B0E"] = "Sigma 50-150mm F2.8 EX APO DC OS HSM";
    nikonLensHash["994029622C3C9B0E"] = "AF-S DX VR Zoom-Nikkor 16-85mm f/3.5-5.6G ED";
    nikonLensHash["9948767624244B0E"] = "Sigma APO Macro 150mm F2.8 EX DG OS HSM";
    nikonLensHash["9A402D532C3C9C0E"] = "AF-S DX VR Zoom-Nikkor 18-55mm f/3.5-5.6G";
    nikonLensHash["9A4C505014149C06"] = "YN50mm F1.8N";
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
    nikonLensHash["A3385C8E3440CE0E"] = "AF-P DX Nikkor 70–300mm f/4.5–6.3G ED";
    nikonLensHash["A3385C8E3440CE8E"] = "AF-P DX Nikkor 70–300mm f/4.5–6.3G ED VR";
    nikonLensHash["A33C29443030A50E"] = "16-35mm f/4G ED VR";
    nikonLensHash["A33C5C8E303C4B0E"] = "Sigma 70-300mm F4-5.6 DG OS";
    nikonLensHash["A4402D8E2C40BF0E"] = "AF-S DX Nikkor 18-300mm f/3.5-6.3G ED VR";
    nikonLensHash["A4472D5024344B0E"] = "Sigma 18-50mm F2.8-4.5 DC OS HSM";
    nikonLensHash["A4485C802424CF0E"] = "70-200mm f/2.8E FL ED VR";
    nikonLensHash["A4485C802424CF4E"] = "AF-S Nikkor 70-200mm f/2.8E FL ED VR";
    nikonLensHash["A45437370C0CA606"] = "24mm f/1.4G ED";
    nikonLensHash["A5402D882C404B0E"] = "Sigma 18-250mm F3.5-6.3 DC OS HSM";
    nikonLensHash["A5403C8E2C3CA70E"] = "28-300mm f/3.5-5.6G ED VR";
    nikonLensHash["A54C44441414C006"] = "35mm f/1.8G ED";
    nikonLensHash["A5546A6A0C0CD006"] = "105mm f/1.4E ED";
    nikonLensHash["A5546A6A0C0CD046"] = "105mm f/1.4E ED";
    nikonLensHash["A6482F2F3030D106"] = "PC Nikkor 19mm f/4E ED";
    nikonLensHash["A6482F2F3030D146"] = "PC Nikkor 19mm f/4E ED";
    nikonLensHash["A648375C24244B06"] = "Sigma 24-70mm F2.8 IF EX DG HSM";
    nikonLensHash["A6488E8E2424A80E"] = "AF-S VR Nikkor 300mm f/2.8G IF-ED II";
    nikonLensHash["A64898982424C10E"] = "400mm f/2.8E FL ED VR";
    nikonLensHash["A73C5380303CC20E"] = "AF-S DX Nikkor 55-200mm f/4-5.6G ED VR II";
    nikonLensHash["A74011262C34D206"] = "AF-S Fisheye Nikkor 8-15mm f/3.5-4.5E ED";
    nikonLensHash["A74011262C34D246"] = "AF-S Fisheye Nikkor 8-15mm f/3.5-4.5E ED";
    nikonLensHash["A74980A024244B06"] = "Sigma APO 200-500mm F2.8 EX DG";
    nikonLensHash["A74B62622C2CA90E"] = "AF-S DX Micro Nikkor 85mm f/3.5G ED VR";
    nikonLensHash["A8381830343CD30E"] = "AF-P DX Nikkor 10–20mm f/4.5–5.6G VR";
    nikonLensHash["A8381830343CD38E"] = "AF-P DX Nikkor 10-20mm f/4.5-5.6G VR";
    nikonLensHash["A84880983030AA0E"] = "AF-S VR Zoom-Nikkor 200-400mm f/4G IF-ED II";
    nikonLensHash["A8488E8E3030C30E"] = "300mm f/4E PF ED VR";
    nikonLensHash["A8488E8E3030C34E"] = "300mm f/4E PF ED VR";
    nikonLensHash["A94C31311414C406"] = "20mm f/1.8G ED";
    nikonLensHash["A95480801818AB0E"] = "200mm f/2G ED VR II";
    nikonLensHash["AA3C376E3030AC0E"] = "24-120mm f/4G ED VR";
    nikonLensHash["AA48375C2424C50E"] = "AF-S Nikkor 24-70mm f/2.8E ED VR";
    nikonLensHash["AA48375C2424C54E"] = "24-70mm f/2.8E ED VR";
    nikonLensHash["AB3CA0A03030C64E"] = "500mm f/4E FL ED VR";
    nikonLensHash["AB445C8E343CD60E"] = "AF-P Nikkor 70-300mm f/4.5–5.6E ED VR";
    nikonLensHash["AB445C8E343CD6CE"] = "AF-P Nikkor 70-300mm f/4.5–5.6E ED VR";
    nikonLensHash["AC38538E343CAE0E"] = "AF-S DX VR Nikkor 55-300mm f/4.5-5.6G ED";
    nikonLensHash["AC3CA6A63030C74E"] = "600mm f/4E FL ED VR";
    nikonLensHash["AC543C3C0C0CD706"] = "AF-S Nikkor 28mm f/1.4E ED";
    nikonLensHash["AC543C3C0C0CD746"] = "AF-S Nikkor 28mm f/1.4E ED";
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
    nikonLensHash["E0402D982C41DF4E"] = "AF 18-400mm F/3.5-6.3 Di II VC HLD (B028)";
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

QByteArray Nikon::nikonDecrypt(QByteArray bData, uint32_t count, uint32_t serial)
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
       quint8 x = static_cast<quint8>(bData[i]);
       x ^= cj;
       bData[i] = static_cast<char>(x);
   }
   return bData;
}

bool Nikon::parse(MetadataParameters &p,
                  ImageMetadata &m,
                  IFD *ifd,
                  Exif *exif,
                  Jpeg *jpeg)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    // moved file.open to readMetadata
    p.file.seek(0);

    // get endian
    bool isBigEnd;
    quint32 order = Utilities::get16(p.file.read(2));
    if (order == 0x4949 || order == 0x4D4D) {
        order == 0x4D4D ? isBigEnd = true : isBigEnd = false;
    }
    else {
        // err, should have been endian order
        G::error(__FUNCTION__, m.fPath, "Endian order not found.");
        return false;
    }
    p.file.read(2);       // skip over 0x2A
    // get offset to first IFD and read it
    quint32 offsetIfd0 = Utilities::get32(p.file.read(4), isBigEnd);
    // Nikon does not chain IFDs
    p.hdr = "IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    ifd->readIFD(p, isBigEnd);


    // pull data reqd from IFD0
    m.make = Utilities::getString(p.file, ifd->ifdDataHash.value(271).tagValue, ifd->ifdDataHash.value(271).tagCount);
    m.model = Utilities::getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount);
    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    m.creator = Utilities::getString(p.file, ifd->ifdDataHash.value(315).tagValue, ifd->ifdDataHash.value(315).tagCount);

    // xmp offset
    m.xmpSegmentOffset = ifd->ifdDataHash.value(700).tagValue;
    // xmpNextSegmentOffset used to later calc available room in xmp
    m.xmpSegmentLength = ifd->ifdDataHash.value(700).tagCount /*+ m.xmpSegmentOffset*/;
    if (m.xmpSegmentOffset) m.isXmp = true;
    else m.isXmp = false;

    quint32 offsetEXIF = 0;
    offsetEXIF = ifd->ifdDataHash.value(34665).tagValue;

//    reportMetadata();

    /* Nikon provides an offset in IFD0 to the offsets for all the subIFDs
    in subIFD0.  Models including the D2H and before have a different structure
    with only one subIFD and an additional preview IFD identified in maker
    notes */
    QList<quint32> ifdOffsets;
    if(ifd->ifdDataHash.contains(330)) {
        if (ifd->ifdDataHash.value(330).tagCount > 1)
            ifdOffsets = ifd->getSubIfdOffsets(p.file, ifd->ifdDataHash.value(330).tagValue,
                             static_cast<int>(ifd->ifdDataHash.value(330).tagCount), isBigEnd);
        else ifdOffsets.append(ifd->ifdDataHash.value(330).tagValue);

        QString hdr;
        // SubIFD1 contains full size jpg offset and length
        if (ifdOffsets.count() > 0) {
            p.hdr = "SubIFD1";
            p.offset = ifdOffsets[0];
            ifd->readIFD(p, isBigEnd);
            // pull data reqd from SubIFD1
            if (ifd->ifdDataHash.contains(513)) {
                // newer models
                m.offsetFull = ifd->ifdDataHash.value(513).tagValue;
                m.lengthFull = ifd->ifdDataHash.value(514).tagValue;
                p.offset = m.offsetFull;
                jpeg->getWidthHeight(p, m.widthFull, m.heightFull);
            }
            else {
            // D2H and older
                m.width = ifd->ifdDataHash.value(256).tagValue;
                m.height = ifd->ifdDataHash.value(257).tagValue;
//                m.offsetFull = ifd->ifdDataHash.value(273).tagValue;
//                m.lengthFull = ifd->ifdDataHash.value(279).tagValue;
            }
        }

        // pull data reqd from SubIFD2
        // SubIFD2 contains image width and height

        if (ifdOffsets.count() > 1) {
            p.hdr = "SubIFD2";
            p.offset = ifdOffsets[1];
            ifd->readIFD(p, isBigEnd);
            m.width = ifd->ifdDataHash.value(256).tagValue;
            m.height = ifd->ifdDataHash.value(257).tagValue;
        }

        // SubIFD3 contains small size jpg offset and length
//        if (ifdOffsets.count() > 2) {
//            hdr = "SubIFD3";
//            p.hdr = "SubIFD3";
//            p.offset = ifdOffsets[2];
//            ifd->readIFD(p, isBigEnd);
//            m.offsetSmall = ifd->ifdDataHash.value(513).tagValue;
//            m.lengthSmall = ifd->ifdDataHash.value(514).tagValue;
////            if (lengthSmallJPG) verifyEmbeddedJpg(offsetSmallJPG, lengthSmallJPG);
//        }
    }

    // read ExifIFD
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD(p, isBigEnd);

    // EXIF: created datetime
    QString createdExif;
    createdExif = Utilities::getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
                        ifd->ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // Exif: get shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = Utilities::getReal(p.file,
                                      ifd->ifdDataHash.value(33434).tagValue,
                                      isBigEnd);
        if (x < 1 ) {
            int t = qRound(1/x);
            m.exposureTime = "1/" + QString::number(t);
            m.exposureTimeNum = x;
        } else {
            int t = static_cast<int>(x);
            m.exposureTime = QString::number(t);
            m.exposureTimeNum = t;
        }
        m.exposureTime += " sec";
    } else {
        m.exposureTime = "";
    }
    // Exif: aperture
    if (ifd->ifdDataHash.contains(33437)) {
        double x = Utilities::getReal(p.file,
                                      ifd->ifdDataHash.value(33437).tagValue,
                                      isBigEnd);
        m.aperture = "f/" + QString::number(x, 'f', 1);
        m.apertureNum = (qRound(x * 10) / 10.0);
    } else {
        m.aperture = "";
        m.apertureNum = 0;
    }
    // Exif: ISO
    if (ifd->ifdDataHash.contains(34855)) {
        quint32 x = ifd->ifdDataHash.value(34855).tagValue;
        m.ISONum = static_cast<int>(x);
        m.ISO = QString::number(m.ISONum);
    } else {
        m.ISO = "";
        m.ISONum = 0;
    }
    // EXIF: Exposure compensation
    if (ifd->ifdDataHash.contains(37380)) {
        // tagType = 10 signed rational
        double x = Utilities::getReal_s(p.file,
                                      ifd->ifdDataHash.value(37380).tagValue,
                                      isBigEnd);
        m.exposureCompensation = QString::number(x, 'f', 1) + " EV";
        m.exposureCompensationNum = x;
    } else {
        m.exposureCompensation = "";
        m.exposureCompensationNum = 0;
    }
    // Exif: focal length
    if (ifd->ifdDataHash.contains(37386)) {
        double x = Utilities::getReal(p.file,
                                      ifd->ifdDataHash.value(37386).tagValue,
                                      isBigEnd);
        m.focalLengthNum = static_cast<int>(x);
        m.focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        m.focalLength = "";
        m.focalLengthNum = 0;
    }

    // Exif: read makernoteIFD
    quint32 makerOffset = ifd->ifdDataHash.value(37500).tagValue;
    if (ifd->ifdDataHash.contains(37500)) {

        /* The makerOffsetBase starts 10 or 12 bits after the offset at endian
        4949 for all recent Nikons. Earlier Nikons, D2H and before, use an endian
        4D4D.  All offsets in the makernoteIFD are relative to makerOffsetBase.
        */

        quint32 endian;
        bool foundEndian = false;
        p.file.seek(makerOffset);
        int step = 0;
        while (!foundEndian) {
            quint32 order = Utilities::get16(p.file.read(2));
            if (order == 0x4949 || order == 0x4D4D) {
                order == 0x4D4D ? isBigEnd = true : isBigEnd = false;
                // offsets are from the endian position in JPEGs
                // therefore must adjust all offsets found in tagValue
                foundEndian = true;
            }
            // add condition to check for EOF
            step++;
            if (step > 100) {
                // err endian order not found
                G::error(__FUNCTION__, m.fPath, "Endian order not found.");
               break;
            }
        }
        quint32 makerOffsetBase = static_cast<quint32>(p.file.pos()) - 2;

        if (p.report) {
            p.rpt << "\nMaker Offset = "
                << makerOffset
                << " / " << QString::number(makerOffset, 16)
                << "  Maker offset base = "
                << makerOffsetBase
                << " / " << QString::number(makerOffsetBase, 16);
        }

        makerOffset = makerOffsetBase + 8;
        p.hdr = "IFD Nikon Maker Note";
        p.offset = makerOffset;
        p.hash = &nikonMakerHash;
        ifd->readIFD(p, isBigEnd);

        // Get serial number, shutter count and lens type to decrypt the lens info
        m.cameraSN = Utilities::getString(p.file, ifd->ifdDataHash.value(29).tagValue + makerOffsetBase,
                                       ifd->ifdDataHash.value(29).tagCount);
        m.shutterCount = ifd->ifdDataHash.value(167).tagValue;
        uint lensType = 0;
        lensType = ifd->ifdDataHash.value(131).tagValue;

        uint32_t serial = static_cast<uint32_t>(m.cameraSN.toInt());
        uint32_t count = static_cast<uint32_t>(m.shutterCount);
        QByteArray encryptedLensInfo = "";
        quint32 offset = ifd->ifdDataHash.value(152).tagValue + makerOffsetBase;
        encryptedLensInfo = Utilities::getByteArray(p.file, offset,
                                 ifd->ifdDataHash.value(152).tagCount);
        QByteArray lensInfo = nikonDecrypt(encryptedLensInfo, count, serial);
        // the byte array code is in the middle of the lensInfo byte stream
        lensInfo.remove(0, 12);
        lensInfo.remove(7, lensInfo.size() - 7);
        lensInfo.append(static_cast<char>(lensType));
        m.nikonLensCode = lensInfo.toHex().toUpper();
//        QByteArray nikonLensCode = lensInfo.toHex().toUpper();
        m.lens = nikonLensHash.value(m.nikonLensCode);
        /*
        or could go with nikonLensHash<QString, QString>
        and use lensInfo.toHex().toUpper() as the key  */

        // Find the preview IFD offset
        if (ifd->ifdDataHash.contains(17)) {
            p.hdr = "IFD Nikon Maker Note: PreviewIFD";
            quint32 offset = ifd->ifdDataHash.value(17).tagValue + makerOffsetBase;
            p.offset = offset;
            p.hash = &exif->hash;
            ifd->readIFD(p, isBigEnd);

            m.offsetThumb = ifd->ifdDataHash.value(513).tagValue + makerOffsetBase;
            m.lengthThumb = ifd->ifdDataHash.value(514).tagValue;
            // older nikons like D2h and D100
            if (m.lengthFull == 0 && m.lengthThumb > 0) {
                m.offsetFull = m.offsetThumb;
                m.lengthFull = m.lengthThumb;
                p.offset = m.offsetFull;
                jpeg->getWidthHeight(p, m.widthFull, m.heightFull);
            }
//            if (lengthSmallJPG) verifyEmbeddedJpg(offsetSmallJPG, lengthSmallJPG);
        }
    }

    // read XMP
    bool okToReadXmp = true;
    if (m.isXmp && okToReadXmp) {
        Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpSegmentLength);
        m.rating = xmp.getItem("Rating");     // case is important "Rating"
        m.label = xmp.getItem("Label");       // case is important "Label"
        if (m.title.isEmpty()) m.title = xmp.getItem("title");       // case is important "title"
        if (m.cameraSN.isEmpty()) m.cameraSN = xmp.getItem("SerialNumber");
        if (m.lens.isEmpty()) m.lens = xmp.getItem("Lens");
        if (m.lensSN.isEmpty()) m.lensSN = xmp.getItem("LensSerialNumber");
        if (m.creator.isEmpty()) m.creator = xmp.getItem("creator");
        if (m.copyright.isEmpty()) m.copyright = xmp.getItem("rights");
        m.email = xmp.getItem("email");
        m.url = xmp.getItem("url");
//        m.email = xmp.getItem("CiEmailWork");
//        m.url = xmp.getItem("CiUrlWork");

        // save original values so can determine if edited when writing changes
        m._rating = m.rating;
        m._label = m.label;
        m._title = m.title;
        m._creator = m.creator;
        m._copyright = m.copyright;
        m._email  = m.email ;
        m._url = m.url;
        m._orientation = m.orientation;
        m._rotationDegrees = m.rotationDegrees;

        if (p.report) p.xmpString = xmp.xmpAsString();
    }

    return true;
}
