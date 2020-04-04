// Qt
#include <QApplication>
#include <QDir>
#include <QtDebug>
#include <QImageReader>

// comic
#include "comicconstants.h"
#include "config.h"
#include "mainwidget.h"


namespace
{
const auto version = "0.1";
}


int main(int argc, char *argv[])
{
   QApplication a(argc, argv);

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

   MainWidget w;
   w.setWindowTitle(QString("oneAM %1").arg(version));
   w.showMaximized();

   return a.exec();
}
