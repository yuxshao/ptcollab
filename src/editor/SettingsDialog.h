#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

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
  void midiPortSelected(int port_no);

 private:
  void showEvent(QShowEvent *event) override;
  const MidiWrapper *m_midi_wrapper;
  Ui::SettingsDialog *ui;

 private slots:
  void styleTickBox();

};

#endif  // SETTINGSDIALOG_H
