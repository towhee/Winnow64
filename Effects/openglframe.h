#ifndef OPENGLFRAME_H
#define OPENGLFRAME_H

#include <QtWidgets>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>

class OpenGLFrame : public QOpenGLWidget, protected QOpenGLFunctions
{
public:
    OpenGLFrame(QWidget* parent = nullptr);
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;};

#endif // OPENGLFRAME_H
