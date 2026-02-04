#include "fsfusion.h"

#include <QDebug>

void FSFusion::resetGeometry()
{
    alignSize = {};
    padSize = {};
    origSize = {};
    validAreaAlign = {};
    outDepth = CV_8U;
}

bool FSFusion::hasValidGeometry() const
{
    return alignSize.width  > 0 && alignSize.height  > 0 &&
           origSize.width   > 0 && origSize.height   > 0 &&
           validAreaAlign.width  > 0 && validAreaAlign.height > 0 &&
           rectInside(validAreaAlign, alignSize);
}

QString FSFusion::geometryText() const
{
    return QString("align=%1 pad=%2 orig=%3 validAreaAlign=%4 outDepth=%5")
        .arg(cvSizeToText(alignSize))
        .arg(cvSizeToText(padSize))
        .arg(cvSizeToText(origSize))
        .arg(cvRectToText(validAreaAlign))
        .arg(outDepth);
}
