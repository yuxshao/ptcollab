#include "stylesheet/stylesheet.h"

QString StyleSheet::processStyleSheet(QString string) {
  if (styleSheetBreakPhrase.isEmpty() || styleSheetEqualsPhrase.isEmpty())
    return nullptr;

  QString vars = string, output = string, currentString;
  QStringList *varNames = new QStringList, *varValues = new QStringList;
  int breakPointBefore =
          output.indexOf(this->styleSheetBreakPhrase, 0, Qt::CaseInsensitive),
      breakPointAfter = breakPointBefore + this->styleSheetBreakPhrase.length(),
      variableCount = vars.count(this->styleSheetEqualsPhrase), current = 0,
      next = 0;

  vars = vars.remove(breakPointBefore,
                     vars.length() - this->styleSheetBreakPhrase.length());
  output = output.remove(0, breakPointAfter);

  for (int i = variableCount; i != 0; i--) {
    currentString = vars.mid(current, vars.indexOf("\n"));
    next = currentString.indexOf(this->styleSheetEqualsPhrase);
    varNames->append(currentString.mid(current, next));
    current = next + this->styleSheetEqualsPhrase.length();

    next = currentString.size();
    varValues->append(currentString.mid(current, next));
    vars.remove(0, currentString.size() + 1);
    currentString.clear();
    current = 0;
  }
  vars.clear();

  for (int i = variableCount; i != 0; i--) {
    output.replace(varNames->at(i - 1), varValues->at(i - 1));
  }
  if (outputProcessed) {
    QFile outputFile(qApp->applicationDirPath() + "/parsed-stylesheet.qss");
    outputFile.remove();
    outputFile.open(QFile::ReadWrite);
    outputFile.write(output.toUtf8());
  }
  return output;
}  /// Member for extracting & applying variables to a stylesheet from a string

QString StyleSheet::fromFile() {
  if (styleSheetDirectory.isEmpty()) return nullptr;

  QFile styleSheetFile(styleSheetDirectory);
  styleSheetFile.open(QFile::ReadOnly);
  QString output = processStyleSheet(styleSheetFile.readAll());
  styleSheetFile.close();
  return output;
}  /// Member for getting a usable stylesheet string from a predefined file
   /// (styleSheetDirectory)

QString StyleSheet::fromString(QString string) {
  StyleSheet temp;
  return temp.processStyleSheet(string);
}  /// Static member for processing a stylesheet from a string without an object

// Ewan Green 2021
