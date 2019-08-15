#ifndef CONFIG_H
#define CONFIG_H

#include <QSettings>


class Config : public QSettings
{
   Q_OBJECT

public:

   static Config* getInstance();

   const QString& getPath() const;

   void setPath(const QString &path);

   bool isValid() const;


   QString getRequestedBook() const;
   void setRequestedBook(const QString &rb);

signals:


public slots:

   void read();
   void write();


protected:

   explicit Config(QObject *parent = 0);

   QString mPath;

   static Config* sConfig;

   QString mRequestedBook;
};

#endif // CONFIG_H
