#pragma once

#include <QStringList>
#include <QPixmap>

#include <cstdint>
#include <vector>

struct Page
{
   bool operator < (const Page& other);
   QString mFilename;
   int32_t mFileIndex;
};


struct Book
{
   QPixmap mCover;
   QString mFilename;
   std::vector<Page> mPages;
};


