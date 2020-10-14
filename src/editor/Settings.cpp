#include "Settings.h"

const QString WOICE_DIR_KEY("woice_dir");
const QString BUFFER_LENGTH_KEY("buffer_length");
const double DEFAULT_BUFFER_LENGTH = 0.5;
const QString VOLUME_KEY("volume");

const QString PTCOP_FILE_KEY("ptcop_dir");
const QString PTREC_SAVE_FILE_KEY("ptrec_save_dir");
const QString DISPLAY_NAME_KEY("display_name");

const QString HOST_SERVER_PORT_KEY("host_server_port");
const int DEFAULT_PORT = 15835;

const QString SAVE_RECORDING_ENABLED_KEY("save_recording_enabled");
const QString HOSTING_ENABLED_KEY("hosting_enabled");

const QString CONNECT_SERVER_KEY("connect_server");
const QString CUSTOM_STYLE_KEY("use_custom_style");

namespace Version {
const int major = 0;
const int minor = 3;
const int patch = 4;
const QString &string() {
  static QString v = QString("%1.%2.%3").arg(major).arg(minor).arg(patch);
  return v;
}
}  // namespace Version
