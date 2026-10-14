#ifndef VERSIONCOLLECTIONSDLG_H
#define VERSIONCOLLECTIONSDLG_H

#include <QDialog>
#include <QHash>
#include <QVector>

class QListWidget;

/*
    "New Version" for an image that is in collections: which of those collections should
    the new version join as well? (MW::newVersion.)

    A version is a collection member in its own right (Datamodel/collectionstore.h), so
    it is in none until it is added -- this is where that is decided, with every
    collection its image is in listed and ticked. With several images selected, each
    collection shows how many of them it holds, and a ticked collection takes only the
    versions of the images it already holds.

    OK makes the versions; Cancel makes none.
*/
class VersionCollectionsDlg : public QDialog
{
public:
    /*  offered: the collection ids to list, in tree order (CollectionStore::nodes);
        imagesIn: how many of the selected images each holds; images: how many images
        are getting a version. */
    VersionCollectionsDlg(const QVector<qint64> &offered,
                          const QHash<qint64, int> &imagesIn,
                          int images, QWidget *parent = nullptr);

    QVector<qint64> checkedIds() const;

private:
    QListWidget *list = nullptr;
};

#endif // VERSIONCOLLECTIONSDLG_H
