#include "CopyOptionsDialog.h"

#include <QKeyEvent>

#include "ui_CopyOptionsDialog.h"

CopyOptionsDialog::CopyOptionsDialog(Clipboard *c, QWidget *parent)
    : QDialog(parent), m_clipboard(c), ui(new Ui::CopyOptionsDialog) {
  ui->setupUi(this);
  mapping[EVENTKIND_VELOCITY] = ui->checkVelocity;
  mapping[EVENTKIND_PAN_VOLUME] = ui->checkPanVolume;
  mapping[EVENTKIND_PAN_TIME] = ui->checkPanTime;
  mapping[EVENTKIND_VOLUME] = ui->checkVolume;
  mapping[EVENTKIND_PORTAMENT] = ui->checkPortamento;
  mapping[EVENTKIND_TUNING] = ui->checkFineTune;
  mapping[EVENTKIND_VOICENO] = ui->checkVoice;
  mapping[EVENTKIND_GROUPNO] = ui->checkGroup;

  for (auto [k, check] : mapping) {
    EVENTKIND kind = k;
    connect(check, &QCheckBox::clicked, m_clipboard, [&, kind](bool checked) {
      m_clipboard->setKindIsCopied(kind, checked);
    });
  }

  setCopyOptions();
  connect(m_clipboard, &Clipboard::copyKindsSet, this,
          &CopyOptionsDialog::setCopyOptions);
}

CopyOptionsDialog::~CopyOptionsDialog() { delete ui; }

void CopyOptionsDialog::setCopyOptions() {
  for (auto [kind, check] : mapping) {
    bool shouldBeChecked = m_clipboard->kindIsCopied(kind);
    if (check->isChecked() != shouldBeChecked)
      check->setChecked(shouldBeChecked);
  }
}
void CopyOptionsDialog::keyPressEvent(QKeyEvent *event) {
  int key = event->key();
  switch (key) {
    case Qt::Key_C:
      if (event->modifiers() & Qt::ControlModifier &&
          event->modifiers() & Qt::ShiftModifier)
        hide();
      break;
    case Qt::Key_Escape:
      hide();
      break;
    default:
      break;
  }
}
