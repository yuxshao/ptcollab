#ifndef MOOCLOCK_H
#define MOOCLOCK_H

#include <QElapsedTimer>
#include <QObject>

#include "editor/PxtoneClient.h"

// A bunch of things to give the illusion of a smooth playhead when there's a
// buffer.
class MooClock : public QObject {
  Q_OBJECT

  PxtoneClient *m_client;
  int m_prev_clock, m_this_seek;
  bool m_this_seek_caught_up;
  QElapsedTimer timeSinceLastClock;

 public:
  explicit MooClock(PxtoneClient *client);

  qint32 now();
  bool this_seek_caught_up() { return m_this_seek_caught_up; }
  qint32 last_clock();
  qint32 repeat_clock();

 signals:
};

#endif  // MOOCLOCK_H
