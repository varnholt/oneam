#include "book.h"

Book::Book()
{
}


int Book::getPageCount()
{
   return mPages.size();
}


const QStringList &Book::getPages() const
{
   return mPages;
}


void Book::setPages(const QStringList &pages)
{
   mPages = pages;
}


const QString &Book::getFilename() const
{
   return mFilename;
}


void Book::setFilename(const QString &filename)
{
   mFilename = filename;
}


