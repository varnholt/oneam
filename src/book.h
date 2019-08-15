#ifndef BOOK_H
#define BOOK_H

#include <QStringList>

class Book
{
   public:

      Book();


      int getPageCount();

      const QStringList& getPages() const;
      void setPages(const QStringList &getPages);


      const QString& getFilename() const;
      void setFilename(const QString &getFilename);


protected:

      QString mFilename;

      QStringList mPages;
};

#endif // BOOK_H
