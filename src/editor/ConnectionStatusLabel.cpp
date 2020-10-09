#include "ConnectionStatusLabel.h"

ConnectionStatusLabel::ConnectionStatusLabel(QWidget *parent) : QLabel(parent) {
  updateText();
}

void ConnectionStatusLabel::setClientConnectionState(
    std::optional<QString> connected_to) {
  m_connected_to = connected_to;
  updateText();
}

void ConnectionStatusLabel::setServerConnectionState(
    std::optional<QString> hosting_on) {
  m_hosting_on = hosting_on;
  updateText();
}

void ConnectionStatusLabel::updateText() {
  if (m_hosting_on.has_value()) {
    if (m_connected_to.has_value())
      setText(tr("Hosting on %1").arg(m_hosting_on.value()));
    else
      setText(
          tr("Hosting on (but not connected to) %1").arg(m_hosting_on.value()));
  } else {
    if (m_connected_to.has_value())
      setText(tr("Connected to %1").arg(m_connected_to.value()));
    else
      setText(tr("Not connected"));
  }
}
