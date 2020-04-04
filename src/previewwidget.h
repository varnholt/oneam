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

   void showBook(std::shared_ptr<Book>);


protected:

   virtual void keyPressEvent(QKeyEvent *);
   virtual void keyReleaseEvent(QKeyEvent *);
   virtual void showEvent(QShowEvent*);
   virtual void hideEvent(QHideEvent*);


private slots:

   void processNext();

   void addPixmap();
   void addItem(
      std::shared_ptr<Book>,
      int32_t index,
      const QString& filename
   );

   void itemClicked(std::shared_ptr<Book>);

   void updateScrollBar();
   void updateScrollBarOffset();

private:

   Ui::PreviewWidget* mUi = nullptr;
   QGraphicsScene* mScene = nullptr;

   QMap<QString, std::shared_ptr<Book>> mBooks;

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

