#ifndef DATA_H
#define DATA_H
#include <QDataStream>
#include <QDebug>
#include <iostream>
#include <memory>

#include "pxtone/pxtnDescriptor.h"
// TODO: SEe if you can use QFile instead for unicdoe support
class Data {
  FILE *f;
  std::unique_ptr<char[]> m_data;
  qint64 m_size;
  pxtnDescriptor m_desc;

 public:
  Data() : f(nullptr), m_data(nullptr), m_size(0) {}
  Data(QString filename);

  ~Data() {
    if (f != nullptr) fclose(f);
  }
  pxtnDescriptor &descriptor() { return m_desc; }

  friend QDataStream &operator<<(QDataStream &out, Data &m);
  friend QDataStream &operator>>(QDataStream &in, Data &m);
};

#endif  // DATA_H
