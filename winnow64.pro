# Winnow64.pro
CONFIG += sdk_no_version_check

macx:CONFIG += app_bundle

# Must rerun qmake and rebuild when change:
#CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT
#CONFIG(release, debug|release):DEFINES += QT_NO_WARNING_OUTPUT

#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000
greaterThan(QT_MAJOR_VERSION, 5) {
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x051500
#    win32:QT += core5compat   # huge performance hit without this
}

# QMAKE_BUNDLE_IDENTIFIER = com.winnow.Winnow

# Compile without warnings
# GCC / Clang
# QMAKE_CXXFLAGS += -Wno-unused-variable
# QMAKE_CFLAGS += -Wno-unused-variable
# MSVC
# pragma warning(disable: 4101) # 4101 is the warning number for unused variable in MSVC

#QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.13

# Compile on MacOS without permission prompts:
# - Add Build Step > Custom Process Step
# - Command: chmod
# - Arguments: +x /path/to.winnow.exe

# MacOS universal binaries
# QMAKE_APPLE_DEVICE_ARCHS = arm64
# QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64

CONFIG += c++17
win32:QMAKE_CXXFLAGS += /std:c++17
win32:QMAKE_CXXFLAGS += /Zc:__cplusplus
win32:QMAKE_CXXFLAGS += /wd4138             # supress "*/" found outside of comment

TEMPLATE = app
TARGET = Winnow

# Precompiled header setup
# CONFIG += precompile_header
# PRECOMPILED_HEADER = $$PWD/global.h

INCLUDEPATH += $$PWD
INCLUDEPATH += .
INCLUDEPATH += Dialogs
INCLUDEPATH += Utilities
INCLUDEPATH += FocusStack
# INCLUDEPATH += FocusStack/Petteri
# INCLUDEPATH += FocusStack/PetteriModular

# Petteri Modular engine
# include($$PWD/FocusStack/PetteriModular/petteri_modular.pri)
# DISTFILES += FocusStack/PetteriModular/petteri_modular.pri


DEPENDPATH += $$PWD

Release:DESTDIR = release
Release:OBJECTS_DIR = release/.obj
Release:MOC_DIR = release/.moc
Release:RCC_DIR = release/.rcc
Release:UI_DIR = release/.ui

Debug:DESTDIR = debug
Debug:OBJECTS_DIR = debug/.obj
Debug:MOC_DIR = debug/.moc
Debug:RCC_DIR = debug/.rcc
Debug:UI_DIR = debug/.ui

QT += core testlib

QT += widgets
QT += multimedia
QT += multimediawidgets
QT += concurrent

HEADERS += Cache/cachedata.h \
    FocusStack/fs.h \
    FocusStack/fsalign.h \
    FocusStack/fsdepth.h \
    FocusStack/fsfocus.h \
    FocusStack/fsfusion.h \
    FocusStack/fsgray.h \
    FocusStack/fsloader.h \
    FocusStack/fsrunner.h
HEADERS += Cache/tiffthumbdecoder.h
HEADERS += ImageFormats/Video/mov.h
HEADERS += ImageFormats/Video/mp4.h
HEADERS += ImageFormats/Png/png.h
HEADERS += Cache/framedecoder.h
HEADERS += Cache/imagecache.h
HEADERS += Cache/imagedecoder.h
HEADERS += Cache/metaread.h
HEADERS += Cache/reader.h
HEADERS += Datamodel/buildfilters.h
HEADERS += Datamodel/datamodel.h
HEADERS += Datamodel/filters.h
HEADERS += Datamodel/selection.h
HEADERS += Dialogs/aboutdlg.h
HEADERS += Dialogs/addthumbnailsdlg.h
HEADERS += Dialogs/appdlg.h
HEADERS += Dialogs/copystyledlg.h
HEADERS += Dialogs/editlistdlg.h
HEADERS += Dialogs/erasememcardimagesdlg.h
HEADERS += Dialogs/findduplicatesdlg.h
HEADERS += Dialogs/imagedlg.h
HEADERS += Dialogs/ingestdlg.h
HEADERS += Dialogs/ingesterrors.h
HEADERS += Dialogs/loadusbdlg.h
HEADERS += Dialogs/loupeinfodlg.h
HEADERS += Dialogs/managegraphicsdlg.h
HEADERS += Dialogs/manageimagesdlg.h
HEADERS += Dialogs/managetilesdlg.h
HEADERS += Dialogs/patterndlg.h
HEADERS += Dialogs/preferencesdlg.h
HEADERS += Dialogs/renamedlg.h
HEADERS += Dialogs/saveasdlg.h
HEADERS += Dialogs/selectionorpicksdlg.h
HEADERS += Dialogs/testaligndlg.h
HEADERS += Dialogs/tokendlg.h
HEADERS += Dialogs/updateapp.h
HEADERS += Dialogs/workspacedlg.h
HEADERS += Dialogs/zoomdlg.h
HEADERS += Effects/effects.h
HEADERS += Effects/graphicseffect.h
HEADERS += Embellish/embel.h
HEADERS += Effects/graphicsitemeventfilter.h
HEADERS += Effects/interpolate.h
HEADERS += Effects/REF_imageblitz_effects.h
HEADERS += Embellish/embelexport.h
HEADERS += Embellish/Properties/embelproperties.h
HEADERS += File/bookmarks.h
HEADERS += File/fstree.h
HEADERS += File/hoverdelegate.h
HEADERS += File/ingest.h

