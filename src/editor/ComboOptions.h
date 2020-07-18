#ifndef QUANTIZE_H
#define QUANTIZE_H

#include <QString>

#include "pxtone/pxtnEvelist.h"
static std::pair<QString, int> quantizeXOptions[] = {
    {"1", 1},   {"1/2", 2},   {"1/3", 3},   {"1/4", 4},   {"1/6", 6},
    {"1/8", 8}, {"1/12", 12}, {"1/16", 16}, {"1/24", 24}, {"1/48", 48}};

static std::pair<QString, int> quantizeYOptions[] = {
    {"1", 1}, {"1/2", 2}, {"1/3", 3}, {"1/4", 4}, {"None", 256}};

static std::pair<QString, EVENTKIND> paramOptions[] = {
    {"Velocity", EVENTKIND_VELOCITY},
    {"Pan (Volume)", EVENTKIND_PAN_VOLUME},
    {"Pan (Time)", EVENTKIND_PAN_TIME},
    {"Volume", EVENTKIND_VOLUME}};

#endif  // QUANTIZE_H
