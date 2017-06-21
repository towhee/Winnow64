

#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication QApp(argc, argv);
//   	if (QCoreApplication::arguments().size() > 2)
//   	{
//        qDebug() << QObject::tr("Usage: Winnow [FILE or DIRECTORY]...");
//   		return -1;
//	}

//	QTranslator qtTranslator;

//	qtTranslator.load("qt_" + QLocale::system().name(),
//					QLibraryInfo::location(QLibraryInfo::TranslationsPath));
//	QApp.installTranslator(&qtTranslator);

//	QTranslator phototonicTranslator;
//    phototonicTranslator.load(":/translations/winnow_" + QLocale::system().name());
//	QApp.installTranslator(&phototonicTranslator);

    MW MW;
    MW.show();
    return QApp.exec();
}
