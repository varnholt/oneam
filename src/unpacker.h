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

   void readData(int32_t page);
   std::vector<Page> getArchiveContents();

   const std::vector<uint8_t>& getPixmap();
   size_t getPixmapSize();

   const QString& getFilename() const;
   void setFilename(const QString &getFilename);

   int getPreviewIndex() const;
   void setPreviewIndex(int getPreviewIndex);

   void setPreviewWidth(const int32_t& previewWidth);

   std::shared_ptr<Book> getBook() const;

   void setFileIndex(int32_t getPage);
   void setTask(const Task &getTask);

   const QPixmap& getCover() const;
   bool isValid() const;


signals:

   void done();


private:

   void run();
   void readFrontPage();
   bool loadCoverFromCache();
   void readPage();

   Task mTask = TaskIdle;

   QString mFilename;

   std::vector<uint8_t> mData;

   int32_t mPreviewWidth = 384;
   int32_t mPreviewIndex = 0;
   std::shared_ptr<Book> mBook = nullptr;
   QPixmap mCover;
   bool mValid = false;

   int32_t mFileIndex = 0;
   void addCoverToCache();
};

