#include "Data.h"

Data::Data(QString filename) : f(nullptr), m_data(nullptr) {
#ifdef _WIN32
  f = _wfopen(filename.toStdWstring().c_str(), "rb");
#else
  f = fopen(filename.toStdString().c_str(), "rb");
#endif
  if (f == nullptr)
    throw std::runtime_error("Unable to open file: " + filename.toStdString());
  m_desc.set_file_r(f);
  m_size = m_desc.get_size_bytes();
};

constexpr qint64 chunkSize = 32 * 1024;  // arbitrary 32KB
// Moves seek position to end
QDataStream &operator<<(QDataStream &out, Data &m) {
  std::unique_ptr<char[]> buffer = std::make_unique<char[]>(chunkSize);
  qint64 bytesLeft = m.descriptor().get_size_bytes();
  qDebug() << "sending file of size" << bytesLeft;
  m.descriptor().seek(pxtnSEEK_set, 0);
  out << bytesLeft;
  while (bytesLeft > 0) {
    qint64 size = std::min(chunkSize, bytesLeft);
    if (!m.descriptor().r(buffer.get(), 1, size)) {
      throw std::runtime_error("pxtnDescriptor write error");
    }
    bytesLeft -= size;
    out.writeRawData(buffer.get(), size);
  }
  return out;
}

QDataStream &operator>>(QDataStream &in, Data &m) {
  m.f = nullptr;
  in >> m.m_size;
  m.m_data = std::make_unique<char[]>(m.m_size);

  qDebug() << "expecting file of size" << m.m_size;
  qint64 read = in.readRawData(m.m_data.get(), m.m_size);
  qDebug() << "read" << read << "bytes";
  if (read == m.m_size)
    m.m_desc.set_memory_r(m.m_data.get(), m.m_size);
  else {
    m.m_data = nullptr;
    m.m_size = 0;
  }
  return in;
}
