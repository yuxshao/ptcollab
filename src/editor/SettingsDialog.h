#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QDirIterator>

#include "MidiWrapper.h"
namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(const MidiWrapper *, QWidget *parent = nullptr);
  void apply();
  ~SettingsDialog();

 signals:
  void quantYOptionsChanged();
  void midiPortSelected(int port_no, const QString &port_name);

 private:
  void showEvent(QShowEvent *event) override;
  const MidiWrapper *m_midi_wrapper;
  Ui::SettingsDialog *ui;
};

#endif  // SETTINGSDIALOG_H