HEADERS += FocusStack/Contracts/IAlign.h
HEADERS += FocusStack/Contracts/IDepthMap.h
HEADERS += FocusStack/Contracts/IFocusmaps.h
HEADERS += FocusStack/Contracts/IFusion.h

HEADERS += FocusStack/Mask/maskassessor.h
HEADERS += FocusStack/Mask/maskgenerator.h
HEADERS += FocusStack/Mask/maskoptions.h
HEADERS += FocusStack/Mask/maskrefiner.h

# HEADERS += FocusStack/Petteri/3dpreview.h
# HEADERS += FocusStack/Petteri/align.h
# HEADERS += FocusStack/Petteri/background_removal.h
# HEADERS += FocusStack/Petteri/denoise.h
# HEADERS += FocusStack/Petteri/depthmap.h
# HEADERS += FocusStack/Petteri/depthmap_inpaint.h
# HEADERS += FocusStack/Petteri/fast_bilateral.h
# HEADERS += FocusStack/Petteri/focusmeasure.h
# HEADERS += FocusStack/Petteri/focusstack.h
# HEADERS += FocusStack/Petteri/focusstackworker.h
# HEADERS += FocusStack/Petteri/grayscale.h
# HEADERS += FocusStack/Petteri/histogrampercentile.h
# HEADERS += FocusStack/Petteri/loadimg.h
# HEADERS += FocusStack/Petteri/logger.h
# HEADERS += FocusStack/Petteri/merge.h
# HEADERS += FocusStack/Petteri/options.h
# HEADERS += FocusStack/Petteri/radialfilter.h
# HEADERS += FocusStack/Petteri/reassign.h
# HEADERS += FocusStack/Petteri/saveimg.h
# HEADERS += FocusStack/Petteri/wavelet.h
# HEADERS += FocusStack/Petteri/wavelet_opencl.h
# HEADERS += FocusStack/Petteri/wavelet_templates.h
# HEADERS += FocusStack/Petteri/worker.h

HEADERS += FocusStack/PetteriModular/Align/align.h
HEADERS += FocusStack/PetteriModular/Align/background_removal.h
HEADERS += FocusStack/PetteriModular/Align/radialfilter.h
HEADERS += FocusStack/PetteriModular/Core/logger.h
HEADERS += FocusStack/PetteriModular/Core/focusstack.h
HEADERS += FocusStack/PetteriModular/Core/worker.h
HEADERS += FocusStack/PetteriModular/Denoise/denoise.h
HEADERS += FocusStack/PetteriModular/Depth/depthmap.h
HEADERS += FocusStack/PetteriModular/Depth/depthmap_inpaint.h
HEADERS += FocusStack/PetteriModular/Denoise/fast_bilateral.h
HEADERS += FocusStack/PetteriModular/Focus/focusmeasure.h
HEADERS += FocusStack/PetteriModular/Focus/histogrampercentile.h
HEADERS += FocusStack/PetteriModular/Fusion/merge.h
HEADERS += FocusStack/PetteriModular/Fusion/reassign.h
HEADERS += FocusStack/PetteriModular/GrayScale/grayscale.h
HEADERS += FocusStack/PetteriModular/IO/loadimg.h
HEADERS += FocusStack/PetteriModular/IO/saveimg.h
HEADERS += FocusStack/PetteriModular/Options/options.h
HEADERS += FocusStack/PetteriModular/View/3dpreview.h
HEADERS += FocusStack/PetteriModular/Wave/wavelet.h
HEADERS += FocusStack/PetteriModular/Wave/wavelet_opencl.h
HEADERS += FocusStack/PetteriModular/Wave/wavelet_opencl_kernels.cl
HEADERS += FocusStack/PetteriModular/Wave/wavelet_templates.h
HEADERS += FocusStack/PetteriModular/Wrappers/petterialignworker.h
HEADERS += FocusStack/PetteriModular/Wrappers/petteridepthmapworker.h
HEADERS += FocusStack/PetteriModular/Wrappers/petterifocusmapsworker.h
HEADERS += FocusStack/PetteriModular/Wrappers/petteripmaxfusionworker.h

HEADERS += FocusStack/Pipeline/PipelineBase.h
HEADERS += FocusStack/Pipeline/pipelinepmax.h

