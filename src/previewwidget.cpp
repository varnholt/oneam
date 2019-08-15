// base
#include "previewwidget.h"
#include "ui_previewwidget.h"

// Qt
#include <QDir>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextStream>
#include <QThreadPool>

// comicview
#include "book.h"
#include "config.h"
#include "comicbookitem.h"
#include "comicconstants.h"
#include "unpacker.h"


PreviewWidget::PreviewWidget(QWidget *parent) :
    QWidget(parent),
    mUi(new Ui::PreviewWidget),
    mIndex(0)
{
   mUi->setupUi(this);

   mScene = new QGraphicsScene(this);

   initGraphicsView();

   if (Config::getInstance()->isValid())
   {
      updateBooks();
   }
}


PreviewWidget::~PreviewWidget()
{
    delete mUi;
}


void PreviewWidget::updateBooks()
{
   initDirList();

   int threads = 2;
   for (int i = 0; i < threads; i++)
      processNext();
}


void PreviewWidget::initDirList()
{
   QDir dir;
   mPath = Config::getInstance()->getPath();
   dir.setPath(mPath);
   QStringList filters;
   filters << "*.cbr";
   // filters << "*.cbz";
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

   // set-up the view
   mUi->mGraphicsView->setSceneRect(0, 0, 1000, 1000);

   // use scrollHand drag mode to enable panning
   mUi->mGraphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
}


void PreviewWidget::addPixmap()
{
   Unpacker* unpacker = (Unpacker*)sender();
   QPixmap pixmap;

   bool valid = pixmap.loadFromData(
      (uchar*)unpacker->getPixmap(),
      unpacker->getPixmapSize()
   );

   if (valid)
   {
      int itemsPerColumn = 3;

      int scrollBarWidth = mUi->mGraphicsView->verticalScrollBar()->sizeHint().width();
      int widthWithoutScrollBar = width() - scrollBarWidth;

      mItemWidth = (int)( widthWithoutScrollBar / (float)itemsPerColumn);
      mItemHeight = (int)(mItemWidth * PAGE_RATIO);
      Book* book = unpacker->getBook();
      int index = unpacker->getIndex();

      QPixmap scaled = pixmap.scaledToWidth(mItemWidth, Qt::SmoothTransformation);
      ComicBookItem* item = new ComicBookItem();
      item->setPixmap(scaled);
      item->setBook(book);
      mScene->addItem(item);
      mBooks.insert(unpacker->getFilename(), book);

      QFileInfo fi(unpacker->getFilename());
      if (fi.baseName() == Config::getInstance()->getRequestedBook())
      {
         emit showBook(book);
      }

      connect(
         item,
         SIGNAL(clicked(Book*)),
         this,
         SLOT(itemClicked(Book*))
      );

      // place item
      int col = (index % itemsPerColumn) * mItemWidth;
      int row = ((index - (index % itemsPerColumn)) / itemsPerColumn) * mItemHeight;

      item->setPos(
         col,
         row
      );

      int desiredHeight = row + mItemHeight;
      if (mUi->mGraphicsView->sceneRect().height() < desiredHeight)
      {
         mUi->mGraphicsView->setSceneRect(
            0,
            0,
            widthWithoutScrollBar,
            desiredHeight
         );
      }

      qDebug(
         "file: %s, dimension: %d, %d",
         qPrintable(unpacker->getFilename()),
         pixmap.width(),
         pixmap.height()
      );

      // store file in cache
      QFileInfo fileInfo(unpacker->getFilename());
      QString filename = QString("%1/%2/%3/%4.jpg")
            .arg(QDir::home().absolutePath())
            .arg(HOME_DIR)
            .arg(CACHE_DIR)
            .arg(fileInfo.baseName()
         );

      QFile f(filename);
      if (!f.exists())
      {
         scaled.save(filename);
      }
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



/*

void MyGraphicsView::wheelEvent(QWheelEvent* event) {

    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    // Scale the view / do the zoom
    double scaleFactor = 1.15;
    if(event->delta() > 0) {
        // Zoom in
        scale(scaleFactor, scaleFactor);
    } else {
        // Zooming out
        scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }

    // Don't call superclass handler here
    // as wheel is normally used for moving scrollbars
}
*/


