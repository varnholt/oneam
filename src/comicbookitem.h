#pragma once

// Qt
#include <QGraphicsPixmapItem>

struct Book;

class ComicBookItem : public QObject, public QGraphicsPixmapItem
{
   Q_OBJECT

public:
   ComicBookItem();
   void setBook(std::shared_ptr<Book> book);

signals:
   void clicked(std::shared_ptr<Book>);

protected:
   virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
   std::shared_ptr<Book> mBook;
};

