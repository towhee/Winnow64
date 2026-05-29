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

    // Re-apply the (font-scaled) style to every open help window. Call after
    // G::fontSize changes so already-open help windows track the new app font.
    static void refreshOpenWindows();
protected:
    bool event(QEvent *e) override;
private:
    // Build the font-scaled stylesheet from G::fontSize and (re)render htmlPath.
    void applyStyle();

    QTextBrowser *text;
    QString htmlPath;                       // source doc, kept so we can re-render

    static QList<HtmlWindow*> instances;    // every open help window
};

#endif // HTMLWINDOW_H