HEADERS += FocusStack/Stages/Align/petterialign.h
HEADERS += FocusStack/Stages/Depth/petteridepthmap.h
HEADERS += FocusStack/Stages/Focus/petterifocusmaps.h
HEADERS += FocusStack/Stages/Fusion/petteripmaxfusion.h

HEADERS += Image/autonomousimage.h
HEADERS += Image/imagealign.h
HEADERS += Image/pixmap.h
HEADERS += Image/stack.h
HEADERS += Image/thumb.h
#HEADERS += Image/tiffhandler.h
HEADERS += ImageFormats/Canon/canon.h
HEADERS += ImageFormats/Canon/canoncr3.h
HEADERS += ImageFormats/Dng/dng.h
HEADERS += ImageFormats/Fuji/fuji.h
#rgh remove heic
HEADERS += ImageFormats/Heic/heic.h
HEADERS += ImageFormats/Heic/heif.h
#HEADERS += ImageFormats/Heic/de265.h
#win32:HEADERS += ImageFormats/Heic/heic.h
#win32:HEADERS += ImageFormats/Heic/heif.h
win32:HEADERS += ImageFormats/Heic/de265.h
HEADERS += ImageFormats/Jpeg/jpeg.h
macx:HEADERS += ImageFormats/Jpeg/jpeg2.h
macx:HEADERS += ImageFormats/Jpeg/decoderJpg.h
macx:HEADERS += ImageFormats/Jpeg/jpgdecoder.h
macx:HEADERS += ImageFormats/jpeg/jpegturbo.h
HEADERS += ImageFormats/Nikon/nikon.h
HEADERS += ImageFormats/Olympus/olympus.h
HEADERS += ImageFormats/Panasonic/panasonic.h
HEADERS += ImageFormats/Sony/sony.h
HEADERS += ImageFormats/Tiff/tiff.h
macx:HEADERS += ImageFormats/Tiff/libtiff.h
HEADERS += ImageFormats/Tiff/rorytiff.h
HEADERS += ImageFormats/Base/rawbase.h
HEADERS += ImageFormats/Tiff/tiffdecoder.h
HEADERS += ImageStack/FocusStackConstants.h
HEADERS += ImageStack/stack_depth_map.h
HEADERS += ImageStack/focushalo.h
HEADERS += ImageStack/stack_focus_measure.h
HEADERS += ImageStack/focusstackutilities.h
HEADERS += ImageStack/focuswavelet.h
HEADERS += ImageStack/stackaligner.h
HEADERS += ImageStack/stackcontroller.h
HEADERS += ImageStack/stackfusion.h
HEADERS += Lcms2/lcms2.h
HEADERS += Lcms2/lcms2_plugin.h
HEADERS += Log/issue.h
HEADERS += Log/log.h
HEADERS += Main/dockwidget.h
HEADERS += Main/global.h
# HEADERS += Main/logger.h
HEADERS += Main/mainwindow.h
HEADERS += Main/qtlocalpeer.h
HEADERS += Main/qtlockedfile.h
HEADERS += Main/qtsingleapplication.h
HEADERS += Main/widgetcss.h
win32:HEADERS += Main/winnativeeventfilter.h
HEADERS += Metadata/exif.h
HEADERS += Metadata/exiftool.h
HEADERS += Metadata/irb.h
HEADERS += Metadata/gps.h
HEADERS += Metadata/ifd.h
HEADERS += Metadata/iptc.h
HEADERS += Metadata/imagemetadata.h
HEADERS += Metadata/metadata.h
HEADERS += Metadata/metareport.h
HEADERS += Metadata/rapidxml.h
HEADERS += Metadata/rapidxml_print_rgh.h
HEADERS += Metadata/xmp.h
#HEADERS += Metadata/ExifTool.h
#HEADERS += Metadata/ExifToolPipe.h
#HEADERS += Metadata/TagInfo.h
HEADERS += PropertyEditor/preferences.h
HEADERS += PropertyEditor/propertyeditor.h
HEADERS += PropertyEditor/propertydelegate.h
HEADERS += PropertyEditor/propertywidgets.h
HEADERS += Utilities/bit.h
HEADERS += Utilities/classificationlabel.h
HEADERS += Utilities/coloranalysis.h
HEADERS += Utilities/dirwatcher.h
HEADERS += Utilities/dropshadowlabel.h
HEADERS += Utilities/focuspointtrainer.h
HEADERS += Utilities/focuspredictor.h
HEADERS += Utilities/foldercompressor.h
HEADERS += Utilities/htmlwindow.h
HEADERS += Utilities/icc.h
macx:HEADERS += Utilities/mac.h
win32:HEADERS += Utilities/win.h
HEADERS += Utilities/inputdlg.h
HEADERS += Utilities/movingavg.h
HEADERS += Utilities/performance.h
HEADERS += Utilities/popup.h
HEADERS += Utilities/progressbar.h
HEADERS += Utilities/renamefile.h
HEADERS += Utilities/showhelp.h
HEADERS += Utilities/stresstest.h
HEADERS += Utilities/usbutil.h
HEADERS += Utilities/utilities.h
HEADERS += Views/compareImages.h
HEADERS += Views/compareview.h
HEADERS += Views/iconview.h
HEADERS += Views/iconviewdelegate.h
HEADERS += Views/imageview.h
HEADERS += Views/infostring.h
HEADERS += Views/infoview.h
HEADERS += Views/tableview.h
HEADERS += Views/videoview.h
HEADERS += Views/videowidget.h

