// base
#include "pagewidget.h"
#include "ui_pagewidget.h"

// Qt
#include <QGraphicsPixmapItem>
#include <QKeyEvent>
#include <QThreadPool>

// comicview
#include "book.h"
#include "comicconstants.h"
#include "unpacker.h"


PageWidget::PageWidget(QWidget *parent) :
   QWidget(parent),
   mUi(new Ui::PageWidget),
   mScene(0),
   mBook(0),
   mIndex(0)
{
   mUi->setupUi(this);

   mScene = new QGraphicsScene(this);

   initGraphicsView();

   grabKeyboard();
}


void PageWidget::initGraphicsView()
{
   mUi->mGraphicsView->setRenderHints(
      QPainter::Antialiasing | QPainter::SmoothPixmapTransform
   );

   mUi->mGraphicsView->setScene(mScene);

   // set-up the view
   mUi->mGraphicsView->setSceneRect(0, 0, 1000, 1000);

   // use scrollHand drag mode to enable panning
   mUi->mGraphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
}



PageWidget::~PageWidget()
{
   delete mUi;
}


Book *PageWidget::getBook() const
{
   return mBook;
}


void PageWidget::setBook(Book *book)
{
   mBook = book;
}


void PageWidget::load()
{
   Unpacker* unpacker = new Unpacker();
   unpacker->setTask(Unpacker::TaskReadpage);
   unpacker->setFilename(mBook->getFilename());
   unpacker->setPage(mBook->getPages().at(getIndex()));
   unpacker->setAutoDelete(false);
   QThreadPool::globalInstance()->start(unpacker);

   connect(
      unpacker,
      SIGNAL(done()),
      this,
      SLOT(update()),
      Qt::QueuedConnection
   );
}


int PageWidget::getIndex() const
{
   return mIndex;
}


void PageWidget::setIndex(int index)
{
   bool changed = (index != mIndex);
   mIndex = index;

   if (changed)
      load();
}


void PageWidget::keyPressEvent(QKeyEvent * e)
{
   if (e->key() == Qt::Key_Left)
   {
      prev();
   }
   else if (e->key() == Qt::Key_Right)
   {
      next();
   }
}


void PageWidget::resizeEvent(QResizeEvent * e)
{
   if (mBook)
      load();
}


void PageWidget::next()
{
   if (!mBook)
      return;

   setIndex(qMin(mBook->getPageCount() - 1, mIndex + 1));
}


void PageWidget::prev()
{
   if (!mBook)
      return;

   setIndex(qMax(0, mIndex - 1));
}


void PageWidget::update()
{
   Unpacker* unpacker = (Unpacker*)sender();

   QPixmap pixmap;

   bool valid = pixmap.loadFromData(
      (uchar*)unpacker->getPixmap(),
      unpacker->getPixmapSize()
   );

   if (valid)
   {
      mScene->clear();

      mItemWidth = width(); // (int)( widthWithoutScrollBar / (float)itemsPerColumn);
      mItemHeight = (int)(mItemWidth * PAGE_RATIO);

      QPixmap scaled = pixmap.scaledToWidth(mItemWidth, Qt::SmoothTransformation);

      QGraphicsPixmapItem* item = new QGraphicsPixmapItem();
      item->setPixmap(scaled);
      item->setPos(0, 0);
      mScene->addItem(item);

   }

   unpacker->deleteLater();
}



void PageWidget::showBook(Book * book)
{
   setBook(book);
   raise();
   setIndex(0);
   load();
}
