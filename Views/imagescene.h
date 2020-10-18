#ifndef IMAGESCENE_H
#define IMAGESCENE_H

#include <QtWidgets>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QMatrix4x4>
#include <QtGui/QOpenGLShaderProgram>

QT_BEGIN_NAMESPACE
class QPainter;
class QOpenGLContext;
class QOpenGLPaintDevice;
QT_END_NAMESPACE

#include "Main/global.h"
#include "Effects/openglframe.h"

class ImageScene : public QGraphicsScene, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    ImageScene(OpenGLFrame *openGLFrame);
    OpenGLFrame *openGLFrame;

protected:
    void drawForeground(QPainter *painter, const QRectF &rect) override;
};

#endif // IMAGESCENE_H
