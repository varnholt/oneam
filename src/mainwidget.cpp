#include "mainwidget.h"
#include "ui_mainwidget.h"

#include "config.h"

#include <QResizeEvent>


MainWidget::MainWidget(QWidget *parent) :
   QMainWindow(parent),
   mUi(new Ui::MainWindow)
{
   mUi->setupUi(this);

   mShortcutEscape = new QShortcut(this);
   mShortcutEscape->setKey(Qt::Key_Escape);

   mShortcutFullscreen = new QShortcut(this);
   mShortcutFullscreen->setKey(Qt::Key_F11);

   mShortcutRefresh = new QShortcut(this);
   mShortcutRefresh->setKey(Qt::Key_F5);

   connect(
      mUi->mPreview,
      &PreviewWidget::showBook,
      this,
      &MainWidget::showBook
   );

   connect(
      mUi->mPreview,
      &PreviewWidget::showBook,
      mUi->mBook,
      &PageWidget::showBook
   );

   connect(
      mShortcutEscape,
      &QShortcut::activated,
      this,
      &MainWidget::showPreview
   );

   connect(
      mShortcutRefresh,
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
}


MainWidget::~MainWidget()
{
   delete mUi;
}


void MainWidget::resizeEvent(QResizeEvent* evt)
{
   // start scanning when we have a proper screen size
   if (evt->oldSize().width() != -1)
   {
      showPreview();
   }
}


void MainWidget::showBook(std::shared_ptr<Book>)
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


