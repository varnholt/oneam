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

   Ui::PreviewWidget* mUi = nullptr;
   QGraphicsScene* mScene = nullptr;

   QMap<QString, Book*> mBooks;

   QStringList mDirList;

   QString mPath;

   int mIndex = 0;
   int mItemWidth = 0;
   int mItemHeight = 0;

   QString mRequestedBook;
};

#endif // COMICWIDGET_H
