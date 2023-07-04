#ifndef MIDIWRAPPER_H
#define MIDIWRAPPER_H

#ifdef RTMIDI_SUPPORTED
#include <RtMidi.h>
#endif

#include <memory>

#include "EditState.h"

class MidiWrapper {
 private:
#ifdef RTMIDI_SUPPORTED
  std::unique_ptr<RtMidiIn> m_in;
  std::function<void(Input::Event::Event)> m_cb;
  int m_current_port;
#endif
 public:
  MidiWrapper();
  QStringList ports() const;

  std::optional<int> currentPort() const;
  std::string portDropdownMessage() const;
  bool usePort(int port, const std::function<void(Input::Event::Event)> &cb);
};

#endif  // MIDIWRAPPER_H
