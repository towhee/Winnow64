#ifndef APPLECAMERASDLG_H
#define APPLECAMERASDLG_H

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;

/*
    "View cameras supported" (Develop > Raw, shown when Render using is an Apple
    decoder): the camera models Core Image lists for each Apple RAW decoder version, so
    the user can see which version supports a body before choosing one.

    A version dropdown, a filter box and the list. Per version on macOS 27+; older
    systems have a single all-versions list and the dropdown says so. The data is
    AppleRawDecode::SupportedCameraModels -- Apple's own list, which names models its
    own way ("Nikon D850", "Sony Alpha ILCE-9M2") and is a guide rather than a promise
    for a given file; the note under the list says that.
*/
class AppleCamerasDlg : public QDialog
{
public:
    /* initialVersion: the version to open on (the one the current image uses); 0 or a
       version this macOS lacks opens on the newest. */
    explicit AppleCamerasDlg(int initialVersion, QWidget *parent = nullptr);

private:
    void loadVersion();
    void applyFilter();

    QComboBox   *versionCombo = nullptr;
    QLineEdit   *filterEdit   = nullptr;
    QListWidget *list         = nullptr;
    QLabel      *countLabel   = nullptr;
};

#endif // APPLECAMERASDLG_H
