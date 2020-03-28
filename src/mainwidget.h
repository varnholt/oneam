#pragma once

#include <QShortcut>
#include <QMainWindow>

class Book;


namespace Ui {
class MainWindow;
}

class MainWidget : public QMainWindow
{
   Q_OBJECT

public:

   explicit MainWidget(QWidget *parent = nullptr);
   ~MainWidget();


public slots:

   void updateBooks();


private slots:

   void showBook(Book*);
   void showPreview();
   void action(QAction* action);


private:

   QShortcut* mShortcutEscape;

   Ui::MainWindow *mUi;
};

