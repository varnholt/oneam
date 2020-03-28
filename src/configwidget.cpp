// base
#include "configwidget.h"
#include "ui_configwidget.h"

// Qt
#include <QFileDialog>

// comicview
#include "config.h"


ConfigWidget::ConfigWidget(QWidget *parent) :
   QWidget(parent),
   mUi(new Ui::ConfigWidget)
{
   mUi->setupUi(this);

   connect(
      mUi->mButtonGetComicBookDir,
      SIGNAL(clicked()),
      this,
      SLOT(configureSearchPath())
   );
}


ConfigWidget::~ConfigWidget()
{
   delete mUi;
}


bool ConfigWidget::isConfigValid()
{
   return Config::getInstance()->isValid();
}


void ConfigWidget::configureSearchPath()
{
   Config* config = Config::getInstance();

   QString path = QFileDialog::getExistingDirectory(
      this,
      tr("Enter comic book directory"),
      config->getPath()
   );

   if (config->isValid(path))
   {
      config->setPath(path);
      config->write();
   }

   emit done();
}


