#include "unpacker.h"

// Qt
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QPixmap>
#include <QTextStream>

#include <iostream>

// comicview
#include "book.h"
#include "config.h"
#include "comicconstants.h"

#include "bitarchiveinfo.hpp"
#include "bitextractor.hpp"
#include "bitexception.hpp"


Unpacker::Unpacker(QObject *parent)
 : QObject(parent)
{
}



void Unpacker::run()
{
   switch (mTask)
   {
      case TaskReadFrontPage:
      {
         readFrontPage();
         break;
      }

      case TaskReadpage:
      {
         readPage();
         break;
      }

      default:
      {
         break;
      }
   }

   emit done();
}


const std::vector<uint8_t>& Unpacker::getPixmap()
{
   return mData;
}


size_t Unpacker::getPixmapSize()
{
   return mData.size();
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


int32_t Unpacker::getPage() const
{
   return mPage;
}


void Unpacker::setPage(int32_t page)
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


const QPixmap& Unpacker::getCover() const
{
   return mCover;
}


bool Unpacker::isValid() const
{
   return mValid;
}


void Unpacker::readData(int32_t page = 0)
{
   std::cout << "reading page " << page << " from " << mFilename.toStdString() << std::endl;

   try
   {
      bit7z::Bit7zLibrary lib{L"7z.dll"};
      bit7z::BitExtractor extractor{ lib, bit7z::BitFormat::Rar };

      extractor.extract(mFilename.toStdWString(), mData, page);
   }
   catch (const bit7z::BitException& ex)
   {
      std::cout << ex.what() << std::endl;
   }
}


QStringList Unpacker::getArchiveContents(const QString& desiredFile)
{
   QStringList files;

   bit7z::Bit7zLibrary lib{L"7z.dll"};
   bit7z::BitArchiveInfo arc{ lib, desiredFile.toStdWString(), bit7z::BitFormat::Rar };

   try
   {
      auto arc_items = arc.items();
      for ( auto& item : arc_items )
      {
         if (
               item.extension() == L".jpg"
            || item.extension() == L".png"
         )
         {
            files << QString::fromStdWString(item.name());
         }

         std::cout << item.name().c_str() << std::endl;
      }
   }
   catch (const bit7z::BitException& ex)
   {
      std::cout << ex.what() << std::endl;
   }

   return files;
}


void Unpacker::readFrontPage()
{
   qDebug("processing file: %s", qPrintable(mFilename));

   // read archive contents
   QStringList files;

   // try cache info
   QFileInfo baseInfo(mFilename);
   QString baseName = baseInfo.baseName();

   QFile cacheFileInfo(QString("%1/%2.txt").arg(Config::getCachePath()).arg(baseName));
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
      files = getArchiveContents(mFilename);

      // nothing to do if no files in there
      if (files.isEmpty())
      {
         return;
      }

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

   // store book information
   mBook = new Book();
   mBook->mFilename = mFilename;
   mBook->mPages = files;

   // rewind to start and read file
   // auto desiredFile = files.at(0);

   // check cache
   auto cacheFilename = QString("%1/%2.jpg").arg(Config::getCachePath()).arg(baseName);
   QFile cacheFileImage(cacheFilename);

   if (cacheFileImage.exists())
   {
      if (cacheFileImage.open(QFile::ReadOnly))
      {
         mData.resize(static_cast<size_t>(cacheFileImage.size()));

         cacheFileImage.read(reinterpret_cast<char*>(mData.data()), cacheFileImage.size());
         cacheFileImage.close();

         mValid = mCover.loadFromData(
            reinterpret_cast<uchar*>(mData.data()),
            static_cast<uint32_t>(getPixmapSize())
         );
      }
   }
   else
   {
      readData();

      mValid = mCover.loadFromData(
         reinterpret_cast<uchar*>(mData.data()),
         static_cast<uint32_t>(getPixmapSize())
      );

      auto scaled = mCover.scaledToWidth(qMin(mCover.width(), 1000), Qt::SmoothTransformation);
      scaled.save(cacheFilename);
   }
}


void Unpacker::readPage()
{
   readData(mPage);
}


