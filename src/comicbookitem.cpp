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

   emit clicked(mBook);
}


void ComicBookItem::setBook(std::shared_ptr<Book> book)
{
   mBook = book;
}

