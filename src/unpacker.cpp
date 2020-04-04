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


const std::vector<uint8_t>& Unpacker::getRawData()
{
   return mRawData;
}


size_t Unpacker::getRawDataSize()
{
   return mRawData.size();
}


const QString& Unpacker::getFilename() const
{
   return mFilename;
}


void Unpacker::setFilename(const QString &filename)
{
   mFilename = filename;
}


int Unpacker::getPreviewIndex() const
{
   return mPreviewIndex;
}


void Unpacker::setPreviewIndex(int32_t index)
{
   mPreviewIndex = index;
}


std::shared_ptr<Book> Unpacker::getBook() const
{
   return mBook;
}


void Unpacker::setFileIndex(int32_t page)
{
   mFileIndex = page;
}


void Unpacker::setTask(const Task &task)
{
   mTask = task;
}


bool Unpacker::isValid() const
{
   return mValid;
}


const bit7z::BitInFormat& getFormat(const QString& filename)
{
   if (filename.toLower().endsWith(".cbz"))
   {
      return bit7z::BitFormat::Zip;
   }
   else if (filename.toLower().endsWith(".cbr"))
   {
      return bit7z::BitFormat::Rar;
   }
   else if (filename.toLower().endsWith(".cb7"))
   {
      return bit7z::BitFormat::SevenZip;
   }
   else if (filename.toLower().endsWith(".cbt"))
   {
      return bit7z::BitFormat::Tar;
   }

   return bit7z::BitFormat::Rar;
}


void Unpacker::readData(int32_t fileIndex)
{
   // std::cout << "reading page " << fileIndex << " from " << mFilename.toStdString() << std::endl;

   try
   {
      bit7z::Bit7zLibrary lib{L"7z.dll"};
      bit7z::BitExtractor extractor{
         lib,
         getFormat(mFilename)
      };

      extractor.extract(
         mFilename.toStdWString(),
         mRawData,
         static_cast<uint32_t>(fileIndex)
      );
   }
   catch (const bit7z::BitException& ex)
   {
      std::cout << ex.what() << std::endl;
   }
}


std::vector<Page> Unpacker::getArchiveContents()
{
   std::vector<Page> pages;

   try
   {
      bit7z::Bit7zLibrary lib{L"7z.dll"};

      bit7z::BitArchiveInfo arc{
         lib,
         mFilename.toStdWString(),
         getFormat(mFilename)
      };

      const auto& items = arc.items();

      for (auto& item : items)
      {
         if (
               item.extension() == L"jpg"
            || item.extension() == L"png"
         )
         {
            Page p;
            p.mFilename = QString::fromStdWString(item.name());
            p.mFileIndex = static_cast<int32_t>(item.index());
            pages.push_back(p);
         }
      }
   }
   catch (const bit7z::BitException& ex)
   {
      std::cout << ex.what() << std::endl;
   }

   // sort and pick the 1st file afterwards
   std::sort(pages.begin(), pages.end());

   return pages;
}


bool Unpacker::loadCoverFromCache()
{
   auto cacheFilename = QString("%1/%2.jpg").arg(Config::getCachePath()).arg(QFileInfo(mFilename).baseName());
   QFile cacheFileImage(cacheFilename);

   if (cacheFileImage.exists())
   {
      if (cacheFileImage.open(QFile::ReadOnly))
      {
         mRawData.resize(static_cast<size_t>(cacheFileImage.size()));

         cacheFileImage.read(reinterpret_cast<char*>(mRawData.data()), cacheFileImage.size());
         cacheFileImage.close();

         mValid = mBook->mCover.loadFromData(
            reinterpret_cast<uchar*>(mRawData.data()),
            static_cast<uint32_t>(getRawDataSize())
         );

         return true;
      }
   }
   return false;
}


void Unpacker::addCoverToCache()
{
   auto cacheFilename = QString("%1/%2.jpg").arg(Config::getCachePath()).arg(QFileInfo(mFilename).baseName());

   readData(mBook->mPages.at(0).mFileIndex);

   mValid = mBook->mCover.loadFromData(
      reinterpret_cast<uchar*>(mRawData.data()),
      static_cast<uint32_t>(getRawDataSize())
   );

   mBook->mCover = mBook->mCover.scaledToWidth(qMin(mBook->mCover.width(), mPreviewWidth), Qt::SmoothTransformation);
   mBook->mCover.save(cacheFilename);
}


void Unpacker::writeMetadataToCache()
{
   QFile cacheFileInfo(QString("%1/%2.txt").arg(Config::getCachePath()).arg(QFileInfo(mFilename).baseName()));

   if (cacheFileInfo.open(QIODevice::WriteOnly | QIODevice::Text))
   {
      QTextStream out(&cacheFileInfo);
      for (const auto& p : mBook->mPages)
      {
         out << p.mFileIndex << ";" << p.mFilename << "\n";
      }

      cacheFileInfo.close();
   }
}


bool Unpacker::loadMetadataFromCache()
{
   QFile cacheFileInfo(QString("%1/%2.txt").arg(Config::getCachePath()).arg(QFileInfo(mFilename).baseName()));

   // try cache info
   if (cacheFileInfo.exists())
   {
      if (cacheFileInfo.open(QFile::ReadOnly | QIODevice::Text))
      {
         QTextStream in(&cacheFileInfo);

         while (!in.atEnd())
         {
            const auto contents = in.readLine().split(";");

            const auto fileIndex = contents.at(0).toInt();
            const auto filename = contents.at(1);

            Page page;

            page.mFilename = filename;
            page.mFileIndex = fileIndex;

            mBook->mPages.push_back(page);
         }

         return true;
      }
   }

   return false;
}


void Unpacker::readFrontPage()
{
   qDebug("processing file: %s", qPrintable(mFilename));

   mBook = std::make_shared<Book>();

   if (!loadMetadataFromCache())
   {
      mBook->mPages = getArchiveContents();

      // nothing to do if no files in there
      if (mBook->mPages.empty())
      {
         std::cerr << "failed to deserialize" << std::endl;
         return;
      }

      writeMetadataToCache();
   }

   // store book information
   mBook->mFilename = mFilename;

   // check cache
   if (!loadCoverFromCache())
   {
      addCoverToCache();
   }
}


void Unpacker::readPage()
{
   readData(mFileIndex);
}


void Unpacker::setPreviewWidth(int32_t previewWidth)
{
   mPreviewWidth = previewWidth;
}


