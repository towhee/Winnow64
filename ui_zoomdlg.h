/********************************************************************************
** Form generated from reading UI file 'zoomdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ZOOMDLG_H
#define UI_ZOOMDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_ZoomDlg
{
public:
    QFrame *border;
    QHBoxLayout *horizontalLayout_3;
    QVBoxLayout *verticalLayout;
    QSpacerItem *verticalSpacer;
    QSlider *zoomSlider;
    QHBoxLayout *horizontalLayout_2;
    QRadioButton *radio25Button;
    QRadioButton *radio50Button;
    QRadioButton *radio66Button;
    QRadioButton *radio100Button;
    QRadioButton *radio133Button;
    QRadioButton *radio200Button;
    QSpacerItem *verticalSpacer_2;
    QHBoxLayout *horizontalLayout;
    QSpinBox *zoomSB;
    QLabel *pctLabel;
    QPushButton *toggleZoomAmountBtn;

    void setupUi(QDialog *ZoomDlg)
    {
        if (ZoomDlg->objectName().isEmpty())
            ZoomDlg->setObjectName(QString::fromUtf8("ZoomDlg"));
        ZoomDlg->resize(705, 69);
        ZoomDlg->setWindowOpacity(0.850000000000000);
        border = new QFrame(ZoomDlg);
        border->setObjectName(QString::fromUtf8("border"));
        border->setGeometry(QRect(0, 0, 705, 69));
        border->setFrameShape(QFrame::Box);
        horizontalLayout_3 = new QHBoxLayout(border);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        zoomSlider = new QSlider(border);
        zoomSlider->setObjectName(QString::fromUtf8("zoomSlider"));
        zoomSlider->setMaximumSize(QSize(16777215, 10));
        zoomSlider->setFocusPolicy(Qt::StrongFocus);
        zoomSlider->setMinimum(1);
        zoomSlider->setMaximum(200);
        zoomSlider->setValue(100);
        zoomSlider->setOrientation(Qt::Horizontal);
        zoomSlider->setTickPosition(QSlider::TicksAbove);
        zoomSlider->setTickInterval(25);

        verticalLayout->addWidget(zoomSlider);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        radio25Button = new QRadioButton(border);
        radio25Button->setObjectName(QString::fromUtf8("radio25Button"));
        QFont font;
        font.setPointSize(11);
        radio25Button->setFont(font);

        horizontalLayout_2->addWidget(radio25Button);

        radio50Button = new QRadioButton(border);
        radio50Button->setObjectName(QString::fromUtf8("radio50Button"));
        radio50Button->setFont(font);

        horizontalLayout_2->addWidget(radio50Button);

        radio66Button = new QRadioButton(border);
        radio66Button->setObjectName(QString::fromUtf8("radio66Button"));
        radio66Button->setFont(font);

        horizontalLayout_2->addWidget(radio66Button);

        radio100Button = new QRadioButton(border);
        radio100Button->setObjectName(QString::fromUtf8("radio100Button"));
        radio100Button->setFont(font);

        horizontalLayout_2->addWidget(radio100Button);

        radio133Button = new QRadioButton(border);
        radio133Button->setObjectName(QString::fromUtf8("radio133Button"));
        radio133Button->setFont(font);

        horizontalLayout_2->addWidget(radio133Button);

        radio200Button = new QRadioButton(border);
        radio200Button->setObjectName(QString::fromUtf8("radio200Button"));
        radio200Button->setFont(font);

        horizontalLayout_2->addWidget(radio200Button);


        verticalLayout->addLayout(horizontalLayout_2);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_2);


        horizontalLayout_3->addLayout(verticalLayout);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        zoomSB = new QSpinBox(border);
        zoomSB->setObjectName(QString::fromUtf8("zoomSB"));
        zoomSB->setMinimumSize(QSize(0, 0));
        zoomSB->setMaximumSize(QSize(16777215, 40));
        QFont font1;
        font1.setPointSize(13);
        font1.setBold(true);
        font1.setWeight(75);
        zoomSB->setFont(font1);
        zoomSB->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        zoomSB->setMinimum(1);
        zoomSB->setMaximum(400);
        zoomSB->setSingleStep(10);
        zoomSB->setDisplayIntegerBase(10);

        horizontalLayout->addWidget(zoomSB);

        pctLabel = new QLabel(border);
        pctLabel->setObjectName(QString::fromUtf8("pctLabel"));
        pctLabel->setMaximumSize(QSize(16777215, 20));
        pctLabel->setLineWidth(0);

        horizontalLayout->addWidget(pctLabel);

        toggleZoomAmountBtn = new QPushButton(border);
        toggleZoomAmountBtn->setObjectName(QString::fromUtf8("toggleZoomAmountBtn"));
        toggleZoomAmountBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(toggleZoomAmountBtn);


        horizontalLayout_3->addLayout(horizontalLayout);


        retranslateUi(ZoomDlg);
        QObject::connect(zoomSlider, SIGNAL(valueChanged(int)), zoomSB, SLOT(setValue(int)));

        QMetaObject::connectSlotsByName(ZoomDlg);
    } // setupUi

    void retranslateUi(QDialog *ZoomDlg)
    {
        ZoomDlg->setWindowTitle(QApplication::translate("ZoomDlg", "Select zoom percentage", nullptr));
#ifndef QT_NO_TOOLTIP
        ZoomDlg->setToolTip(QApplication::translate("ZoomDlg", "<html><head/><body><p><span style=\" font-size:18pt; font-weight:600;\">Zoom tips:</span></p><p><span style=\" font-weight:600;\">Z </span>opens and closes the zoom window.</p><p><span style=\" font-weight:600;\">Enter </span>or <span style=\" font-weight:600;\">Esc </span>will close this window.</p><p><span style=\" font-weight:600;\">1</span> - <span style=\" font-weight:600;\">6</span> to quick set zoom percentage.</p><p><span style=\" font-weight:600;\">Use toggle zoom buttom</span> to change the toggle zoom amount (when clicking on the main image).  The zoom will alternate between <span style=\" font-weight:600;\">fill</span> and the <span style=\" font-weight:600;\">toggle zoom amount</span> you have set here each time you mouse click on the image.</p><p>This window will stay-on-top so you can scroll the image, click zoom or pan to click point on a thumbnail.</p><p>Settings are sticky and will remain when you use Esc to exit.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        radio25Button->setToolTip(QApplication::translate("ZoomDlg", "Shortcut = 1", nullptr));
#endif // QT_NO_TOOLTIP
        radio25Button->setText(QApplication::translate("ZoomDlg", "25% (1)", nullptr));
#ifndef QT_NO_SHORTCUT
        radio25Button->setShortcut(QApplication::translate("ZoomDlg", "1", nullptr));
#endif // QT_NO_SHORTCUT
#ifndef QT_NO_TOOLTIP
        radio50Button->setToolTip(QApplication::translate("ZoomDlg", "Shortcut = 2", nullptr));
#endif // QT_NO_TOOLTIP
        radio50Button->setText(QApplication::translate("ZoomDlg", "50% (2)", nullptr));
#ifndef QT_NO_SHORTCUT
        radio50Button->setShortcut(QApplication::translate("ZoomDlg", "2", nullptr));
#endif // QT_NO_SHORTCUT
#ifndef QT_NO_TOOLTIP
        radio66Button->setToolTip(QApplication::translate("ZoomDlg", "Shortcut = 3", nullptr));
#endif // QT_NO_TOOLTIP
        radio66Button->setText(QApplication::translate("ZoomDlg", "67% (3)", nullptr));
#ifndef QT_NO_SHORTCUT
        radio66Button->setShortcut(QApplication::translate("ZoomDlg", "3", nullptr));
#endif // QT_NO_SHORTCUT
#ifndef QT_NO_TOOLTIP
        radio100Button->setToolTip(QApplication::translate("ZoomDlg", "Shortcut = 4", nullptr));
#endif // QT_NO_TOOLTIP
        radio100Button->setText(QApplication::translate("ZoomDlg", "100% (4)", nullptr));
#ifndef QT_NO_SHORTCUT
        radio100Button->setShortcut(QApplication::translate("ZoomDlg", "4", nullptr));
#endif // QT_NO_SHORTCUT
#ifndef QT_NO_TOOLTIP
        radio133Button->setToolTip(QApplication::translate("ZoomDlg", "Shortcut = 5", nullptr));
#endif // QT_NO_TOOLTIP
        radio133Button->setText(QApplication::translate("ZoomDlg", "133% (5)", nullptr));
#ifndef QT_NO_SHORTCUT
        radio133Button->setShortcut(QApplication::translate("ZoomDlg", "5", nullptr));
#endif // QT_NO_SHORTCUT
#ifndef QT_NO_TOOLTIP
        radio200Button->setToolTip(QApplication::translate("ZoomDlg", "Shortcut = 6", nullptr));
#endif // QT_NO_TOOLTIP
        radio200Button->setText(QApplication::translate("ZoomDlg", "200% (6)", nullptr));
#ifndef QT_NO_SHORTCUT
        radio200Button->setShortcut(QApplication::translate("ZoomDlg", "6", nullptr));
#endif // QT_NO_SHORTCUT
        pctLabel->setText(QApplication::translate("ZoomDlg", "%", nullptr));
        toggleZoomAmountBtn->setText(QApplication::translate("ZoomDlg", "Set as toggle \n"
"zoom amount", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ZoomDlg: public Ui_ZoomDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ZOOMDLG_H