SOURCES += Cache/cachedata.cpp \
    FocusStack/fs.cpp \
    FocusStack/fsalign.cpp \
    FocusStack/fsdepth.cpp \
    FocusStack/fsfocus.cpp \
    FocusStack/fsfusion.cpp \
    FocusStack/fsgray.cpp \
    FocusStack/fsloader.cpp \
    FocusStack/fsrunner.cpp
SOURCES += Utilities/focuspointtrainer.cpp
SOURCES += Utilities/focuspredictor.cpp
SOURCES += Cache/tiffthumbdecoder.cpp
SOURCES += ImageFormats/Video/mov.cpp
SOURCES += ImageFormats/Video/mp4.cpp
SOURCES += ImageFormats/Png/png.cpp
SOURCES += Cache/framedecoder.cpp
SOURCES += Cache/imagecache.cpp
SOURCES += Cache/imagedecoder.cpp
SOURCES += Cache/metaread.cpp
SOURCES += Cache/reader.cpp
SOURCES += Datamodel/buildfilters.cpp
SOURCES += Datamodel/datamodel.cpp
SOURCES += Datamodel/filters.cpp
SOURCES += Datamodel/selection.cpp
SOURCES += Dialogs/aboutdlg.cpp
SOURCES += Dialogs/addthumbnailsdlg.cpp
SOURCES += Dialogs/appdlg.cpp
SOURCES += Dialogs/copystyledlg.cpp
SOURCES += Dialogs/editlistdlg.cpp
SOURCES += Dialogs/erasememcardimagesdlg.cpp
SOURCES += Dialogs/imagedlg.cpp
SOURCES += Dialogs/ingestdlg.cpp
SOURCES += Dialogs/ingesterrors.cpp
SOURCES += Dialogs/loadusbdlg.cpp
SOURCES += Dialogs/loupeinfodlg.cpp
SOURCES += Dialogs/managegraphicsdlg.cpp
SOURCES += Dialogs/manageimagesdlg.cpp
SOURCES += Dialogs/managetilesdlg.cpp
SOURCES += Dialogs/patterndlg.cpp
SOURCES += Dialogs/preferencesdlg.cpp
SOURCES += Dialogs/renamedlg.cpp
SOURCES += Dialogs/saveasdlg.cpp
SOURCES += Dialogs/selectionorpicksdlg.cpp
SOURCES += Dialogs/testaligndlg.cpp
SOURCES += Dialogs/tokendlg.cpp
SOURCES += Dialogs/updateapp.cpp
SOURCES += Dialogs/findduplicatesdlg.cpp
SOURCES += Dialogs/workspacedlg.cpp
SOURCES += Dialogs/zoomdlg.cpp
SOURCES += Effects/effects.cpp
SOURCES += Effects/graphicseffect.cpp
SOURCES += Effects/graphicsitemeventfilter.cpp
SOURCES += Embellish/embel.cpp
SOURCES += Embellish/Properties/embelproperties.cpp
SOURCES += Embellish/embelexport.cpp
SOURCES += File/bookmarks.cpp
SOURCES += File/fstree.cpp
SOURCES += File/hoverdelegate.cpp
SOURCES += File/ingest.cpp

SOURCES += FocusStack/generateFocusStack.cpp

SOURCES += FocusStack/Mask/maskassessor.cpp
SOURCES += FocusStack/Mask/maskgenerator.cpp
SOURCES += FocusStack/Mask/maskrefiner.cpp

# Temp comment out while working on PetteriModular
# SOURCES += FocusStack/Petteri/3dpreview.cpp
# SOURCES += FocusStack/Petteri/align.cpp
# SOURCES += FocusStack/Petteri/background_removal.cpp
# SOURCES += FocusStack/Petteri/denoise.cpp
# SOURCES += FocusStack/Petteri/depthmap.cpp
# SOURCES += FocusStack/Petteri/depthmap_inpaint.cpp
# SOURCES += FocusStack/Petteri/focusmeasure.cpp
# SOURCES += FocusStack/Petteri/focusstack.cpp
# SOURCES += FocusStack/Petteri/focusstackworker.cpp
# SOURCES += FocusStack/Petteri/grayscale.cpp
# SOURCES += FocusStack/Petteri/histogrampercentile.cpp
# SOURCES += FocusStack/Petteri/loadimg.cpp
# SOURCES += FocusStack/Petteri/logger.cpp
# SOURCES += FocusStack/Petteri/merge.cpp
# SOURCES += FocusStack/Petteri/options.cpp
# SOURCES += FocusStack/Petteri/radialfilter.cpp
# SOURCES += FocusStack/Petteri/reassign.cpp
# SOURCES += FocusStack/Petteri/saveimg.cpp
# SOURCES += FocusStack/Petteri/wavelet.cpp
# SOURCES += FocusStack/Petteri/wavelet_opencl.cpp
# SOURCES += FocusStack/Petteri/worker.cpp

