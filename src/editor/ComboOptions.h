#ifndef COMBOOPTIONS_H
#define COMBOOPTIONS_H

#include <QString>

#include "pxtone/pxtnEvelist.h"
extern const std::vector<std::pair<QString, int>> &quantizeXOptions();
extern const std::vector<std::pair<QString, int>> &quantizeYOptions();
extern const std::vector<std::pair<QString, EVENTKIND>> &paramOptions();

#endif  // COMBOOPTIONS_H
