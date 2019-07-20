/********************************************************************************
** Form generated from reading UI file 'helpform.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HELPFORM_H
#define UI_HELPFORM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_helpForm
{
public:
    QGridLayout *gridLayout;
    QVBoxLayout *verticalLayout;
    QTextBrowser *textBrowser;

    void setupUi(QWidget *helpForm)
    {
        if (helpForm->objectName().isEmpty())
            helpForm->setObjectName(QString::fromUtf8("helpForm"));
        helpForm->setWindowModality(Qt::NonModal);
        helpForm->resize(824, 690);
        helpForm->setStyleSheet(QString::fromUtf8("font-size: 14px;"));
        gridLayout = new QGridLayout(helpForm);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        gridLayout->setContentsMargins(0, 0, 0, 0);
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        textBrowser = new QTextBrowser(helpForm);
        textBrowser->setObjectName(QString::fromUtf8("textBrowser"));
        textBrowser->setSource(QUrl(QString::fromUtf8("qrc:/Docs/Winnow help.html")));
        textBrowser->setOpenExternalLinks(false);

        verticalLayout->addWidget(textBrowser);


        gridLayout->addLayout(verticalLayout, 0, 0, 1, 1);


        retranslateUi(helpForm);

        QMetaObject::connectSlotsByName(helpForm);
    } // setupUi

    void retranslateUi(QWidget *helpForm)
    {
        helpForm->setWindowTitle(QCoreApplication::translate("helpForm", "Winnow help", nullptr));
        textBrowser->setHtml(QCoreApplication::translate("helpForm", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><title>Winnow help</title><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:14px; font-weight:400; font-style:normal;\" bgcolor=\"#cccccc\">\n"
"<p style=\" margin-top:16px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt; font-weight:600; color:#333333;\">Overview</span></p>\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:10pt;\">Winnow is an image viewer designed to quickly view raw files and jpgs and &quot;winnow&quot; down to the &quot;picks&quot; that are copied to the users computer from the camera storage medium.  It also serves as a general image viewer and can launch other programs to edi"
                        "t images and it has a simple slideshow feature.<br /><br />Winnow supports the following file types: ARW, BMP, CR2, CUR, DDS, GIF, ICNS, ICO, JPEG, JP2, JPE, MNG, NEF, ORF, PBM, PGM, PNG, PPM, RAF, RW2, SR2, SVG, SVG2, TGA, TIF, WBMP, WEBP, XBM AND XPM. <br /><br />Camera makers supported:  Canon, Fuji, Nikon, Olympus, Panasonic and Sony.</span></p>\n"
"<p style=\" margin-top:16px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt; font-weight:600; color:#333333;\">Contents</span><span style=\" font-size:14pt;\"> </span><span style=\" font-size:8pt;\"><br /></span></p>\n"
"<p style=\" margin-top:16px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt; font-weight:600; color:#333333;\">Quick tips</span></p>\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:10pt;\">Win"
                        "now is can be highly customized which might leave parts &quot;stranded&quot; on secondary monitors that are missing in action.  Use the shortcut \342\207\247\342\214\230W (or in the menu Window &gt; Workspace &gt; Default workspace) to reset to the default workspace.<br /></span></p>\n"
"<p style=\" margin-top:16px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt; font-weight:600; color:#333333;\">Anatomy</span></p>\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:10pt;\">Loupe View:</span><span style=\" font-size:8pt;\"><br /></span><img src=\"../images/Winnow%20Loupe%20View.png\" width=\"800\" height=\"549\" /><span style=\" font-size:8pt;\"><br /></span></p>\n"
"<ol style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 1;\"><li style=\" font-size:8pt;\" style=\" margin-top:12px; margin-bot"
                        "tom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:10pt;\">The folder panel.  Panels (or docks) can be resized, moved to the top, left, right or bottom of the man window, or floated as a separate window.<br /></span></li>\n"
"<li style=\" font-size:10pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14px;\">The fav panel is used to bookmark your favourite folders for quick access.<br /></span></li>\n"
"<li style=\" font-size:10pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14px;\">The metadata panel displays information about the selected image.<br /></span></li>\n"
"<li style=\" font-size:10pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14px;\">The thumbs "
                        "panel shows small thumbnails for all the images in the folder.  If &quot;Include subfolders&quot; has been selected then all subfolder images will also be displayed.<br /></span></li>\n"
"<li style=\" font-size:10pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14px;\">The loupe view, where the image is displayed.  Clicking on the image will zoom to 100% centered on the mouse click location.<br /></span></li>\n"
"<li style=\" font-size:10pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14px;\">The shooting information can be toggled via the shortcut &quot;I&quot;.<br /></span></li>\n"
"<li style=\" font-size:10pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14px;\">The thumbs up icon shows this image has been picke"
                        "d.  The list of images can be filtered to only show picks and only the picks are copied when &quot;Ingest / Copy&quot; is chosen.<br /></span></li>\n"
"<li style=\" font-size:10pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14px;\">This image is the current selected (white border) and has also been picked (green border).<br /></span></li>\n"
"<li style=\" font-size:10pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14px;\">This is the image cache status display.  The entire bar represents all the images in the thumb list.  The green part shows the images that have been cached and the tiny red bar shows where the current selection is situated.  Cached images display much faster.<br /></span></li>\n"
"<li style=\" font-size:10pt;\" style=\" margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt"
                        "-block-indent:0; text-indent:0px;\"><span style=\" font-size:14px;\">The cache status buttons (metadata, thumbs and full size images respectively) turn green when caching is active.   </span></li></ol>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:10pt;\">Grid View:    </span><span style=\" font-size:8pt;\"><br /></span><img src=\"../images/Winnow%20Grid%20View.png\" width=\"800\" height=\"549\" /><span style=\" font-size:8pt;\"><br /></span></p>\n"
"<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-size:8pt;\"><br /></p></body></html>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class helpForm: public Ui_helpForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_HELPFORM_H
