#ifndef PREFERENCESDLG_H
#define PREFERENCESDLG_H

#include <QDialog>
#include <QtWidgets>
#include "Main/global.h"
#include "PropertyEditor/preferences.h"
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

class PreferencesDlg  : public QDialog
{
    Q_OBJECT

public:
    PreferencesDlg(QWidget *parent, bool &isSolo, Preferences *pref, QString &css);
    ~PreferencesDlg() override;

    // Resize the dialog to the tree's font-scaled natural width (and a
    // proportional height, clamped to the screen) so the caption:value column
    // proportions stay constant when the global font size changes.
    void resizeForFontChange();

    /* The reset buttons at the bottom: "Factory reset" (hidden while testing) on the
       left; on the right "Test factory settings" for G::isRory, or "Restore my
       settings" while a test runs.  Called by MW after creating the dialog and from
       MW::rory. */
    void syncResetButtons(bool testing);

signals:
    /* "reset", "test" or "restore": MW::requestSettingsReset (queued -- it may close
       Winnow, which deletes this dialog). */
    void resetSettingsRequested(QString step);

public slots:
    void solo();

private:
    PropertyEditor *tree;
    void expand();
    void collapse();
    QVBoxLayout* layout;
    QPushButton *factoryResetBtn;
    QPushButton *testFactoryBtn;        // or "Restore my settings" while testing
    bool isFactoryTest = false;
    QAction *expandAllAction;
    QAction *collapseAllAction;
    QAction *soloAction;
    bool &isSolo;
};

#endif // PREFERENCESDLG_H
