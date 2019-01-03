/********************************************************************************
** Form generated from reading UI file 'helpingest.ui'
**
** Created by: Qt User Interface Compiler version 5.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HELPINGEST_H
#define UI_HELPINGEST_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
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
            helpIngest->setObjectName(QStringLiteral("helpIngest"));
        helpIngest->setWindowModality(Qt::ApplicationModal);
        helpIngest->resize(900, 777);
        verticalLayout_2 = new QVBoxLayout(helpIngest);
        verticalLayout_2->setSpacing(0);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        textBrowser = new QTextBrowser(helpIngest);
        textBrowser->setObjectName(QStringLiteral("textBrowser"));
        QFont font;
        font.setPointSize(10);
        textBrowser->setFont(font);
        textBrowser->setFrameShape(QFrame::NoFrame);
        textBrowser->setLineWidth(0);
        textBrowser->setSource(QUrl(QStringLiteral("file:///D:/My Projects/Winnow Project/Winnow64/Docs/ingesthelp.htm")));

        verticalLayout_2->addWidget(textBrowser);


        retranslateUi(helpIngest);

        QMetaObject::connectSlotsByName(helpIngest);
    } // setupUi

    void retranslateUi(QWidget *helpIngest)
    {
        helpIngest->setWindowTitle(QApplication::translate("helpIngest", "Templates and tokens explanation", nullptr));
        textBrowser->setHtml(QApplication::translate("helpIngest", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:10pt; font-weight:400; font-style:normal;\" bgcolor=\"#404040\">\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt; color:#ffffff;\">Creating a path to the destination folder</span><span style=\" color:#bebebe;\"> </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">Example:         Windows:    e:\\2018\\2018-02\\2018-02-23_Spring in Paris<br />                        Mac:            /users/pictures/Feb/23 Paris </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-ri"
                        "ght:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#c8c804;\">Root</span><span style=\" color:#bebebe;\">: </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">This is the base location where your images are stored.  The root is entered in the \342\200\234Ingest Picks\342\200\235 dialog. </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">            = </span><span style=\" color:#c8c804;\">e:\\</span><span style=\" color:#bebebe;\">2018\\2018-02\\2018-02-23_Spring in Paris<br />            = </span><span style=\" color:#c8c804;\">/users/pictures/</span><span style=\" color:#bebebe;\">Feb/23 Paris </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#c8c8"
                        "04;\">Template path</span><span style=\" color:#bebebe;\">: </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">The path from the root to the destination folder that can be defined by metadata from an image.  It is based on a template in \342\200\234Edit Template(s)\342\200\235 accessed from the \342\200\234Ingest Picks\342\200\235 dialog. </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">            = e:\\</span><span style=\" color:#c8c804;\">2018\\2018-02\\2018-02-23</span><span style=\" color:#bebebe;\">_Spring in Paris<br />            = /users/pictures/</span><span style=\" color:#c8c804;\">Feb/23</span><span style=\" color:#bebebe;\"> Paris </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-inde"
                        "nt:0px;\"><span style=\" color:#c8c804;\">Description</span><span style=\" color:#bebebe;\">: </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">An optional description appended to the template path, entered in the \342\200\234Ingest Picks\342\200\235 dialog. </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">            = e:\\2018\\2018-02\\2018-02-23</span><span style=\" color:#c8c804;\">_Spring in Paris</span><span style=\" color:#bebebe;\"><br />            = /users/pictures/Feb/23</span><span style=\" color:#c8c804;\"> Paris<br /></span><span style=\" color:#bebebe;\"><br /></span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">The path = Root + Template p"
                        "ath + Description<br /><br /></span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt; color:#ffffff;\">Creating the file names</span><span style=\" color:#bebebe;\"> </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">File names are created in the template editor, based on tokens, including sequence tokens that append digits. </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">For example, if the image was taken with a Canon camera on Feb 23, 2018 and was the 36th image picked: </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">Template = {YYYY}-{M"
                        "M}-{DD}_{XXX} will become 2018-02-23_036.CR2<br /><br /></span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt; color:#ffffff;\">What is a template and what are tokens </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">A template is a structure to use to build a file path or a file name.  You can create templates using any descriptive name and create the template using tokens.  Tokens are place holders and the values for the tokens are inserted from the metadata in the images.  For example: </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">Template name = \342\200\234Rory template\342\200\235<br />Template = {YYYY}/{YYYY}-{MM}-{DD} </span></p>\n"
"<p style=\" ma"
                        "rgin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">{YYYY} = a token (dragged from token list in token editor)<br /><br /></span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt; color:#ffffff;\">Where does the metadata come from</span><span style=\" color:#bebebe;\"> </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">The metadata is sourced from the first image picked and all the picked images will be copied to one destination folder. <br /></span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt; color:#ffffff;\">Slashes and back slashes</span><span style=\" font-weight:600; color:#"
                        "bebebe;\"> </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">Do not use \342\200\234/\342\200\235 or \342\200\234\\\342\200\235 in the template name.  The template name is used as a key to save the templates between sessions.  Slashes do terrible things to the windows registry. </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">In the template itself, Windows uses the \342\200\234\\\342\200\235 in file paths while MacOS and Linux use \342\200\234/\342\200\235.  Since the \342\200\234\\\342\200\235 is also used for escape sequences I suggest you use the \342\200\234/\342\200\235.  Winnow will automatically convert this to a \342\200\234\\\342\200\235 when creating the path to the destination folder in windows.  </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px;"
                        " margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">For example, if the image was taken on Feb 23, 2018: </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">            Template = {YYYY}/{YYYY}-{MM}-{DD} will become \342\200\2342018\\2018-02\\2018-02-23\342\200\235 in windows. </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" color:#bebebe;\">  </span></p></body></html>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class helpIngest: public Ui_helpIngest {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_HELPINGEST_H
