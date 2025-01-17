#ifndef INGESTERRORS_H
#define INGESTERRORS_H

#include <QDialog>
#include <QTextDocument>
#include <QTextEdit>
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

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
    ~IngestErrors() override;

private:
    Ui::IngestErrors *ui;
};

#endif // INGESTERRORS_H
