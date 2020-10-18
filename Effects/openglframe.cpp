#include "openglframe.h"

OpenGLFrame::OpenGLFrame(QWidget *parent)
{
//    initializeGL();
}

void OpenGLFrame::initializeGL()
{
    qDebug() << __FUNCTION__;
    // Set up the rendering context, load shaders and other resources, etc.:
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void OpenGLFrame::resizeGL(int w, int h)
{
    qDebug() << __FUNCTION__ << "w =" << w << "h =" << h;
//    glViewport(0, 0, w, h);
}

void OpenGLFrame::paintGL()
{
    // this is not being triggered ... why?
    qDebug() << __FUNCTION__;

    glClear(GL_COLOR_BUFFER_BIT);

    QPainter p(this);
    p.drawText(rect(), Qt::AlignCenter, "Testing");
    p.drawEllipse(400, 400, 100, 100);
}
