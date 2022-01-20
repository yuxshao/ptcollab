#ifndef KEYSPINBOX_H
#define KEYSPINBOX_H

#include <QAbstractSpinBox>
#include <optional>

class KeySpinBox : public QAbstractSpinBox {
  Q_OBJECT

  void fixup(QString &input) const override;
  void stepBy(int steps) override;
  QValidator::State validate(QString &input, int &pos) const override;

 public:
  static std::optional<int> text_to_key(const QString &text);
  KeySpinBox();
};

#endif  // KEYSPINBOX_H
