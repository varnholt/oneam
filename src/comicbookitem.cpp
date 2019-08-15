// base
#include "comicbookitem.h"


ComicBookItem::ComicBookItem()
{
   setFlag( QGraphicsItem::ItemIsFocusable );
   setAcceptHoverEvents( true );
}


void ComicBookItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
   QGraphicsPixmapItem::mousePressEvent(event);

   emit clicked(getBook());
}


Book *ComicBookItem::getBook() const
{
   return mBook;
}


void ComicBookItem::setBook(Book *book)
{
   mBook = book;
}

