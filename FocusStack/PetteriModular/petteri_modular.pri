# petteri_modular.pri
# ---------------------------------------------------------
# Modular version of Petteri-based focus stack engine
# Linked into Winnow as a separate subsystem.
# ---------------------------------------------------------

INCLUDEPATH += $$PWD
INCLUDEPATH += $$PWD/Pipeline
INCLUDEPATH += $$PWD/Core
INCLUDEPATH += $$PWD/Align
INCLUDEPATH += $$PWD/Focus
INCLUDEPATH += $$PWD/Depth
INCLUDEPATH += $$PWD/Fusion
INCLUDEPATH += $$PWD/IO
INCLUDEPATH += $$PWD/Denoise
INCLUDEPATH += $$PWD/Options
INCLUDEPATH += $$PWD/View


HEADERS += FocusStack/PetteriModular/Align/align.h
HEADERS += FocusStack/PetteriModular/Align/background_removal.h
HEADERS += FocusStack/PetteriModular/Align/radialfilter.h
HEADERS += FocusStack/PetteriModular/Core/logger.h
HEADERS += FocusStack/PetteriModular/Core/worker.h
HEADERS += FocusStack/PetteriModular/Core/grayscale.h
HEADERS += FocusStack/PetteriModular/Denoise/denoise.h
HEADERS += FocusStack/PetteriModular/Depth/depthmap.h
HEADERS += FocusStack/PetteriModular/Depth/depthmap_inpaint.h
HEADERS += FocusStack/PetteriModular/Denoise/fast_bilateral.h
HEADERS += FocusStack/PetteriModular/Focus/focusmeasure.h
HEADERS += FocusStack/PetteriModular/Focus/histogrampercentile.h
HEADERS += FocusStack/PetteriModular/Fusion/merge.h
HEADERS += FocusStack/PetteriModular/Fusion/reassign.h
HEADERS += FocusStack/PetteriModular/IO/loadimg.h
HEADERS += FocusStack/PetteriModular/IO/saveimg.h
HEADERS += FocusStack/PetteriModular/Options/options.h
HEADERS += FocusStack/PetteriModular/Pipeline/focusstack.h
HEADERS += FocusStack/PetteriModular/Pipeline/focusstackworker.h
HEADERS += FocusStack/PetteriModular/View/3dpreview.h
HEADERS += FocusStack/PetteriModular/Wave/wavelet.h
HEADERS += FocusStack/PetteriModular/Wave/wavelet_opencl.h
HEADERS += FocusStack/PetteriModular/Wave/wavelet_opencl_kernels.cl
HEADERS += FocusStack/PetteriModular/Wave/wavelet_templates.h

SOURCES += FocusStack/PetteriModular/Align/align.cpp
SOURCES += FocusStack/PetteriModular/Align/background_removal.cpp
SOURCES += FocusStack/PetteriModular/Align/radialfilter.cpp
SOURCES += FocusStack/PetteriModular/Core/logger.cpp
SOURCES += FocusStack/PetteriModular/Core/worker.cpp
SOURCES += FocusStack/PetteriModular/Core/grayscale.cpp
SOURCES += FocusStack/PetteriModular/Depth/depthmap.cpp
SOURCES += FocusStack/PetteriModular/Depth/depthmap_inpaint.cpp
SOURCES += FocusStack/PetteriModular/Denoise/denoise.cpp
SOURCES += FocusStack/PetteriModular/Focus/focusmeasure.cpp
SOURCES += FocusStack/PetteriModular/Focus/histogrampercentile.cpp
SOURCES += FocusStack/PetteriModular/Fusion/merge.cpp
SOURCES += FocusStack/PetteriModular/Fusion/reassign.cpp
SOURCES += FocusStack/PetteriModular/IO/loadimg.cpp
SOURCES += FocusStack/PetteriModular/IO/saveimg.cpp
SOURCES += FocusStack/PetteriModular/Options/options.cpp
SOURCES += FocusStack/PetteriModular/Pipeline/focusstack.cpp
SOURCES += FocusStack/PetteriModular/Pipeline/focusstackworker.cpp
SOURCES += FocusStack/PetteriModular/View/3dpreview.cpp
SOURCES += FocusStack/PetteriModular/Wave/wavelet.cpp
SOURCES += FocusStack/PetteriModular/Wave/wavelet_opencl.cpp

