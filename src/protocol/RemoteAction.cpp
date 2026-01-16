#include "RemoteAction.h"

#include <QDebug>

QDebug operator<<(QDebug debug, const ClientAction &a) {
  debug << "ClientAction";
  return debug;
}

QDebug operator<<(QDebug debug, const ServerAction &a) {
  debug << a.uid << "ClientAction";
  return debug;
}
