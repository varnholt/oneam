// base
#include "previewwidget.h"
#include "ui_previewwidget.h"

// Qt
#include <QDir>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QScrollBar>
#include <QStyle>
#include <QTextStream>
#include <QThreadPool>
#include <QTimer>

// comicview
#include "book.h"
#include "config.h"
#include "comicbookitem.h"
#include "comicconstants.h"
#include "unpacker.h"

namespace
{
   static const auto threads = 1;
   static const auto itemsPerColumn = 3;
}

PreviewWidget::PreviewWidget(QWidget *parent) :
    QWidget(parent),
    mUi(new Ui::PreviewWidget),
    mIndex(0)
{
   mUi->setupUi(this);

   mScene = new QGraphicsScene(this);

   mScrollUpdateTimer = new QTimer(this);

   connect(
      mScrollUpdateTimer,
      &QTimer::timeout,
      this,
      &PreviewWidget::updateScrollBar
   );

   mScrollUpdateTimer->setInterval(16);

   initGraphicsView();
}


PreviewWidget::~PreviewWidget()
{
    delete mUi;
}


void PreviewWidget::scan()
{
   if (!Config::getInstance()->isValid())
   {
      return;
   }

   mIndex = 0;

   initDirList();

   for (auto i = 0; i < threads; i++)
   {
      processNext();
   }
}


void PreviewWidget::initDirList()
{
   mPath = QDir::toNativeSeparators(Config::getInstance()->getPath());
   QDir dir(mPath);

   QStringList filters;
   filters << "*.cbr";
   filters << "*.cbz";

   dir.setNameFilters(filters);
   mDirList = dir.entryList();
}


void PreviewWidget::itemClicked(Book* book)
{
   if (book)
   {
      qDebug("book clicked: %s", qPrintable(book->getFilename()));

      emit showBook(book);
   }
}


void PreviewWidget::initGraphicsView()
{
   mUi->mGraphicsView->setRenderHints(
      QPainter::Antialiasing | QPainter::SmoothPixmapTransform
   );

   mUi->mGraphicsView->setScene(mScene);
   mUi->mGraphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
}


void PreviewWidget::addItem(
   Book* book,
   int32_t index,
   const QPixmap& scaled,
   const QString& filename
)
{
   const auto scrollBarWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 4;
   const auto widthWithoutScrollBar = width() - scrollBarWidth;

   // place item
   const auto col = (index % itemsPerColumn) * mItemWidth;
   const auto row = ((index - (index % itemsPerColumn)) / itemsPerColumn) * mItemHeight;

   mItemWidth = static_cast<int32_t>((width() / static_cast<float>(itemsPerColumn)) - scrollBarWidth);
   mItemHeight = static_cast<int32_t>(mItemWidth * A4_ASPECT_RATIO);

   mMax = qMax((row + 1) * mItemHeight, mMax);

   ComicBookItem* item = new ComicBookItem();
   item->setPixmap(scaled);
   item->setBook(book);
   item->setPos(col, row);

   mScene->addItem(item);
   mBooks.insert(filename, book);

   connect(
      item,
      SIGNAL(clicked(Book*)),
      this,
      SLOT(itemClicked(Book*))
   );

   const auto desiredHeight = row + mItemHeight;
   if (mUi->mGraphicsView->sceneRect().height() < desiredHeight)
   {
      mUi->mGraphicsView->setSceneRect(
         0,
         0,
         widthWithoutScrollBar,
         desiredHeight
      );
   }

   QFileInfo fi(filename);
   if (fi.baseName() == Config::getInstance()->getRequestedBook())
   {
      emit showBook(book);
   }
}


void PreviewWidget::addPixmap()
{
   auto unpacker = dynamic_cast<Unpacker*>(sender());

   if (unpacker->isValid())
   {
      auto scaled = unpacker->getCover().scaledToWidth(mItemWidth, Qt::SmoothTransformation);

      const auto book = unpacker->getBook();
      const auto index = unpacker->getIndex();
      const auto filename = unpacker->getFilename();

      addItem(book, index, scaled, filename);
   }

   unpacker->deleteLater();
}


void PreviewWidget::processNext()
{
   if (mIndex < mDirList.size())
   {
      QString file = mDirList[mIndex];
      Unpacker* unpacker = new Unpacker();
      unpacker->setTask(Unpacker::TaskReadFrontPage);
      unpacker->setFilename(QString("%1/%2").arg(mPath).arg(file));
      unpacker->setIndex(mIndex);
      unpacker->setAutoDelete(false);

      connect(
         unpacker,
         SIGNAL(done()),
         this,
         SLOT(addPixmap()),
         Qt::QueuedConnection
      );

      connect(
         unpacker,
         SIGNAL(done()),
         this,
         SLOT(processNext()),
         Qt::QueuedConnection
      );

      mIndex++;

      QThreadPool::globalInstance()->start(unpacker);
   }
}



void PreviewWidget::keyPressEvent(QKeyEvent * e)
{
   if (e->isAutoRepeat())
   {
      return;
   }

   if (e->key() == Qt::Key_Up)
   {
      mDy = -SCROLL_SPEED_PREVIEW;
      mScrollUpdateTimer->start();
   }
   else if (e->key() == Qt::Key_Down)
   {
      mDy = SCROLL_SPEED_PREVIEW;
      mScrollUpdateTimer->start();
   }
}


void PreviewWidget::keyReleaseEvent(QKeyEvent* e)
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


void PreviewWidget::updateScrollBar()
{
   mY = mY + mDy;

   if (mY < 0)
   {
      mY = 0;
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


void PreviewWidget::showEvent(QShowEvent*)
{
   grabKeyboard();
}


void PreviewWidget::hideEvent(QHideEvent*)
{
   releaseKeyboard();
}
