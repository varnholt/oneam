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

QString Config::getCachePath()
{
   const auto cachePath = QString("%1/%2/%3")
      .arg(QDir::home().absolutePath())
      .arg(HOME_DIR)
      .arg(CACHE_DIR);

   return cachePath;
}


void Config::setPath(const QString &path)
{
   mPath = path;
}

bool Config::isValid(const QString& path) const
{
   const auto empty = path.isEmpty();
   const auto exists = QDir(path).exists();

   bool valid = (!empty && exists);

   return valid;
}


bool Config::isValid() const
{
   const auto& path = getPath();
   return isValid(path);
}

