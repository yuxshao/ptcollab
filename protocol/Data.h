#ifndef DATA_H
#define DATA_H
#include <QDataStream>
#include <QDebug>
#include <iostream>
#include <memory>

#include "pxtone/pxtnDescriptor.h"
// TODO: SEe if you can use QFile instead for unicdoe support
class Data {
  // Mutable because writing Data involves temp. changing the seek ptr.
  mutable std::shared_ptr<FILE> f;
  std::shared_ptr<char[]> m_data;
  qint64 m_size;

  pxtnDescriptor _descriptor_promise_no_change_to_seek_or_contents(
      long int *current_seek_pos) const {
    pxtnDescriptor d;
    if (f != nullptr) {
      d.set_file_r(f.get());
      if (current_seek_pos != nullptr) *current_seek_pos = ftell(f.get());
    } else {
      d.set_memory_r(m_data.get(), m_size);
      if (current_seek_pos != nullptr) *current_seek_pos = -1;
    }
    return d;
  }

 public:
  Data() : f(nullptr), m_data(nullptr), m_size(0) {}
  Data(QString filename);

  pxtnDescriptor descriptor() {
    return _descriptor_promise_no_change_to_seek_or_contents(nullptr);
  }

  friend QDataStream &operator<<(QDataStream &out, const Data &m);
  friend QDataStream &operator>>(QDataStream &in, Data &m);
};

#endif  // DATA_H
