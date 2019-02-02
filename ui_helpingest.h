/********************************************************************************
** Form generated from reading UI file 'helpingest.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HELPINGEST_H
#define UI_HELPINGEST_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_helpIngest
{
public:
    QVBoxLayout *verticalLayout_2;
    QTextBrowser *textBrowser;

    void setupUi(QWidget *helpIngest)
    {
        if (helpIngest->objectName().isEmpty())
            helpIngest->setObjectName(QString::fromUtf8("helpIngest"));
        helpIngest->setWindowModality(Qt::ApplicationModal);
        helpIngest->resize(1203, 1145);
        verticalLayout_2 = new QVBoxLayout(helpIngest);
        verticalLayout_2->setSpacing(0);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        textBrowser = new QTextBrowser(helpIngest);
        textBrowser->setObjectName(QString::fromUtf8("textBrowser"));
        QFont font;
        font.setPointSize(10);
        textBrowser->setFont(font);
        textBrowser->setFrameShape(QFrame::NoFrame);
        textBrowser->setLineWidth(0);
        textBrowser->setSource(QUrl(QString::fromUtf8("file:///D:/My Projects/Winnow Project/Winnow64/Docs/ingesthelp.htm")));

        verticalLayout_2->addWidget(textBrowser);


        retranslateUi(helpIngest);

        QMetaObject::connectSlotsByName(helpIngest);
    } // setupUi

    void retranslateUi(QWidget *helpIngest)
    {
        helpIngest->setWindowTitle(QApplication::translate("helpIngest", "Templates and tokens explanation", nullptr));
        textBrowser->setHtml(QApplication::translate("helpIngest", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><title>Winnow ingesting help</title><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:10pt; font-weight:400; font-style:normal;\" bgcolor=\"#404040\">\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-weight:600;\">Creating a path to the destination folder</span> </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Example:         Windows:    e:\\2018\\2018-02\\2018-02-23_Spring in Paris<br />                        Mac:            /users/pictures/Feb/23 Paris </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Root: </"
                        "p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">This is the base location where your images are stored.  The root is entered in the \342\200\234Ingest Picks\342\200\235 dialog. </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">            = e:\\2018\\2018-02\\2018-02-23_Spring in Paris<br />            = /users/pictures/Feb/23 Paris </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Template path: </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">The path from the root to the destination folder that can be defined by metadata from an image.  It is based on a template in \342\200\234Edit Template(s)\342\200\235 accessed from the \342\200\234Ingest Picks\342\200\235 dialog. </p>\n"
"<p style"
                        "=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">            = e:\\2018\\2018-02\\2018-02-23_Spring in Paris<br />            = /users/pictures/Feb/23 Paris </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Description: </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">An optional description appended to the template path, entered in the \342\200\234Ingest Picks\342\200\235 dialog. </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">            = e:\\2018\\2018-02\\2018-02-23_Spring in Paris<br />            = /users/pictures/Feb/23 Paris<br /><br /></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">The path = Root + Template "
                        "path + Description<br /><br /></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-weight:600;\">Creating the file names</span> </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">File names are created in the template editor, based on tokens, including sequence tokens that append digits. </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">For example, if the image was taken with a Canon camera on Feb 23, 2018 and was the 36th image picked: </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Template = {YYYY}-{MM}-{DD}_{XXX} will become 2018-02-23_036.CR2<br /><br /></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; "
                        "text-indent:0px;\"><span style=\" font-weight:600;\">What is a template and what are tokens</span> </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">A template is a structure to use to build a file path or a file name.  You can create templates using any descriptive name and create the template using tokens.  Tokens are place holders and the values for the tokens are inserted from the metadata in the images.  For example: </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Template name = \342\200\234Rory template\342\200\235<br />Template = {YYYY}/{YYYY}-{MM}-{DD} </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">{YYYY} = a token (dragged from token list in token editor)<br /><br /></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0"
                        "px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-weight:600;\">Where does the metadata come from</span> </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">The metadata is sourced from the first image picked and all the picked images will be copied to one destination folder. </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-weight:600;\">Slashes and back slashes </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Do not use \342\200\234/\342\200\235 or \342\200\234\\\342\200\235 in the template name.  The template name is used as a key to save the templates between sessions.  Slashes do terrible things to the windows registry. </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-inde"
                        "nt:0; text-indent:0px;\">In the template itself, Windows uses the \342\200\234\\\342\200\235 in file paths while MacOS and Linux use \342\200\234/\342\200\235.  Since the \342\200\234\\\342\200\235 is also used for escape sequences I suggest you use the \342\200\234/\342\200\235.  Winnow will automatically convert this to a \342\200\234\\\342\200\235 when creating the path to the destination folder in windows.  </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">For example, if the image was taken on Feb 23, 2018: </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">            Template = {YYYY}/{YYYY}-{MM}-{DD} will become \342\200\2342018\\2018-02\\2018-02-23\342\200\235 in windows. </p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">  </p></body></html>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class helpIngest: public Ui_helpIngest {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_HELPINGEST_H
