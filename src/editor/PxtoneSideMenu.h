#ifndef PXTONESIDEMENU_H
#define PXTONESIDEMENU_H

#include <QObject>

#include "PxtoneClient.h"
#include "SideMenu.h"
#include "audio/NotePreview.h"

class PxtoneSideMenu : public SideMenu {
  Q_OBJECT
  PxtoneClient *m_client;
  EditState m_last_edit_state;
  std::unique_ptr<NotePreview> m_note_preview;

  void handleNewEditState(const EditState &s);
  void refreshWoices();
  void refreshTempoBeat();

 public:
  explicit PxtoneSideMenu(PxtoneClient *client, UnitListModel *units,
                          QWidget *parent = nullptr);
 signals:
};

#endif  // PXTONESIDEMENU_H
