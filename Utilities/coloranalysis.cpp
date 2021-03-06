#include "coloranalysis.h"
#include "Effects/effects.h"

ColorAnalysis::ColorAnalysis(QObject *parent)/* : fPath(fPath)*/
{
}

void ColorAnalysis::process(QStringList &fPathList)
{
    int count = fPathList.length();
    G::isRunningColorAnalysis = true;
    G::popUp->setProgressVisible(true);
    G::popUp->setProgressMax(count);
    QString txt = "Counting hues in " + QString::number(count) + " images. " +
                  "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popUp->showPopup(txt, 0, true, 1);

    QVector<int> hues(360, 0);
    Effects effects;
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << "Winnow Colour Analysis\n";
    rpt << "\n";
    rpt << "Hue    Count      Total    File\n";
    for (int f = 0; f < count; f++) {
        QImage img(fPathList.at(f));
        effects.hueCount(img, hues);
        G::popUp->setProgress(f+1);
        qApp->processEvents();
        if (abort) return;
        for (int i = 0; i < hues.size(); i++) {
            rpt << QString::number(i).rightJustified(3);
            rpt << QString::number(hues[i]).rightJustified(9);
            rpt << QString::number(img.width()*img.height()).rightJustified(11);
            rpt << "    ";
            rpt << fPathList.at(f);
//            rpt << QString::number(f).rightJustified(9);
            rpt << "\n";
        }
    }

    G::popUp->setProgressVisible(false);
    G::popUp->hide();
    if (abort) return;

    QTextBrowser *tb = new QTextBrowser;
    QDialog *dlg = new QDialog;
    dlg->setMinimumSize(800, 600);
    QVBoxLayout *layout = new QVBoxLayout;
    dlg->setLayout(layout);
    layout->addWidget(tb);
    tb->setText(reportString);
    tb->setFont(QFont("Courier", 10));
    tb->setWordWrapMode(QTextOption::NoWrap);
    dlg->show();
}

void ColorAnalysis::abortHueReport()
{
    abort = true;
    G::isRunningColorAnalysis = false;
    G::popUp->setProgressVisible(false);
    G::popUp->hide();
    G::popUp->showPopup("Hue report has been aborted.");
    qDebug() << __FUNCTION__ << abort;
    qApp->processEvents();
}
