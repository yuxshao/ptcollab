#include "MidiWrapper.h"

#include <QDebug>

MidiWrapper::MidiWrapper() {
  try {
    m_in = std::make_unique<RtMidiIn>();
  } catch (RtMidiError &error) {
    qWarning() << QString::fromStdString(error.getMessage());
  }
}

std::vector<std::string> MidiWrapper::ports() {
  if (m_in == nullptr) return {};
  unsigned int nPorts = m_in->getPortCount();
  std::string portName;
  std::vector<std::string> ports;
  for (unsigned int i = 0; i < nPorts; i++) {
    try {
      portName = m_in->getPortName(i);
    } catch (RtMidiError &error) {
      qWarning() << QString::fromStdString(error.getMessage());
      return {};
    }
    ports.push_back(portName);
  }
  return ports;
}

void callback(double deltatime, std::vector<unsigned char> *message,
              void *callback) {
  auto &cb = *(std::function<void(Input::Event::Event)> *)callback;
  qDebug() << deltatime;
  int message_kind = (*message)[0] >> 8;
  switch (message_kind) {
    case 8:
      cb(Input::Event::Off{(*message)[1]});
      break;
    case 9:
      cb(Input::Event::On{(*message)[1], (*message)[2]});
      break;
      // TODO: pedal for skip, pitch bend
  }
};

bool MidiWrapper::usePort(int port,
                          const std::function<void(Input::Event::Event)> &cb) {
  if (m_in == nullptr) return false;
  m_cb = cb;
  m_in->openPort(port);
  m_in->setCallback(callback, (void *)&m_cb);
  return true;
}
