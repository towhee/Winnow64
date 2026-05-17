#ifndef HTMLWINDOW_H
#define HTMLWINDOW_H

#include <QtWidgets>

class HtmlWindow : public QScrollArea
{
    Q_OBJECT
public:
    HtmlWindow(const QString &title,
               const QString &htmlPath,
               const QSize &size,
               const QRect mwRect = QRect(),
               QWidget *parent = nullptr);
    ~HtmlWindow() override;
protected:
    bool event(QEvent *e) override;
private:
    QTextBrowser *text;
};

#endif // HTMLWINDOW_H
