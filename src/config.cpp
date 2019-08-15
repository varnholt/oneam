// base
#include "config.h"

// oneam
#include "comicconstants.h"

// Qt
#include <QCoreApplication>
#include <QDir>

Config* Config::sConfig = 0;


Config::Config(QObject *parent)
 : QSettings(
      QString("%1/%2/%3")
         .arg(QDir::home().absolutePath())
         .arg(HOME_DIR)
         .arg("oneam.ini"),
      QSettings::IniFormat, parent
   )
{
   sConfig = this;
   read();
}


QString Config::getRequestedBook() const
{
    return mRequestedBook;
}


void Config::setRequestedBook(const QString &rb)
{
    mRequestedBook = rb;
}



void Config::read()
{
    setPath(value("path").toString());
}


void Config::write()
{
    setValue("path", getPath());
}



Config *Config::getInstance()
{
    if (!sConfig)
      new Config();

   return sConfig;
}


const QString& Config::getPath() const
{
   return mPath;
}


void Config::setPath(const QString &path)
{
   mPath = path;
}


bool Config::isValid() const
{
   bool valid = (!getPath().isEmpty());
   return valid;
}

