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
  int m_prev_clock, m_prev_moo_clock, m_this_seek;
  bool m_this_seek_caught_up;
  QElapsedTimer timeSinceLastClock;

 public:
  explicit MooClock(PxtoneClient *client);

  int now();
  int nowNoWrap();
  bool this_seek_caught_up() const { return m_this_seek_caught_up; }
  bool has_last() const;
  int wrapClock(int clock) const;
  int last_clock() const;
  int repeat_clock() const;

 signals:
};

#endif  // MOOCLOCK_H
