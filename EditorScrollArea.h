#ifndef EDITORSCROLLAREA_H
#define EDITORSCROLLAREA_H

#include <QObject>
#include <QScrollArea>
#include <QWheelEvent>

class EditorScrollArea : public QScrollArea {
  Q_OBJECT
 public:
  EditorScrollArea(QWidget *parent);
  void wheelEvent(QWheelEvent *event) override;
};

#endif  // EDITORSCROLLAREA_H
