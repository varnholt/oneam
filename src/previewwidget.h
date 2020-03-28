#ifndef PREVIEWWIDGET_H
#define PREVIEWWIDGET_H

#include <QMap>
#include <QWidget>


class Book;
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

   void updateBooks();


signals:

   void showBook(Book*);


protected slots:

   void processNext();

   void addPixmap();

   void itemClicked(Book*);


private:

   Ui::PreviewWidget *mUi;

   QGraphicsScene* mScene;

   QMap<QString, Book*> mBooks;

   QStringList mDirList;

   QString mPath;

   int mIndex;

   int mItemWidth;

   int mItemHeight;

   QString mRequestedBook;
};

#endif // COMICWIDGET_H
