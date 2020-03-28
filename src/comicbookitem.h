#pragma once

// Qt
#include <QGraphicsPixmapItem>

class Book;

class ComicBookItem : public QObject, public QGraphicsPixmapItem
{
   Q_OBJECT

   public:

      ComicBookItem();

      Book *getBook() const;
      void setBook(Book *book);

signals:

      void clicked(Book*);

protected:

      virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);

      Book* mBook;
};

