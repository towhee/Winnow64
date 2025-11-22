# petteri_modular.pri
# ---------------------------------------------------------
# Modular version of Petteri-based focus stack engine
# Linked into Winnow as a separate subsystem.
# ---------------------------------------------------------

INCLUDEPATH += $$PWD

# ---- Core ------------------------------------------------
SOURCES += \
    $$PWD/Core/worker.cpp \
    $$PWD/Core/logger.cpp \
    $$PWD/Core/grayscale.cpp

HEADERS += \
    $$PWD/Core/worker.h \
    $$PWD/Core/logger.h \
    $$PWD/Core/grayscale.h

# ---- Denoise ---------------------------------------------
SOURCES += \
    $$PWD/Denoise/denoise.cpp

HEADERS += \
    $$PWD/Denoise/denoise.h \
    $$PWD/Denoise/fast_bilateral.h

# ---- Alignment -------------------------------------------
SOURCES += \
    $$PWD/Align/align.cpp

HEADERS += \
    $$PWD/Align/align.h

# ---- Focus -----------------------------------------------
SOURCES += \
    $$PWD/Focus/focusmeasure.cpp \
    $$PWD/Focus/focusstackworker.cpp

HEADERS += \
    $$PWD/Focus/focusmeasure.h \
    $$PWD/Focus/focusstackworker.h

# ---- Depth -----------------------------------------------
SOURCES += \
    $$PWD/Depth/depthmap.cpp \
    $$PWD/Depth/depthmap_inpaint.cpp

HEADERS += \
    $$PWD/Depth/depthmap.h \
    $$PWD/Depth/depthmap_inpaint.h

# ---- Fusion ----------------------------------------------
SOURCES += \
    $$PWD/Fusion/merge.cpp \
    $$PWD/Fusion/reassign.cpp \
    $$PWD/Fusion/wavelet_opencl.cpp \
    $$PWD/Fusion/wavelet.cpp \
    $$PWD/Fusion/background_removal.cpp \
    $$PWD/Fusion/3dpreview.cpp

HEADERS += \
    $$PWD/Fusion/merge.h \
    $$PWD/Fusion/reassign.h \
    $$PWD/Fusion/wavelet_opencl.h \
    $$PWD/Fusion/wavelet.h \
    $$PWD/Fusion/background_removal.h \
    $$PWD/Fusion/3dpreview.h \
    $$PWD/Fusion/wavelet_templates.h \
    $$PWD/Fusion/wavelet_opencl_kernels.cl

# ---- IO ---------------------------------------------------
SOURCES += \
    $$PWD/IO/loadimg.cpp \
    $$PWD/IO/saveimg.cpp

HEADERS += \
    $$PWD/IO/loadimg.h \
    $$PWD/IO/saveimg.h