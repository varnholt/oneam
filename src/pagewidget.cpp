// base
#include "pagewidget.h"
#include "ui_pagewidget.h"

// Qt
#include <QGraphicsPixmapItem>
#include <QKeyEvent>
#include <QScrollBar>
#include <QStyle>
#include <QThreadPool>
#include <QTimer>

// stl
#include <iostream>

// comicview
#include "book.h"
#include "comicconstants.h"
#include "unpacker.h"



PageWidget::PageWidget(QWidget *parent) :
   QWidget(parent),
   mUi(new Ui::PageWidget)
{
   mUi->setupUi(this);

   mScene = new QGraphicsScene(this);

   initGraphicsView();

   mScrollUpdateTimer = new QTimer(this);

   connect(
      mScrollUpdateTimer,
      &QTimer::timeout,
      this,
      &PageWidget::updateScrollBar
   );

   connect(
      mUi->mGraphicsView->verticalScrollBar(),
      &QAbstractSlider::sliderReleased,
      this,
      &PageWidget::updateScrollBarOffset
   );

   mScrollUpdateTimer->setInterval(16);
}


void PageWidget::initGraphicsView()
{
   mUi->mGraphicsView->setRenderHints(
      QPainter::Antialiasing | QPainter::SmoothPixmapTransform
   );

   mUi->mGraphicsView->setScene(mScene);
   mUi->mGraphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
}



PageWidget::~PageWidget()
{
   delete mUi;
}


std::shared_ptr<Book> PageWidget::getBook() const
{
   return mBook;
}


void PageWidget::load()
{
   Unpacker* unpacker = new Unpacker();
   unpacker->setTask(Unpacker::TaskReadpage);
   unpacker->setFilename(mBook->mFilename);
   unpacker->setFileIndex(mBook->mPages[getIndex()].mFileIndex);
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
   {
      load();
   }
}


void PageWidget::keyPressEvent(QKeyEvent * e)
{
   if (e->isAutoRepeat())
   {
      return;
   }

   if (e->key() == Qt::Key_Left)
   {
      prev();
   }
   else if (e->key() == Qt::Key_Right)
   {
      next();
   }
   else if (e->key() == Qt::Key_Up)
   {
      mDy = -SCROLL_SPEED_PAGE;
      mScrollUpdateTimer->start();
   }
   else if (e->key() == Qt::Key_Down)
   {
      mDy = SCROLL_SPEED_PAGE;
      mScrollUpdateTimer->start();
   }
}


void PageWidget::keyReleaseEvent(QKeyEvent* e)
{
   if (e->isAutoRepeat())
   {
      return;
   }

   if (
         e->key() == Qt::Key_Up
      || e->key() == Qt::Key_Down
   )
   {
      mDy = 0.0f;
      mScrollUpdateTimer->stop();
   }
}


void PageWidget::resizeEvent(QResizeEvent * /*e*/)
{
   if (mBook)
   {
      load();
   }
}


void PageWidget::showEvent(QShowEvent*)
{
   grabKeyboard();
}


void PageWidget::hideEvent(QHideEvent*)
{
   releaseKeyboard();
}


void PageWidget::next()
{
   if (!mBook)
   {
      return;
   }

   const auto last = static_cast<int32_t>(mBook->mPages.size() - 1);
   const auto next = mIndex + 1;

   setIndex(std::min(last, next));
}


void PageWidget::prev()
{
   if (!mBook)
   {
      return;
   }

   setIndex(qMax(0, mIndex - 1));
}


void PageWidget::update()
{
   auto unpacker = dynamic_cast<Unpacker*>(sender());

   QPixmap pixmap;

   bool valid = pixmap.loadFromData(
      const_cast<uchar*>(unpacker->getRawData().data()),
      static_cast<uint32_t>(unpacker->getRawDataSize())
   );

   if (valid)
   {
      mScene->clear();

      auto scrollBarWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 4;

      mItemWidth = width() - scrollBarWidth;
      mItemHeight = static_cast<int32_t>(mItemWidth * A4_ASPECT_RATIO);

      mUi->mGraphicsView->setSceneRect(
         0,
         0,
         mItemWidth,
         mItemHeight
      );

      QPixmap scaled = pixmap.scaledToWidth(mItemWidth, Qt::SmoothTransformation);

      QGraphicsPixmapItem* item = new QGraphicsPixmapItem();
      item->setPixmap(scaled);
      item->setPos(0, 0);

      mScene->addItem(item);
   }

   unpacker->deleteLater();

   mDy = 0.0f;
   mY = 0.0f;

   mMax = mUi->mGraphicsView->verticalScrollBar()->maximum();

   updateScrollBar();
}


void PageWidget::updateScrollBar()
{
   mY = mY + mDy;

   if (mY < mMin)
   {
      mY = mMin;
      mDy = 0.0f;
   }

   if (mY > mMax)
   {
      mY = mMax;
      mDy = 0.0f;
   }

   const auto value = mUi->mGraphicsView->verticalScrollBar()->value();

   if (static_cast<int32_t>(mY) != value)
   {
      mUi->mGraphicsView->verticalScrollBar()->setValue(static_cast<int32_t>(mY));
   }
}


void PageWidget::updateScrollBarOffset()
{
   mY = mUi->mGraphicsView->verticalScrollBar()->value();
}


void PageWidget::showBook(std::shared_ptr<Book> book)
{
   mBook = book;
   raise();
   setIndex(0);
   load();
}

