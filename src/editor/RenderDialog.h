#ifndef RENDERDIALOG_H
#define RENDERDIALOG_H

#include <QDialog>

namespace Ui {
class RenderDialog;
}

class RenderDialog : public QDialog {
  Q_OBJECT

 public:
  explicit RenderDialog(QWidget *parent = nullptr);
  ~RenderDialog();

  void setSongLength(double l);
  void setSongLoopLength(double l);
  void setVolume(double v);
  double renderLength();
  double renderFadeout();
  double renderVolume();
  bool renderUnitsSeparately();
  QString renderDestination();

 private:
  Ui::RenderDialog *ui;
};

#endif  // RENDERDIALOG_H
