#include "book.h"


bool Page::operator <(const Page& other)
{
   return mFilename < other.mFilename;
}
