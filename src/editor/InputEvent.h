#ifndef INPUTEVENT_H
#define INPUTEVENT_H

#include <QEvent>

#include "EditState.h"

class InputEvent : public QEvent {
 public:
  InputEvent(const Input::Event::Event &event);
  Input::Event::Event event;

  static QEvent::Type registeredType();

 private:
  static QEvent::Type eventType;
};

#endif  // INPUTEVENT_H
