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

   initDirList();

   int threads = 2;
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
   int itemsPerColumn = 3;

   auto scrollBarWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 4;
   int widthWithoutScrollBar = width() - scrollBarWidth;

   mItemWidth = (width() / static_cast<float>(itemsPerColumn)) - scrollBarWidth;
   mItemHeight = mItemWidth * A4_ASPECT_RATIO;

   ComicBookItem* item = new ComicBookItem();
   item->setPixmap(scaled);
   item->setBook(book);
   mScene->addItem(item);
   mBooks.insert(filename, book);

   QFileInfo fi(filename);
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
}


void PreviewWidget::addPixmap()
{
   auto unpacker = dynamic_cast<Unpacker*>(sender());
   QPixmap pixmap;

   bool valid = pixmap.loadFromData(
      (uchar*)unpacker->getPixmap(),
      unpacker->getPixmapSize()
   );

   if (valid)
   {
      auto scaled = pixmap.scaledToWidth(mItemWidth, Qt::SmoothTransformation);

      const auto book = unpacker->getBook();
      const auto index = unpacker->getIndex();
      const auto filename = unpacker->getFilename();

      addItem(book, index, scaled, filename);

      qDebug(
         "file: %s, dimension: %d, %d",
         qPrintable(unpacker->getFilename()),
         pixmap.width(),
         pixmap.height()
      );

      // store file in cache
      QFileInfo fileInfo(filename);

      QString cachedFilename = QString("%1/%2/%3/%4.jpg")
            .arg(QDir::home().absolutePath())
            .arg(HOME_DIR)
            .arg(CACHE_DIR)
            .arg(fileInfo.baseName()
         );

      QFile f(cachedFilename);
      if (!f.exists())
      {
         scaled.save(cachedFilename);
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

