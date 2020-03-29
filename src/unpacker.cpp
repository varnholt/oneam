#include "unpacker.h"

// Qt
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QPixmap>
#include <QTextStream>

// fex
#include "fex.h"

// comicview
#include "book.h"
#include "comicconstants.h"


void error(fex_err_t err)
{
   if (err != nullptr)
   {
      const char* str = fex_err_str( err );

      // log errors
      QFile errorFile("error.log");
      errorFile.open(QFile::Append);
      QTextStream debugOut(&errorFile);
      debugOut << str << "\n";
   }
}


Unpacker::Unpacker(QObject *parent)
 : QObject(parent)
{
}


Unpacker::~Unpacker()
{
   if (mFex)
   {
      fex_close( mFex );
   }
}


void Unpacker::run()
{
   switch (mTask)
   {
      case TaskReadFrontPage:
         readFrontPage();
         break;

      case TaskReadpage:
         readPage();
         break;

      default:
         break;
   }

    emit done();
}


uchar *Unpacker::getPixmap()
{
   return mPixmap;
}


int Unpacker::getPixmapSize()
{
   return mPixmapSize;
}


const QString& Unpacker::getFilename() const
{
   return mFilename;
}


void Unpacker::setFilename(const QString &filename)
{
   mFilename = filename;
}


int Unpacker::getIndex() const
{
   return mIndex;
}


void Unpacker::setIndex(int index)
{
   mIndex = index;
}


Book *Unpacker::getBook() const
{
   return mBook;
}


void Unpacker::setBook(Book *book)
{
   mBook = book;
}


const QString& Unpacker::getPage() const
{
   return mPage;
}


void Unpacker::setPage(const QString &page)
{
   mPage = page;
}


Unpacker::Task Unpacker::getTask() const
{
   return mTask;
}


void Unpacker::setTask(const Task &task)
{
   mTask = task;
}


void Unpacker::readData(const QString &desiredFile)
{
   while ( !fex_done( mFex ) )
   {
      const char* filename = fex_name(mFex);

      if (desiredFile == filename)
      {
         const void* p;
         error( fex_data( mFex, &p ) );

         mPixmap = reinterpret_cast<uchar*>(const_cast<void*>(p));
         mPixmapSize = fex_size(mFex);

         QImage img;
         img.loadFromData(mPixmap, mPixmapSize);

         break;
      }

      error( fex_next( mFex ) );
   }
}


void Unpacker::openFile()
{
   error(fex_open( &mFex, qPrintable(mFilename)));
}


void Unpacker::readFrontPage()
{
   qDebug(
      "processing file: %s",
      qPrintable(mFilename)
   );

   // read archive contents
   QStringList files;

   // try cache info
   QFileInfo baseInfo(mFilename);
   QString baseName = baseInfo.baseName();
   QFile cacheFileInfo(QString("%1/%2.txt").arg(CACHE_DIR).arg(baseName));
   if (cacheFileInfo.exists())
   {
      if (cacheFileInfo.open(QFile::ReadOnly | QIODevice::Text))
      {
         QTextStream in(&cacheFileInfo);
         while (!in.atEnd())
         {
            QString line = in.readLine();
            files << line;
         }
      }
   }
   else
   {
      openFile();

      if (mFex)
      {
         while ( !fex_done( mFex ) )
         {
            const char* filename = fex_name(mFex);

            if (
                  fex_has_extension( filename, ".jpg" )
               || fex_has_extension( filename, ".png" )
            )
            {
               files << filename;
            }

            error( fex_next( mFex ) );
         }

         // nothing to do if no files in there
         if (files.isEmpty())
            return;

         // sort and pick the 1st file afterwards
         qSort(files);

         if (cacheFileInfo.open(QIODevice::WriteOnly | QIODevice::Text))
         {
            QTextStream out(&cacheFileInfo);
            foreach (const QString& file, files)
            {
               out << file << "\n";
            }

            cacheFileInfo.close();
         }
      }
   }

   // store book information
   mBook = new Book();
   mBook->setFilename(mFilename);
   mBook->setPages(files);

   // rewind to start and read file
   QString desiredFile = files.at(0);

   // check cache
   QFile cacheFileImage(QString("%1/%2.jpg").arg(CACHE_DIR).arg(baseName));
   if (cacheFileImage.exists())
   {
      if (cacheFileImage.open(QFile::ReadOnly))
      {
         mPixmap = new uchar[cacheFileImage.size()];
         mPixmapSize = cacheFileImage.size();
         cacheFileImage.read(reinterpret_cast<char*>(mPixmap), cacheFileImage.size());
         cacheFileImage.close();
      }
   }
   else
   {
      openFile();

      if (mFex)
      {
         fex_rewind(mFex);
         readData(desiredFile);
      }
   }
}


void Unpacker::readPage()
{
   openFile();
   readData(getPage());
}

