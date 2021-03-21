#include "MidiWrapper.h"

#include <QDebug>
#include <QMessageBox>
#include <QString>

MidiWrapper::MidiWrapper() {
  try {
    m_in = std::make_unique<RtMidiIn>();
  } catch (RtMidiError &error) {
    qWarning() << QString::fromStdString(error.getMessage());
  }
}

QStringList MidiWrapper::ports() const {
  if (m_in == nullptr) return {};
  unsigned int nPorts = m_in->getPortCount();
  std::string portName;
  QStringList ports;
  for (unsigned int i = 0; i < nPorts; i++) {
    try {
      portName = m_in->getPortName(i);
    } catch (RtMidiError &error) {
      qWarning() << QString::fromStdString(error.getMessage());
      return {};
    }
    ports.push_back(QString::fromStdString(portName));
  }
  return ports;
}

std::optional<int> MidiWrapper::currentPort() const {
  if (m_in->isPortOpen()) return m_current_port;
  return std::nullopt;
}

int pitch(std::vector<unsigned char> *message) {
  return int((*message)[1]) * 256 - 17664 /* middle a */ + EVENTDEFAULT_KEY;
}

void callback(double deltatime, std::vector<unsigned char> *message,
              void *callback) {
  auto &cb = *(std::function<void(Input::Event::Event)> *)callback;
  int message_kind = (*message)[0] >> 4;

  switch (message_kind) {
    case 8:
      qDebug() << deltatime << message_kind << pitch(message);
      cb(Input::Event::Off{pitch(message)});
      break;
    case 9: {
      int key = pitch(message);
      int vel = (*message)[2];
      qDebug() << deltatime << message_kind << (*message)[0] << key << vel;
      if (vel > 0)
        cb(Input::Event::On{key, vel});
      else
        cb(Input::Event::Off{key});
    } break;
      // TODO: pedal for skip, pitch bend
  }
};

bool MidiWrapper::usePort(int port,
                          const std::function<void(Input::Event::Event)> &cb) {
  if (m_in == nullptr) return false;
  if (m_in->isPortOpen()) {
    m_in->cancelCallback();
    m_in->closePort();
  }
  m_cb = cb;
  try {
    m_in->openPort(port);
  } catch (RtMidiError &error) {
    QMessageBox::warning(nullptr, QString("Could not open MIDI port"),
                         QString("Error opening MIDI port: %1")
                             .arg(QString::fromStdString(error.getMessage())));
  }

  if (m_in->isPortOpen())
    m_current_port = port;
  else
    return false;
  m_in->setCallback(callback, (void *)&m_cb);
  return true;
}
