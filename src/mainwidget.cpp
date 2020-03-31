#include "mainwidget.h"
#include "ui_mainwidget.h"

#include "config.h"


MainWidget::MainWidget(QWidget *parent) :
   QMainWindow(parent),
   mUi(new Ui::MainWindow)
{
   mUi->setupUi(this);

   mShortcutEscape = new QShortcut(this);
   mShortcutEscape->setKey(Qt::Key_Escape);

   mShortcutFullscreen = new QShortcut(this);
   mShortcutFullscreen->setKey(Qt::Key_F11);

   connect(
      mUi->mPreview,
      SIGNAL(showBook(Book*)),
      this,
      SLOT(showBook(Book*))
   );

   connect(
      mUi->mPreview,
      SIGNAL(showBook(Book*)),
      mUi->mBook,
      SLOT(showBook(Book*))
   );

   connect(
      mShortcutEscape,
      &QShortcut::activated,
      this,
      &MainWidget::showPreview
   );

   connect(
      mShortcutFullscreen,
      &QShortcut::activated,
      this,
      &MainWidget::toggleFullscreen
   );

   // menubar
   connect(
      mUi->mMenuBar,
      &QMenuBar::triggered,
      this,
      &MainWidget::action
   );

//   // menubar
//   connect(
//      mUi->mMenuBrowse,
//      &QMenu::aboutToShow,
//      this,
//      &MainWidget::showPreview
//   );
}


MainWidget::~MainWidget()
{
   delete mUi;
}



void MainWidget::scan()
{
   mUi->mPreview->scan();
}


void MainWidget::showBook(Book*)
{
   mUi->mStackedWidget->setCurrentWidget(
      mUi->mBook
   );
}


void MainWidget::showPreview()
{
   mUi->mStackedWidget->setCurrentWidget(
      mUi->mPreview
   );

   mUi->mPreview->scan();
}


void MainWidget::action(QAction* action)
{
   if (action == mUi->mConfigPath)
   {
      mUi->mConfig->configureSearchPath();
      showPreview();
   }
   else if (action == mUi->mFullscreen)
   {
      toggleFullscreen();
   }
   else if (action == mUi->mBrowse)
   {
      showPreview();
   }
}


void MainWidget::toggleFullscreen()
{
   mFullscreen = !mFullscreen;

   if (mFullscreen)
   {
      showFullScreen();
   }
   else
   {
      showNormal();
   }
}


