#ifndef MIDIWRAPPER_H
#define MIDIWRAPPER_H

#include <RtMidi.h>

#include <memory>

#include "EditState.h"

class MidiWrapper {
 private:
  std::unique_ptr<RtMidiIn> m_in;
  std::function<void(Input::Event::Event)> m_cb;
  int m_current_port;

 public:
  MidiWrapper();
  QStringList ports() const;

  std::optional<int> currentPort() const;
  bool usePort(int port, const std::function<void(Input::Event::Event)> &cb);
};

#endif  // MIDIWRAPPER_H
