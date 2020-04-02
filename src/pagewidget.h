#pragma once

#include <QWidget>

struct Book;
class QGraphicsScene;

namespace Ui {
class PageWidget;
}

class PageWidget : public QWidget
{
   Q_OBJECT

public:


   explicit PageWidget(QWidget *parent = 0);
   ~PageWidget();


   Book *getBook() const;
   void setBook(Book *getBook);


   int getIndex() const;
   void setIndex(int getIndex);


protected:

   virtual void keyPressEvent(QKeyEvent *);
   virtual void keyReleaseEvent(QKeyEvent *);

   virtual void resizeEvent(QResizeEvent*);
   virtual void showEvent(QShowEvent*);
   virtual void hideEvent(QHideEvent*);


public slots:

   void load();
   void showBook(Book*);


private slots:

   void next();
   void prev();
   void update();
   void updateScrollBar();


protected:

   void initGraphicsView();

   Ui::PageWidget *mUi = nullptr;
   QGraphicsScene* mScene = nullptr;
   Book* mBook = nullptr;
   QTimer* mScrollUpdateTimer = nullptr;

   int mIndex = 0;
   int mItemWidth = 0;
   int mItemHeight = 0;

   float mY = 0.0f;
   float mDy = 0.0f;
   float mMin = 0.0f;
   float mMax = 0.0f;

};

