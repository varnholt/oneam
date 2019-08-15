#ifndef PAGEWIDGET_H
#define PAGEWIDGET_H

#include <QWidget>

class Book;
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

   virtual void resizeEvent(QResizeEvent*);


public slots:

   void load();
   void showBook(Book*);


protected slots:

   void next();
   void prev();
   void update();


protected:

   void initGraphicsView();

   Ui::PageWidget *mUi;

   QGraphicsScene* mScene;

   Book* mBook;

   int mIndex;

   int mItemWidth;
   int mItemHeight;
};

#endif // PAGEWIDGET_H
