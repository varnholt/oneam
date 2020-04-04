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


   explicit PageWidget(QWidget *parent = nullptr);
   ~PageWidget() override;

   std::shared_ptr<Book> getBook() const;

   int getIndex() const;
   void setIndex(int getIndex);


protected:

   void keyPressEvent(QKeyEvent *) override;
   void keyReleaseEvent(QKeyEvent *) override;

   void resizeEvent(QResizeEvent*) override;
   void showEvent(QShowEvent*) override;
   void hideEvent(QHideEvent*) override;


public slots:

   void load();
   void showBook(std::shared_ptr<Book>);


private slots:

   void next();
   void prev();
   void update();
   void updateScrollBar();
   void updateScrollBarOffset();


protected:

   void initGraphicsView();

   Ui::PageWidget *mUi = nullptr;
   QGraphicsScene* mScene = nullptr;
   std::shared_ptr<Book> mBook = nullptr;
   QTimer* mScrollUpdateTimer = nullptr;

   int32_t mIndex = 0;

   int32_t mItemWidth = 0;
   int32_t mItemHeight = 0;

   float mY = 0.0f;
   float mDy = 0.0f;
   float mMin = 0.0f;
   float mMax = 0.0f;

};

