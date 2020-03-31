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

   void scan();


private slots:

   void showBook(Book*);
   void showPreview();
   void action(QAction* action);
   void toggleFullscreen();


private:

   QShortcut* mShortcutEscape = nullptr;
   QShortcut* mShortcutFullscreen = nullptr;

   Ui::MainWindow *mUi;
   bool mFullscreen = false;
};

