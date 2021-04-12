#ifndef INGESTERRORS_H
#define INGESTERRORS_H

#include <QDialog>
#include <QTextDocument>
#include <QTextEdit>
#include "Main/global.h"

namespace Ui {
class IngestErrors;
}

class IngestErrors : public QDialog
{
    Q_OBJECT

public:
    explicit IngestErrors(const QStringList &failedToCopy,
                          const QStringList &integrityFailure,
                          QWidget *parent = nullptr);
    ~IngestErrors();

private:
    Ui::IngestErrors *ui;
};

#endif // INGESTERRORS_H