SOURCES += FocusStack/PetteriModular/Align/align.cpp
SOURCES += FocusStack/PetteriModular/Align/background_removal.cpp
SOURCES += FocusStack/PetteriModular/Align/radialfilter.cpp
SOURCES += FocusStack/PetteriModular/Core/focusstack.cpp
SOURCES += FocusStack/PetteriModular/Core/logger.cpp
SOURCES += FocusStack/PetteriModular/Core/worker.cpp
SOURCES +=
SOURCES += FocusStack/PetteriModular/Depth/depthmap.cpp
SOURCES += FocusStack/PetteriModular/Depth/depthmap_inpaint.cpp
SOURCES += FocusStack/PetteriModular/Denoise/denoise.cpp
SOURCES += FocusStack/PetteriModular/Focus/focusmeasure.cpp
SOURCES += FocusStack/PetteriModular/Focus/histogrampercentile.cpp
SOURCES += FocusStack/PetteriModular/Fusion/merge.cpp
SOURCES += FocusStack/PetteriModular/Fusion/reassign.cpp
SOURCES += FocusStack/PetteriModular/GrayScale/grayscale.cpp
SOURCES += FocusStack/PetteriModular/IO/loadimg.cpp
SOURCES += FocusStack/PetteriModular/IO/saveimg.cpp
SOURCES += FocusStack/PetteriModular/Options/options.cpp
SOURCES += FocusStack/PetteriModular/View/3dpreview.cpp
SOURCES += FocusStack/PetteriModular/Wave/wavelet.cpp
SOURCES += FocusStack/PetteriModular/Wave/wavelet_opencl.cpp
SOURCES += FocusStack/PetteriModular/Wrappers/petterialignworker.cpp
SOURCES += FocusStack/PetteriModular/Wrappers/petteridepthmapworker.cpp
SOURCES += FocusStack/PetteriModular/Wrappers/petterifocusmapsworker.cpp
SOURCES += FocusStack/PetteriModular/Wrappers/petteripmaxfusionworker.cpp

SOURCES += FocusStack/Pipeline/PipelineBase.cpp
SOURCES += FocusStack/Pipeline/pipelinepmax.cpp

SOURCES += FocusStack/Stages/Align/petterialign.cpp
SOURCES += FocusStack/Stages/Depth/petteridepthmap.cpp
SOURCES += FocusStack/Stages/Focus/petterifocusmaps.cpp
SOURCES += FocusStack/Stages/Fusion/petteripmaxfusion.cpp

