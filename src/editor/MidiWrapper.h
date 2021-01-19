#ifndef MIDIWRAPPER_H
#define MIDIWRAPPER_H

#include <rtmidi/RtMidi.h>

#include <memory>

#include "EditState.h"

class MidiWrapper {
 private:
  std::unique_ptr<RtMidiIn> m_in;
  std::function<void(Input::Event::Event)> m_cb;

 public:
  MidiWrapper();
  std::vector<std::string> ports();

  bool usePort(int port, const std::function<void(Input::Event::Event)> &cb);
};

#endif  // MIDIWRAPPER_H
