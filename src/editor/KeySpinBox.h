#ifndef KEYSPINBOX_H
#define KEYSPINBOX_H

#include <QSpinBox>
#include <optional>

class KeySpinBox : public QSpinBox {
  Q_OBJECT

 public:
  int valueFromText(const QString &) const override;
  QString textFromValue(int) const override;
  QValidator::State validate(QString &text, int &pos) const override;
  KeySpinBox(QWidget *parent = nullptr) : QSpinBox(parent){};
};

#endif  // KEYSPINBOX_H
