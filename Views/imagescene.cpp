#include "imagescene.h"
#include <QtCore/QCoreApplication>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLPaintDevice>
#include <QtGui/QPainter>

ImageScene::ImageScene(OpenGLFrame *openGLFrame)
{
    this->openGLFrame = openGLFrame;
}

void ImageScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    qDebug() << __FUNCTION__ << "painter->paintEngine()->type()" << painter->paintEngine()->type();
    if (painter->paintEngine()->type() == QPaintEngine::OpenGL2) {
        painter->save();
        painter->setPen(QPen(Qt::red));
        painter->drawRect(QRect(100, 100, 100, 100));

        painter->beginNativePainting();

        qDebug() << __FUNCTION__ << "painter->paintEngine()->type() == QPaintEngine::OpenGL2";
        painter->setPen(QPen(Qt::yellow));
        painter->drawRect(QRect(200, 200, 100, 100));

//        openGLFrame->makeCurrent();

        // start open raw OpenGL ...
        static const char *vertexShaderSource =
            "attribute highp vec4 posAttr;\n"
            "attribute lowp vec4 colAttr;\n"
            "varying lowp vec4 col;\n"
            "uniform highp mat4 matrix;\n"
            "void main() {\n"
            "   col = colAttr;\n"
            "   gl_Position = matrix * posAttr;\n"
            "}\n";

        static const char *fragmentShaderSource =
            "varying lowp vec4 col;\n"
            "void main() {\n"
            "   gl_FragColor = col;\n"
            "}\n";

        QSurfaceFormat format;
        format.setSamples(16);

        QOpenGLContext *m_context = nullptr;
        QOpenGLPaintDevice *m_device = nullptr;

        qDebug() << __FUNCTION__ << "1";
        int m_frame;
        auto m_program = new QOpenGLShaderProgram(this);
        m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
        m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
        m_program->link();
        auto m_posAttr = m_program->attributeLocation("posAttr");
        auto m_colAttr = m_program->attributeLocation("colAttr");
        auto m_matrixUniform = m_program->uniformLocation("matrix");

//        glViewport(0, 0, 400, 500);

//        glClear(GL_COLOR_BUFFER_BIT);

        m_program->bind();

        QMatrix4x4 matrix;
        matrix.perspective(60.0f, 4.0f/3.0f, 0.1f, 100.0f);
        matrix.translate(0, 0, -2);
        matrix.rotate(20.0f, 0, 1, 0);

        m_program->setUniformValue(m_matrixUniform, matrix);

        GLfloat vertices[] = {
            0.0f, 0.707f,
            -0.5f, -0.5f,
            0.5f, -0.5f
        };

        GLfloat colors[] = {
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 1.0f
        };
        qDebug() << __FUNCTION__ << vertices << colors << m_posAttr << m_colAttr;
        /*  crashes here
        glVertexAttribPointer(m_posAttr, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glVertexAttribPointer(m_colAttr, 3, GL_FLOAT, GL_FALSE, 0, colors);

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);

        m_program->release();
        */

        painter->endNativePainting();
        painter->restore();

    } else {
        QGraphicsScene::drawForeground(painter, rect);
        /* nada
        painter->save();
        painter->setPen(QPen(Qt::red));
        painter->drawRect(QRect(100, 100, 100, 100));
        painter->restore();
        */
    }
}
