#pragma once

#include <QSettings>


class Config : public QSettings
{
   Q_OBJECT

public:

   static Config* getInstance();

   const QString& getPath() const;
   static QString getCachePath();

   void setPath(const QString &path);

   bool isValid(const QString& path) const;
   bool isValid() const;


   QString getRequestedBook() const;
   void setRequestedBook(const QString &rb);


public slots:

   void read();
   void write();


protected:

   explicit Config(QObject *parent = nullptr);

   QString mPath;

   static Config* sConfig;

   QString mRequestedBook;
};
