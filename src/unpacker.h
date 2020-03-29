#ifndef UNPACKER_H
#define UNPACKER_H

#include <QObject>
#include <QRunnable>
#include <QString>


class fex_t;
class Book;

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
   virtual ~Unpacker();

   void run();

   // basic operations
   void readFrontPage();
   void readPage();

   // archive operations
   void openFile();
   void readData(const QString& desiredFile);


   // getter & setter

   uchar* getPixmap();
   int getPixmapSize();


   const QString& getFilename() const;
   void setFilename(const QString &getFilename);


   int getIndex() const;
   void setIndex(int getIndex);


   Book* getBook() const;
   void setBook(Book* getBook);


   const QString &getPage() const;
   void setPage(const QString &getPage);


   Task getTask() const;
   void setTask(const Task &getTask);


signals:

   void done();


protected:

   Task mTask = TaskIdle;

   QString mFilename;

   uchar* mPixmap = nullptr;
   int mPixmapSize = 0;
   int mIndex = 0;
   fex_t* mFex = nullptr;
   Book* mBook = nullptr;

   QString mPage;
};

#endif // UNPACKER_H
