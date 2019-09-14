/********************************************************************************
** Form generated from reading UI file 'ingestautopath.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_INGESTAUTOPATH_H
#define UI_INGESTAUTOPATH_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_ingestAutoPath
{
public:
    QVBoxLayout *verticalLayout_2;
    QFrame *frame;
    QVBoxLayout *verticalLayout;
    QLabel *label;

    void setupUi(QDialog *ingestAutoPath)
    {
        if (ingestAutoPath->objectName().isEmpty())
            ingestAutoPath->setObjectName(QString::fromUtf8("ingestAutoPath"));
        ingestAutoPath->resize(640, 480);
        verticalLayout_2 = new QVBoxLayout(ingestAutoPath);
        verticalLayout_2->setSpacing(0);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        frame = new QFrame(ingestAutoPath);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        verticalLayout = new QVBoxLayout(frame);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        label = new QLabel(frame);
        label->setObjectName(QString::fromUtf8("label"));
        label->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);

        verticalLayout->addWidget(label);


        verticalLayout_2->addWidget(frame);


        retranslateUi(ingestAutoPath);

        QMetaObject::connectSlotsByName(ingestAutoPath);
    } // setupUi

    void retranslateUi(QDialog *ingestAutoPath)
    {
        ingestAutoPath->setWindowTitle(QCoreApplication::translate("ingestAutoPath", "Dialog", nullptr));
        label->setText(QCoreApplication::translate("ingestAutoPath", "<html><head/><body><h3 style=\" margin-top:14px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:large; font-weight:600;\">Creating a path to the destination folder</span></h3><p>Example: </p><p>Windows: e:\\2018\\2018-02\\2018-02-23_Spring in Paris </p><p>Mac: /users/pictures/Feb/23 Paris Root: </p><p>This is the base location where your images are stored. </p><p>The root is entered in the \342\200\234Ingest Picks\342\200\235 dialog. = e:\\2018\\2018-02\\2018-02-23_Spring in Paris = /users/pictures/Feb/23 Paris Template path: The path from the root to the destination folder that can be defined by metadata from an image. It is based on a template in \342\200\234Edit Template(s)\342\200\235 accessed from the \342\200\234Ingest Picks\342\200\235 dialog. = e:\\2018\\2018-02\\2018-02-23_Spring in Paris = /users/pictures/Feb/23 Paris Description: An optional description appended to the template path, entered in the \342\200\234Ingest Picks\342\200"
                        "\235 dialog. = e:\\2018\\2018-02\\2018-02-23_Spring in Paris = /users/pictures/Feb/23 Paris The path = Root + Template path + Description Creating the file names File names are created in the template editor, based on tokens, including sequence tokens that append digits. For example, if the image was taken with a Canon camera on Feb 23, 2018 and was the 36th image picked: Template = {YYYY}-{MM}-{DD}_{XXX} will become 2018-02-23_036.CR2 What is a template and what are tokens A template is a structure to use to build a file path or a file name. You can create templates using any descriptive name and create the template using tokens. Tokens are place holders and the values for the tokens are inserted from the metadata in the images. </p><p>For example: Template name = \342\200\234Rory template\342\200\235 Template = {YYYY}/{YYYY}-{MM}-{DD} {YYYY} = a token (dragged from token list in token editor) Where does the metadata come from The metadata is sourced from the first image picked and all the picked images will "
                        "be copied to one destination folder. </p><h3 style=\" margin-top:14px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:large; font-weight:600;\">Slashes and back slashes </span></h3><p>Do not use \342\200\234/\342\200\235 or \342\200\234\\\342\200\235 in the template name. The template name is used as a key to save the templates between sessions. Slashes do terrible things to the windows registry. In the template itself, Windows uses the \342\200\234\\\342\200\235 in file paths while MacOS and Linux use \342\200\234/\342\200\235. Since the \342\200\234\\\342\200\235 is also used for escape sequences I suggest you use the \342\200\234/\342\200\235. Winnow will automatically convert this to a \342\200\234\\\342\200\235 when creating the path to the destination folder in windows. </p><p>For example, if the image was taken on Feb 23, 2018: Template = {YYYY}/{YYYY}-{MM}-{DD} will become \342\200\2342018\\2018-02\\2018-02-23\342\200\235 in window"
                        "s. </p></body></html>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ingestAutoPath: public Ui_ingestAutoPath {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_INGESTAUTOPATH_H
