#ifndef REPORTDIALOG_H
#define REPORTDIALOG_H

#include <QDialog>
#include <QTextBrowser>
#include <QVBoxLayout>

class ReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReportDialog(QWidget *parent = nullptr);

    void setReport(const QString &title, const QString &content);
    QTextBrowser* browser() const { return textBrowser; }

protected:
    bool event(QEvent *e) override;

private:
    QTextBrowser *textBrowser;
};

#endif // REPORTDIALOG_H
