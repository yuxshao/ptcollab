#ifndef STATUSLABEL_H
#define STATUSLABEL_H

#include <optional>
#include <QLabel>

class ConnectionStatusLabel : public QLabel {
  Q_OBJECT
  std::optional<QString> m_connected_to;
  std::optional<QString> m_hosting_on;
  void updateText();

 public:
  ConnectionStatusLabel(QWidget *parent = nullptr);
  void setClientConnectionState(std::optional<QString> connected_to);
  void setServerConnectionState(std::optional<QString> hosting_on);
};

#endif  // STATUSLABEL_H
