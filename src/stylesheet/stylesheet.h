#ifndef STYLESHEET_H
#define STYLESHEET_H

#include <QApplication>
#include <QFile>
#include <QString>

class StyleSheet : public QObject {
  Q_OBJECT

 public:
  QString styleSheetDirectory, styleSheetBreakPhrase, styleSheetEqualsPhrase;

  QString processStyleSheet(QString string);
  QString fromFile();
  static QString fromString(QString string);

  bool outputProcessed = false;
};

#endif  // STYLESHEET_H

// Ewan Green 2021
