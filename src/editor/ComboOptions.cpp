#include "ComboOptions.h"

const std::vector<std::pair<QString, int>> &quantizeXOptions() {
  static auto v = ([]() {
    return std::vector<std::pair<QString, int>>{
        {"1", 1},   {"1/2", 2},   {"1/3", 3},   {"1/4", 4},   {"1/6", 6},
        {"1/8", 8}, {"1/12", 12}, {"1/16", 16}, {"1/24", 24}, {"1/48", 48}};
  })();
  return v;
}

const std::vector<std::pair<QString, int>> &quantizeYOptions() {
  static auto v = ([]() {
    return std::vector<std::pair<QString, int>>{
        {"1", 1}, {"1/2", 2}, {"1/3", 3}, {"1/4", 4}, {"None", 256}};
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
