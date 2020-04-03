#pragma once

#include <QObject>
#include <QPixmap>
#include <QRunnable>
#include <QString>

#include "book.h"


class Unpacker : public QObject, public QRunnable
{

Q_OBJECT

public:

   enum Task
   {
      TaskIdle,
      TaskReadFrontPage,
      TaskReadpage
   };

   Unpacker(QObject* parent = nullptr);

   void run();

   // basic operations
   void readFrontPage();
   void readPage();

   // archive operations
   void readData(int32_t page);
   std::vector<Page> getArchiveContents(const QString& desiredFile);

   const std::vector<uint8_t>& getPixmap();
   size_t getPixmapSize();

   const QString& getFilename() const;
   void setFilename(const QString &getFilename);


   int getIndex() const;
   void setIndex(int getIndex);


   Book* getBook() const;
   void setBook(Book* getBook);


   int32_t getPage() const;
   void setPage(int32_t getPage);


   Task getTask() const;
   void setTask(const Task &getTask);


   const QPixmap& getCover() const;

   bool isValid() const;


signals:

   void done();


protected:

   Task mTask = TaskIdle;

   QString mFilename;

   std::vector<uint8_t> mData;

   int mIndex = 0;
   Book* mBook = nullptr;
   QPixmap mCover;
   bool mValid = false;

   int32_t mPage = 0;
};