SOURCES += Image/autonomousimage.cpp
SOURCES += Image/imagealign.cpp
SOURCES += Image/pixmap.cpp
SOURCES += Image/stack.cpp
SOURCES += Image/thumb.cpp
#SOURCES += Image/tiffhandler.cpp
SOURCES += ImageFormats/Canon/canon.cpp
SOURCES += ImageFormats/Canon/canoncr3.cpp
SOURCES += ImageFormats/Dng/dng.cpp
SOURCES += ImageFormats/Fuji/fuji.cpp
SOURCES += ImageFormats/Heic/Heic.cpp
#win32:SOURCES += ImageFormats/Heic/Heic.cpp
SOURCES += ImageFormats/Jpeg/jpeg.cpp               # For parsing
macx:SOURCES += ImageFormats/Jpeg/jpeg2.cpp         # Copy jpeg.h for experimenting
macx:SOURCES += ImageFormats/Jpeg/jpgdecoder.cpp    # JED project (Jpeg Encoder Decoder)
macx:SOURCES += ImageFormats/Jpeg/jpegturbo.cpp     # libjpeg-turbo
SOURCES += ImageFormats/Nikon/nikon.cpp
SOURCES += ImageFormats/Olympus/olympus.cpp
SOURCES += ImageFormats/Panasonic/panasonic.cpp
SOURCES += ImageFormats/Sony/sony.cpp
SOURCES += ImageFormats/Tiff/tiff.cpp               # For parsing
macx:SOURCES += ImageFormats/Tiff/libtiff.cpp       # use library directly
SOURCES += ImageFormats/Tiff/rorytiff.cpp           # Rory decoder
SOURCES += ImageFormats/Base/rawbase.cpp
SOURCES += ImageFormats/Tiff/tiffdecoder.cpp
SOURCES += ImageStack/stack_depth_map.cpp
SOURCES += ImageStack/focushalo.cpp
SOURCES += ImageStack/stack_focus_measure.cpp
SOURCES += ImageStack/focusstackutilities.cpp
SOURCES += ImageStack/focuswavelet.cpp
SOURCES += ImageStack/stackaligner.cpp
SOURCES += ImageStack/stackcontroller.cpp
SOURCES += ImageStack/stackfusion.cpp
SOURCES += Lcms2/cmsalpha.c
SOURCES += Lcms2/cmscam02.c
SOURCES += Lcms2/cmscgats.c
SOURCES += Lcms2/cmscnvrt.c
SOURCES += Lcms2/cmserr.c
SOURCES += Lcms2/cmsgamma.c
SOURCES += Lcms2/cmsgmt.c
SOURCES += Lcms2/cmshalf.c
SOURCES += Lcms2/cmsintrp.c
SOURCES += Lcms2/cmsio0.c
SOURCES += Lcms2/cmsio1.c
SOURCES += Lcms2/cmslut.c
SOURCES += Lcms2/cmsmd5.c
SOURCES += Lcms2/cmsmtrx.c
SOURCES += Lcms2/cmsnamed.c
SOURCES += Lcms2/cmsopt.c
SOURCES += Lcms2/cmspack.c
SOURCES += Lcms2/cmspcs.c
SOURCES += Lcms2/cmsplugin.c
SOURCES += Lcms2/cmsps2.c
SOURCES += Lcms2/cmssamp.c
SOURCES += Lcms2/cmssm.c
SOURCES += Lcms2/cmstypes.c
SOURCES += Lcms2/cmsvirt.c
SOURCES += Lcms2/cmswtpnt.c
SOURCES += Lcms2/cmsxform.c
SOURCES += Log/issue.cpp
SOURCES += Log/log.cpp
SOURCES += Main/main.cpp
# all part of MW (mainwindow)
SOURCES += Main/mainwindow.cpp
SOURCES += Main/diagnostics.cpp
SOURCES += Main/draganddrop.cpp
SOURCES += Main/externalapp.cpp
SOURCES += Main/fileoperations.cpp
SOURCES += Main/initialize.cpp
# SOURCES += Main/log.cpp
# SOURCES += Main/logger.cpp
SOURCES += Main/menusandactions.cpp
SOURCES += Main/navigate.cpp
SOURCES += Main/pick.cpp
SOURCES += Main/setandupdate.cpp
SOURCES += Main/settings.cpp
SOURCES += Main/slideshow.cpp
SOURCES += Main/sortandfilter.cpp
SOURCES += Main/status.cpp
SOURCES += Main/test.cpp
SOURCES += Main/viewmodes.cpp
win32:SOURCES += Main/winnativeeventfilter.cpp
SOURCES += Main/workspaces.cpp
SOURCES += Main/dockwidget.cpp
SOURCES += Main/global.cpp
SOURCES += Main/qtlocalpeer.cpp
SOURCES += Main/qtlockedfile.cpp
macx:SOURCES += Main/qtlockedfile_unix.cpp
win32:SOURCES += Main/qtlockedfile_win.cpp
SOURCES += Main/qtsingleapplication.cpp
SOURCES += Main/widgetcss.cpp
SOURCES += Metadata/exif.cpp
SOURCES += Metadata/exiftool.cpp
SOURCES += Metadata/irb.cpp
SOURCES += Metadata/gps.cpp
SOURCES += Metadata/ifd.cpp
SOURCES += Metadata/iptc.cpp
SOURCES += Metadata/metadata.cpp
SOURCES += Metadata/metareport.cpp
SOURCES += Metadata/xmp.cpp
#SOURCES += Metadata/ExifTool.cpp
#SOURCES += Metadata/ExifToolPipe.cpp
#SOURCES += Metadata/TagInfo.cpp
SOURCES += PropertyEditor/preferences.cpp
SOURCES += PropertyEditor/propertyeditor.cpp
SOURCES += PropertyEditor/propertydelegate.cpp
SOURCES += PropertyEditor/propertywidgets.cpp
SOURCES += Utilities/bit.cpp
SOURCES += Utilities/classificationlabel.cpp
SOURCES += Utilities/coloranalysis.cpp
SOURCES += Utilities/dirwatcher.cpp
SOURCES += Utilities/dropshadowlabel.cpp
SOURCES += Utilities/foldercompressor.cpp
SOURCES += Utilities/htmlwindow.cpp
SOURCES += Utilities/icc.cpp
SOURCES += Utilities/inputdlg.cpp
macx:SOURCES += Utilities/mac.mm
SOURCES += Utilities/performance.cpp
SOURCES += Utilities/popup.cpp
SOURCES += Utilities/progressbar.cpp
SOURCES += Utilities/renamefile.cpp
SOURCES += Utilities/showhelp.cpp
SOURCES += Utilities/stresstest.cpp
SOURCES += Utilities/usbutil.cpp
SOURCES += Utilities/utilities.cpp
win32:SOURCES += Utilities/win.cpp
SOURCES += Views/compareImages.cpp
SOURCES += Views/compareview.cpp
SOURCES += Views/iconview.cpp
SOURCES += Views/iconviewdelegate.cpp
SOURCES += Views/imageview.cpp
SOURCES += Views/infoview.cpp
SOURCES += Views/tableview.cpp
SOURCES += Views/infostring.cpp
SOURCES += Views/videoview.cpp
SOURCES += Views/videowidget.cpp

