#pragma once

#include <QShortcut>
#include <QMainWindow>

struct Book;


namespace Ui {
class MainWindow;
}

class MainWidget : public QMainWindow
{
   Q_OBJECT

public:

   explicit MainWidget(QWidget *parent = nullptr);
   ~MainWidget() override;

protected:
   void resizeEvent(QResizeEvent* evt) override;

private slots:
   void showBook(std::shared_ptr<Book>);
   void showPreview();
   void action(QAction* action);
   void toggleFullscreen();


private:
   QShortcut* mShortcutEscape = nullptr;
   QShortcut* mShortcutFullscreen = nullptr;
   QShortcut* mShortcutRefresh = nullptr;

   Ui::MainWindow *mUi;
   bool mFullscreen = false;
};

