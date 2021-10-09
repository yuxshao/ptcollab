#ifndef PXTONESIDEMENU_H
#define PXTONESIDEMENU_H

#include <QObject>

#include "SideMenu.h"
#include "editor/NewWoiceDialog.h"
#include "editor/PxtoneClient.h"
#include "editor/audio/NotePreview.h"
#include "editor/views/MooClock.h"

class PxtoneSideMenu : public SideMenu {
  Q_OBJECT
  PxtoneClient *m_client;
  MooClock *m_moo_clock;
  EditState m_last_edit_state;
  std::unique_ptr<NotePreview> m_note_preview;

  void handleNewEditState(const EditState &s);
  void refreshWoices();
  void refreshTempoBeat();
  void refreshCopyCheckbox();

 public:
  explicit PxtoneSideMenu(PxtoneClient *client, MooClock *moo_clock,
                          NewWoiceDialog *new_woice_dialog,
                          NewWoiceDialog *change_woice_dialog,
                          QWidget *parent = nullptr);
 signals:
};

#endif  // PXTONESIDEMENU_H