FORMS += Dialogs/aboutdlg.ui
FORMS += Dialogs/loupeinfodlg.ui
FORMS += Dialogs/addthumbnailsdlg.ui
FORMS += Dialogs/aligndlg.ui
FORMS += Dialogs/appdlg.ui
FORMS += Dialogs/copystyledlg.ui
FORMS += Dialogs/editlistdlg.ui
FORMS += Dialogs/erasememcardimagesdlg.ui
FORMS += Dialogs/findduplicatesdlg.ui
FORMS += Dialogs/imagedlg.ui
FORMS += Dialogs/ingestdlg.ui
FORMS += Dialogs/ingesterrors.ui
FORMS += Dialogs/loadusbdlg.ui
FORMS += Dialogs/managegraphicsdlg.ui
FORMS += Dialogs/manageimagesdlg.ui
FORMS += Dialogs/managetilesdlg.ui
FORMS += Dialogs/patterndlg.ui
FORMS += Dialogs/renamedlg.ui
FORMS += Dialogs/renamefiledlg.ui
FORMS += Dialogs/saveasdlg.ui
FORMS += Dialogs/selectionorpicksdlg.ui
FORMS += Dialogs/testaligndlg.ui
FORMS += Dialogs/testdlg.ui
FORMS += Dialogs/test1.ui
FORMS += Dialogs/test12.ui
FORMS += Dialogs/tokendlg.ui
FORMS += Dialogs/updateapp.ui
FORMS += Dialogs/workspacedlg.ui
FORMS += Dialogs/zoomdlg.ui
FORMS += Embellish/embelCoord.ui
FORMS += Help/filtershelp.ui
FORMS += Help/foldershelp.ui
FORMS += Help/helpfindduplicates.ui
FORMS += Help/helpform.ui
FORMS += Help/helpingest.ui
FORMS += Help/helppixeldelta.ui
FORMS += Help/ingestautopath.ui
FORMS += Help/introduction.ui
FORMS += Help/message.ui
FORMS += Help/shortcutsform.ui
FORMS += Help/welcome.ui
FORMS += Metadata/metadatareport.ui
FORMS += Utilities/inputdlg.ui

RESOURCES += winnow.qrc
ICON = images/winnow.icns
RC_ICONS = images/winnow.ico

