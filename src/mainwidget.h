#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QShortcut>
#include <QWidget>

class Book;


namespace Ui {
class MainWidget;
}

class MainWidget : public QWidget
{
   Q_OBJECT

public:

   explicit MainWidget(QWidget *parent = 0);
   ~MainWidget();


public slots:

   void updateBooks();


protected slots:

   void showBook(Book*);

   void showPreview();


private:

   QShortcut* mShortcutEscape;

   Ui::MainWidget *mUi;
};

#endif // MAINWIDGET_H
