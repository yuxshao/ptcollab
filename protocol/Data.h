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
  Data(QString filename) : f(nullptr), m_data(nullptr) {
#ifdef _WIN32
    f = _wfopen(filename.toStdWstring().c_str(), "rb");
#else
    f = fopen(filename.toStdString().c_str(), "rb");
#endif
    if (f == nullptr)
      throw std::runtime_error("Unable to open file: " +
                               filename.toStdString());
    m_desc.set_file_r(f);
    m_size = m_desc.get_size_bytes();
  };

  ~Data() {
    if (f != nullptr) fclose(f);
  }
  pxtnDescriptor &descriptor() { return m_desc; }

  friend QDataStream &operator<<(QDataStream &out, Data &m);
  friend QDataStream &operator>>(QDataStream &in, Data &m);
};

#endif  // DATA_H