DISTFILES += $$files(Lib/*, true) \
    FocusStack/Petteri/wavelet_opencl_kernels.cl
DISTFILES += notes/Documentation.txt
DISTFILES += Docs/ingestautopath
DISTFILES += notes/InstallMediaPipe.txt
DISTFILES += CMakeLists.txt
DISTFILES += _CMakeLists.txt
DISTFILES += notes/_InstallLibrary.txt
DISTFILES += notes/_install library.txt
DISTFILES += notes/_install library.txt
DISTFILES += notes/_install library.txt
DISTFILES += notes/_install library.txt
DISTFILES += notes/Create help dialog.rtf
DISTFILES += notes/Create help dialog.txt
DISTFILES += Docs/ingestautopath.html
DISTFILES += Docs/versions
DISTFILES += Docs/test.html
DISTFILES += notes/_Symbols
DISTFILES += notes/_Notes
DISTFILES += notes/_ToDo.txt
DISTFILES += notes/DeployInstall.txt
DISTFILES += notes/ExiftoolCommands.txt
DISTFILES += notes/git.txt
DISTFILES += notes/help_videos_on_mac.txt
DISTFILES += notes/HelpDocCreation.txt
DISTFILES += notes/HelpVideoNotes.txt
DISTFILES += notes/hevcinfo
DISTFILES += notes/ImageMagick
DISTFILES += notes/Menu.txt
DISTFILES += notes/Object_detection
DISTFILES += notes/scratch.css
DISTFILES += notes/scratch.html
DISTFILES += notes/Scratch.txt
DISTFILES += notes/Scratch2.txt
DISTFILES += notes/Shortcuts.txt
DISTFILES += notes/snippets.txt
DISTFILES += notes/users.txt
DISTFILES += notes/VideoScripts
DISTFILES += notes/xmp.txt

# *** MAC Libraries ***
macx {
    LIBS += -framework ApplicationServices
    LIBS += -framework AppKit
    LIBS += -framework CoreFoundation
    LIBS += -framework Foundation

    # opencv
    LIBS += -L/opt/homebrew/lib
    LIBS += -lopencv_core.411
    LIBS += -lopencv_imgproc.411
    LIBS += -lopencv_imgcodecs
    LIBS += -lopencv_video
    LIBS += -lopencv_dnn.411
    INCLUDEPATH += /opt/homebrew/opt/opencv/include/opencv4

    # libtiff
    LIBS += -L/opt/homebrew/opt/libtiff/lib -ltiff
    INCLUDEPATH += /opt/homebrew/opt/libtiff/include

    # libjpeg-turbo
    LIBS += -L/opt/homebrew/opt/jpeg-turbo/lib -lturbojpeg
    INCLUDEPATH += /opt/homebrew/opt/jpeg-turbo/include

    # zLib
    LIBS += -lz
    INCLUDEPATH += /usr/include

    # ExifTool folder (recursive)
    QMAKE_BUNDLE_DATA += exiftool_data
    exiftool_data.files = $$files($$PWD/ReleaseExtras/ExifTool, true)
    exiftool_data.path  = Contents/MacOS

    # Winnets folder (recursive)
    QMAKE_BUNDLE_DATA += winnets_data
    winnets_data.files = $$files($$PWD/ReleaseExtras/Winnets, true)
    winnets_data.path  = Contents/MacOS

    # ONNX model file
    QMAKE_BUNDLE_DATA += model_data
    model_data.files = $$PWD/ReleaseExtras/focus_point_model.onnx
    model_data.path  = Contents/MacOS
}

# *** WIN Libraries ***
win32 {
    CONFIG(debug, debug|release) {
        message("🔧 Debug build - using debug libraries")

        # --- OpenCV (single combined lib build) ---
        LIBS += -L$$PWD/Lib/opencv/windows/build/x64/vc16/lib -lopencv_world4110
        INCLUDEPATH += $$PWD/Lib/opencv/windows/build/include

        # --- libde265 (frame parallel) ---
        LIBS += -L$$PWD/Lib/libde265/debug -llibde265
        INCLUDEPATH += $$PWD/Lib/libde265/include
        DEPENDPATH  += $$PWD/Lib/libde265/debug

        # --- zLib ---
        LIBS += -L$$PWD/Lib/zlib/build/Debug -lzlib
        INCLUDEPATH += $$PWD/Lib/zlib
        INCLUDEPATH += $$PWD/Lib/zlib/build
        DEPENDPATH  += $$PWD/Lib/zlib/build/Debug

        # --- libheif ---
        LIBS += -L$$PWD/Lib/libheif/debug -llibheif
        INCLUDEPATH += $$PWD/Lib/libheif/include
        DEPENDPATH  += $$PWD/Lib/libheif/debug

        # --- libtiff ---
        LIBS += -L$$PWD/Lib/libtiff/build/libtiff/Debug -ltiff
        INCLUDEPATH += $$PWD/Lib/libtiff/libtiff
        INCLUDEPATH += $$PWD/Lib/libtiff/build/libtiff
        DEPENDPATH  += $$PWD/Lib/libtiff/build/libtiff/Debug

    } else {
        message("🚀 Release build - using release libraries")

        # --- OpenCV (single combined lib build) ---
        LIBS += -L$$PWD/Lib/opencv/windows/build/x64/vc16/lib -lopencv_world4110
        INCLUDEPATH += $$PWD/Lib/opencv/windows/build/include

        # --- libde265 (frame parallel) ---
        LIBS += -L$$PWD/Lib/libde265/release -llibde265
        INCLUDEPATH += $$PWD/Lib/libde265/include
        DEPENDPATH  += $$PWD/Lib/libde265/release

        # --- zLib ---
        LIBS += -L$$PWD/Lib/zlib/build/Release -lzlib
        INCLUDEPATH += $$PWD/Lib/zlib
        INCLUDEPATH += $$PWD/Lib/zlib/build
        DEPENDPATH  += $$PWD/Lib/zlib/build/Release

        # --- libheif ---
        LIBS += -L$$PWD/Lib/libheif/release -llibheif
        INCLUDEPATH += $$PWD/Lib/libheif/include
        DEPENDPATH  += $$PWD/Lib/libheif/release

        # --- libtiff ---
        LIBS += -L$$PWD/Lib/libtiff/build/libtiff/Release -ltiff
        INCLUDEPATH += $$PWD/Lib/libtiff/libtiff
        INCLUDEPATH += $$PWD/Lib/libtiff/build/libtiff
        DEPENDPATH  += $$PWD/Lib/libtiff/build/libtiff/Release
    }
}
