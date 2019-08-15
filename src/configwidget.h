#ifndef CONFIGWIDGET_H
#define CONFIGWIDGET_H

#include <QWidget>

class Config;

namespace Ui {
class ConfigWidget;
}

class ConfigWidget : public QWidget
{
   Q_OBJECT

public:

   explicit ConfigWidget(QWidget *parent = 0);

   ~ConfigWidget();

   bool isConfigValid();


signals:

   void done();


public slots:

protected slots:

   void openFileDialogForPath();

private:

   Ui::ConfigWidget* mUi;
};

#endif // CONFIGWIDGET_H
