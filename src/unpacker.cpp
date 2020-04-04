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


int Unpacker::getPreviewIndex() const
{
   return mPreviewIndex;
}


void Unpacker::setPreviewIndex(int index)
{
   mPreviewIndex = index;
}


Book *Unpacker::getBook() const
{
   return mBook;
}


void Unpacker::setBook(Book *book)
{
   mBook = book;
}


void Unpacker::setFileIndex(int32_t page)
{
   mFileIndex = page;
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
   std::cout << "reading page " << fileIndex << " from " << mFilename.toStdString() << std::endl;

   try
   {
      bit7z::Bit7zLibrary lib{L"7z.dll"};
      bit7z::BitExtractor extractor{
         lib,
         getFormat(mFilename)
      };

      extractor.extract(
         mFilename.toStdWString(),
         mData,
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
         // const auto& extension = item.extension();
         // const auto& name = item.name();
         // const auto& path = item.path();

         // std::wcout << "name: " << name << " path: " << path << " ext: " << extension << std::endl;

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

   return pages;
}


void Unpacker::readFrontPage()
{
   qDebug("processing file: %s", qPrintable(mFilename));

   // read archive contents
   std::vector<Page> pages;

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
            const auto contents = in.readLine().split(";");

            const auto fileIndex = contents.at(0).toInt();
            const auto filename = contents.at(1);

            Page page;

            page.mFilename = filename;
            page.mFileIndex = fileIndex;

            pages.push_back(page);
         }
      }
   }
   else
   {
      pages = getArchiveContents();

      // nothing to do if no files in there
      if (pages.empty())
      {
         return;
      }

      // sort and pick the 1st file afterwards
      std::sort(pages.begin(), pages.end());

      if (cacheFileInfo.open(QIODevice::WriteOnly | QIODevice::Text))
      {
         QTextStream out(&cacheFileInfo);
         for (const auto& p : pages)
         {
            out << p.mFileIndex << ";" << p.mFilename << "\n";
         }

         cacheFileInfo.close();
      }
   }

   // store book information
   mBook = new Book();
   mBook->mFilename = mFilename;
   mBook->mPages = pages;

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
      readData(mBook->mPages.at(0).mFileIndex);

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
   readData(mFileIndex);
}


