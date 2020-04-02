#pragma once

#include <QMap>
#include <QWidget>


struct Book;
class QGraphicsScene;
class QGraphicsItem;


namespace Ui {
class PreviewWidget;
}

class PreviewWidget : public QWidget
{
    Q_OBJECT

public:

   explicit PreviewWidget(QWidget *parent = nullptr);

   ~PreviewWidget();

   void initGraphicsView();

   void initDirList();

   void scan();


signals:

   void showBook(Book*);


protected:

   virtual void keyPressEvent(QKeyEvent *);
   virtual void keyReleaseEvent(QKeyEvent *);
   virtual void showEvent(QShowEvent*);
   virtual void hideEvent(QHideEvent*);


private slots:

   void processNext();

   void addPixmap();
   void addItem(
      Book* book,
      int32_t index,
      const QPixmap& scaled,
      const QString& filename
   );

   void itemClicked(Book*);

   void updateScrollBar();


private:

   Ui::PreviewWidget* mUi = nullptr;
   QGraphicsScene* mScene = nullptr;

   QMap<QString, Book*> mBooks;

   QStringList mDirList;

   QString mPath;

   int mIndex = 0;
   int mItemWidth = 0;
   int mItemHeight = 0;

   float mY = 0.0f;
   float mDy = 0.0f;
   int32_t mMax = 0;

   QTimer* mScrollUpdateTimer = nullptr;
   QString mRequestedBook;
};

