#include "Data.h"

Data::Data(QString filename) : f(nullptr), m_data(nullptr) {
#ifdef _WIN32
  FILE *file;
  _wfopen_s(&file, filename.toStdWString().c_str(), L"rb");
#else
  FILE *file = fopen(filename.toStdString().c_str(), "rb");
#endif
  if (file == nullptr)
    throw std::runtime_error("Unable to open file: " + filename.toStdString());
  f = std::shared_ptr<FILE>(file, std::fclose);
  pxtnDescriptor d;
  d.set_file_r(f.get());
  m_size = d.get_size_bytes();
};

constexpr qint64 chunkSize = 32 * 1024;  // arbitrary 32KB

// Moves seek position to end
QDataStream &operator<<(QDataStream &out, const Data &m) {
  std::unique_ptr<char[]> buffer = std::make_unique<char[]>(chunkSize);
  long int starting_seek_pos;
  pxtnDescriptor d =
      m._descriptor_promise_no_change_to_seek_or_contents(&starting_seek_pos);
  qint64 bytesLeft = d.get_size_bytes();
  qDebug() << "sending file of size" << bytesLeft;
  d.seek(pxtnSEEK_set, 0);
  out << bytesLeft;
  while (bytesLeft > 0) {
    qint64 size = std::min(chunkSize, bytesLeft);
    if (!d.r(buffer.get(), 1, size)) {
      throw std::runtime_error("pxtnDescriptor write error");
    }
    bytesLeft -= size;
    out.writeRawData(buffer.get(), size);
  }
  if (starting_seek_pos != -1) d.seek(pxtnSEEK_set, starting_seek_pos);
  return out;
}

QDataStream &operator>>(QDataStream &in, Data &m) {
  m.f = nullptr;
  in >> m.m_size;
  m.m_data = std::shared_ptr<char[]>(new char[m.m_size]);

  qDebug() << "expecting file of size" << m.m_size;
  qint64 read = in.readRawData(m.m_data.get(), m.m_size);
  qDebug() << "read" << read << "bytes";
  if (read != m.m_size) {
    m.m_data = nullptr;
    m.m_size = 0;
  }
  return in;
}
