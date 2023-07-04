#include "ComboOptions.h"

#include <QCoreApplication>

const std::vector<std::pair<QString, int>> &quantizeXOptions() {
  static auto v = ([]() {
    return std::vector<std::pair<QString, int>>{
        {"1", 1},   {"1/2", 2},   {"1/3", 3},   {"1/4", 4},   {"1/6", 6},
        {"1/8", 8}, {"1/12", 12}, {"1/16", 16}, {"1/24", 24}, {"1/48", 48}};
  })();
  return v;
}

const char* quantizeOptionNoneName = QT_TRANSLATE_NOOP("ComboOptions", "None");

const std::vector<std::pair<QString, int>> &quantizeYOptionsSimple() {
  static auto v = ([]() {
    return std::vector<std::pair<QString, int>>{
        {"1", 12}, {"1/2", 24}, {"1/3", 36}, {QCoreApplication::translate("ComboOptions", quantizeOptionNoneName), 3072}};
  })();
  return v;
}

const std::vector<std::pair<QString, int>> &quantizeYOptionsAdvanced() {
  static auto v = ([]() {
    std::vector<std::pair<QString, int>> v;
    for (int i = 7; i <= 36; ++i) v.push_back({QString("1/%1").arg(i), i});
    v.push_back({QCoreApplication::translate("ComboOptions", quantizeOptionNoneName), 3072});
    return v;
  })();
  return v;
}

// TODO: somehow dedup with above
const std::vector<std::pair<QString, int>> &keyboardDisplayOptions() {
  static auto v = ([]() {
    std::vector<std::pair<QString, int>> v;
    for (int i = 7; i <= 36; ++i) v.push_back({QString("%1-edo").arg(i), i});
    return v;
  })();
  return v;
}

const std::vector<std::string> paramOptionNames = std::vector<std::string> {
  QT_TRANSLATE_NOOP("ComboOptions", "Velocity"),
  QT_TRANSLATE_NOOP("ComboOptions", "Pan (Volume)"),
  QT_TRANSLATE_NOOP("ComboOptions", "Pan (Time)"),
  QT_TRANSLATE_NOOP("ComboOptions", "Volume"),
  QT_TRANSLATE_NOOP("ComboOptions", "Portamento"),
  QT_TRANSLATE_NOOP("ComboOptions", "Fine-tune"),
  QT_TRANSLATE_NOOP("ComboOptions", "Voice"),
  QT_TRANSLATE_NOOP("ComboOptions", "Group"),
};

const std::vector<std::pair<QString, EVENTKIND>> &paramOptions() {
  static auto v = ([]() {
    return std::vector<std::pair<QString, EVENTKIND>>{
        {QCoreApplication::translate("ComboOptions", paramOptionNames[0].c_str()), EVENTKIND_VELOCITY},
        {QCoreApplication::translate("ComboOptions", paramOptionNames[1].c_str()), EVENTKIND_PAN_VOLUME},
        {QCoreApplication::translate("ComboOptions", paramOptionNames[2].c_str()), EVENTKIND_PAN_TIME},
        {QCoreApplication::translate("ComboOptions", paramOptionNames[3].c_str()), EVENTKIND_VOLUME},
        {QCoreApplication::translate("ComboOptions", paramOptionNames[4].c_str()), EVENTKIND_PORTAMENT},
        {QCoreApplication::translate("ComboOptions", paramOptionNames[5].c_str()), EVENTKIND_TUNING},
        {QCoreApplication::translate("ComboOptions", paramOptionNames[6].c_str()), EVENTKIND_VOICENO},
        {QCoreApplication::translate("ComboOptions", paramOptionNames[7].c_str()), EVENTKIND_GROUPNO}};
  })();
  return v;
}
