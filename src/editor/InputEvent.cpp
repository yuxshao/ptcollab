#include "InputEvent.h"

QEvent::Type InputEvent::eventType = QEvent::None;

InputEvent::InputEvent(const Input::Event::Event &event)
    : QEvent(eventType), event(event) {}

QEvent::Type InputEvent::registeredType() {
  if (eventType == QEvent::None) {
    int generatedType = QEvent::registerEventType();
    eventType = static_cast<QEvent::Type>(generatedType);
  }
  return eventType;
}
