#pragma once

#include <QStringList>

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
   QString mFilename;
   std::vector<Page> mPages;
};


