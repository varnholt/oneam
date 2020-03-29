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

   void scan();


signals:

   void showBook(Book*);


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
   void addItem();
};

#endif // COMICWIDGET_H
