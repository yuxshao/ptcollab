#ifndef SONGTITLEDIALOG_H
#define SONGTITLEDIALOG_H

#include <QDialog>

namespace Ui {
class SongTitleDialog;
}

class SongTitleDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SongTitleDialog(const QString &title, const QString &comment,
                           QWidget *parent = nullptr);
  QString title();
  QString comment();

  ~SongTitleDialog();

 private:
  Ui::SongTitleDialog *ui;
};

#endif  // SONGTITLEDIALOG_H
