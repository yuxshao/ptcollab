#include "ComboOptions.h"

const std::vector<std::pair<QString, int>> &quantizeXOptions() {
  static auto v = ([]() {
    return std::vector<std::pair<QString, int>>{
        {"1", 1},   {"1/2", 2},   {"1/3", 3},   {"1/4", 4},   {"1/6", 6},
        {"1/8", 8}, {"1/12", 12}, {"1/16", 16}, {"1/24", 24}, {"1/48", 48}};
  })();
  return v;
}

const std::vector<std::pair<QString, int>> &quantizeYOptionsSimple() {
  static auto v = ([]() {
    return std::vector<std::pair<QString, int>>{
        {"1", 12}, {"1/2", 24}, {"1/3", 36}, {"None", 3072}};
  })();
  return v;
}
const std::vector<std::pair<QString, int>> &quantizeYOptionsAdvanced() {
  static auto v = ([]() {
    std::vector<std::pair<QString, int>> v;
    for (int i = 7; i <= 36; ++i) v.push_back({QString("1/%1").arg(i), i});
    v.push_back({"None", 3072});
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

const std::vector<std::pair<QString, EVENTKIND>> &paramOptions() {
  static auto v = ([]() {
    return std::vector<std::pair<QString, EVENTKIND>>{
        {"Velocity", EVENTKIND_VELOCITY},
        {"Pan (Volume)", EVENTKIND_PAN_VOLUME},
        {"Pan (Time)", EVENTKIND_PAN_TIME},
        {"Volume", EVENTKIND_VOLUME},
        {"Portamento", EVENTKIND_PORTAMENT},
        {"Fine-tune", EVENTKIND_TUNING},
        {"Voice", EVENTKIND_VOICENO},
        {"Group", EVENTKIND_GROUPNO}};
  })();
  return v;
}
