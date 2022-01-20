#include "KeySpinBox.h"

#include "editor/EditState.h"
#include "pxtone/pxtnService.h"

std::optional<int> KeySpinBox::text_to_key(const QString &text) {
  static QRegExp regex("([a-g])([#b]?)([0-9]*)");
  static std::map<QChar, int> notes{{'c', -9}, {'d', -7}, {'e', -5}, {'f', -4},
                                    {'g', -2}, {'a', 0},  {'b', 2}};
  static std::map<QChar, int> accidentals{{'#', 1}, {'b', -1}};
  constexpr int offset = EVENTDEFAULT_KEY / PITCH_PER_KEY - /* A4 */ 48;

  if (regex.indexIn(text) != 0) return std::nullopt;
  QStringList l = regex.capturedTexts();
  if (l.length() != 3) return std::nullopt;
  QString note = l[0].toLower(), accidental = l[1], octave = l[2];
  return notes[note[0]] + accidentals[accidental[0]] + octave.toInt() * 12 +
         offset;
}

KeySpinBox::KeySpinBox() {}

void KeySpinBox::fixup(QString &input) const {}

void KeySpinBox::stepBy(int steps) {}

QValidator::State KeySpinBox::validate(QString &input, int &pos) const {}
