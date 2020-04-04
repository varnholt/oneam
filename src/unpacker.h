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

   const std::vector<uint8_t>& getRawData();
   size_t getRawDataSize();

   const QString& getFilename() const;
   void setFilename(const QString &getFilename);

   int getPreviewIndex() const;
   void setPreviewIndex(int32_t getPreviewIndex);

   void setPreviewWidth(int32_t previewWidth);

   std::shared_ptr<Book> getBook() const;

   void setFileIndex(int32_t getPage);
   void setTask(const Task &getTask);

   bool isValid() const;


   void writeMetadataToCache();

signals:

   void done();


private:

   void run();
   void readFrontPage();
   bool loadCoverFromCache();
   void readPage();
   void addCoverToCache();
   bool loadMetadataFromCache();

   Task mTask = TaskIdle;

   QString mFilename;

   std::vector<uint8_t> mRawData;

   int32_t mPreviewWidth = 384;
   int32_t mPreviewIndex = 0;
   std::shared_ptr<Book> mBook = nullptr;
   bool mValid = false;

   int32_t mFileIndex = 0;
};

