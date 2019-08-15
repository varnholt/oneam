// Qt
#include <QApplication>
#include <QDir>
#include <QtDebug>
#include <QImageReader>

// comic
#include "comicconstants.h"
#include "config.h"
#include "mainwidget.h"


int main(int argc, char *argv[])
{
   QString version = "0.1";

   QApplication a(argc, argv);

//   QString sDir = QCoreApplication::applicationDirPath();
//   a.addLibraryPath(sDir+"/plugins");
//
//   qDebug() << "Reader:" << QImageReader::supportedImageFormats();

   // make cache dir
   QDir cache;
   cache.mkdir(
      QString("%1/%2/%3")
         .arg(QDir::home().absolutePath())
         .arg(HOME_DIR)
         .arg(CACHE_DIR)
      );

   QString requestedBook;
   if (argc > 1)
   {
      QString path = argv[1];

      if (path.trimmed() == ".")
         path = QDir::currentPath();

      QFileInfo fi(path);

      if (QFileInfo(fi.absoluteFilePath()).isDir())
         Config::getInstance()->setPath(fi.absoluteFilePath());
      else
         Config::getInstance()->setPath(fi.absolutePath());

      if (fi.isFile())
      {
         requestedBook = fi.baseName();
         Config::getInstance()->setRequestedBook(requestedBook);
      }
   }

   // PreviewWidget w;
   MainWidget w;
   w.show();
   w.setWindowTitle(QString("oneAM %1").arg(version));

   int width = 650;
   w.resize(width, width * PAGE_RATIO);

   return a.exec();
}
