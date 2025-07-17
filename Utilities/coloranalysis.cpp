#include "coloranalysis.h"
#include "Main/global.h"
#include "Effects/effects.h"

ColorAnalysis::ColorAnalysis(QObject *)
{

}

void ColorAnalysis::process(QStringList &fPathList)
{
    if (G::isLogger) G::log("ColorAnalysis::process");

    int count = fPathList.length();
    G::isRunningColorAnalysis = true;
    G::popup->setProgressVisible(true);
    G::popup->setProgressMax(count);
    QString txt = "Generating hue count for " + QString::number(count) + " images. " +
                  "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popup->showPopup(txt, 0, true, 1);

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
        if (img.isNull()) {
            rpt << "INVALID FILE FORMAT (ONLY JPG, PNG, TIF)   " << fPathList.at(f) << "\n";
            continue;
        }
        effects.hueCount(img, hues);
        G::popup->setProgress(f+1);
        if (G::useProcessEvents) qApp->processEvents();
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

    G::popup->setProgressVisible(false);
    G::popup->reset();
    if (abort) return;

    QTextBrowser *tb = new QTextBrowser;
    QDialog *dlg = new QDialog;
    dlg->setMinimumSize(1200, 800);
    dlg->setStyleSheet(G::css);
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
    G::popup->setProgressVisible(false);
    G::popup->reset();
    G::popup->showPopup("Hue report has been aborted.");
    qDebug() << "ColorAnalysis::abortHueReport" << abort;
    // if (G::useProcessEvents) qApp->processEvents();
}
