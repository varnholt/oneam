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
      SIGNAL(activated()),
      this,
      SLOT(showPreview())
   );

   // menubar
   connect(
      mUi->mMenuBar,
      &QMenuBar::triggered,
      this,
      &MainWidget::action
   );

   // menubar
   connect(
      mUi->mMenuBrowse,
      &QMenu::aboutToShow,
      this,
      &MainWidget::showPreview
   );

   // if (!Config::getInstance()->isValid())
   // {
   //    mUi->mStackedWidget->setCurrentWidget(
   //       mUi->mConfig
   //    );
   //
   //    connect(
   //       mUi->mConfig,
   //       SIGNAL(done()),
   //       this,
   //       SLOT(showPreview())
   //    );
   // }
}


MainWidget::~MainWidget()
{
   delete mUi;
}



void MainWidget::updateBooks()
{
   mUi->mPreview->updateBooks();
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

   mUi->mPreview->updateBooks();
}


void MainWidget::action(QAction* action)
{
   if (action == mUi->mConfigPath)
   {
      mUi->mConfig->configureSearchPath();
      showPreview();
   }
}




