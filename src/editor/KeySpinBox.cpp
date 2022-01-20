#include "KeySpinBox.h"

#include <QDebug>

#include "editor/EditState.h"
#include "pxtone/pxtnService.h"

static const char *full_regex = "^([a-gA-G])([#b]?)(-?[0-9]+)$";
static const char *partial_regex = "^[a-gA-G]?[#b]?-?[0-9]*$";

constexpr int offset = EVENTDEFAULT_KEY / PITCH_PER_KEY - /* A4 */ 48;
static std::optional<int> text_to_key(const QString &text) {
  static QRegExp regex(full_regex);
  static std::map<QChar, int> notes{{'c', -9}, {'d', -7}, {'e', -5}, {'f', -4},
                                    {'g', -2}, {'a', 0},  {'b', 2}};
  static std::map<QChar, int> accidentals{{'#', 1}, {'b', -1}};

  if (regex.indexIn(text) != 0) return std::nullopt;
  QStringList l = regex.capturedTexts();
  if (l.length() != 4) return std::nullopt;
  QString note = l[1].toLower(), accidental = l[2], octave = l[3];
  int key = notes[note.at(0)] +
            (accidental == "" ? 0 : accidentals[accidental.at(0)]) +
            octave.toInt() * 12 + offset;
  // qDebug() << l << key;
  return key;
}

static QString key_to_text(int key) {
  constexpr int offset = EVENTDEFAULT_KEY / PITCH_PER_KEY - /* A4 */ 48;
  static std::vector<std::pair<QString, int>> notes{
      {"A", 0},  {"A#", 0}, {"B", 0}, {"C", 1},  {"C#", 1}, {"D", 1},
      {"D#", 1}, {"E", 1},  {"F", 1}, {"F#", 1}, {"G", 1},  {"G#", 1}};

  auto &[note, octave_offset] = notes[key % 12];
  int octave = (key - offset) / 12 + octave_offset;
  if (key < offset) --octave;  // Adjust if dividend is negative b/c of int div

  QString result = QString("%1%2").arg(note).arg(octave);
  // qDebug() << key << result;
  return result;
}

int KeySpinBox::valueFromText(const QString &text) const {
  return text_to_key(text).value_or(EVENTDEFAULT_KEY / PITCH_PER_KEY);
}

QString KeySpinBox::textFromValue(int key) const { return key_to_text(key); }

QValidator::State KeySpinBox::validate(QString &text, int &) const {
  static QRegExp regex(partial_regex);
  if (text_to_key(text).has_value()) return QValidator::Acceptable;
  if (regex.indexIn(text) != -1) return QValidator::Intermediate;
  return QValidator::Invalid;
}
