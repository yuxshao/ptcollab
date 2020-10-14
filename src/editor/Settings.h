#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>

const extern QString WOICE_DIR_KEY;
const extern QString BUFFER_LENGTH_KEY;
const extern double DEFAULT_BUFFER_LENGTH;
const extern QString VOLUME_KEY;

const extern QString PTCOP_FILE_KEY;
const extern QString PTREC_SAVE_FILE_KEY;
const extern QString DISPLAY_NAME_KEY;

const extern QString HOST_SERVER_PORT_KEY;
const extern int DEFAULT_PORT;

const extern QString SAVE_RECORDING_ENABLED_KEY;
const extern QString HOSTING_ENABLED_KEY;

const extern QString CONNECT_SERVER_KEY;
const extern QString CUSTOM_STYLE_KEY;

namespace Version {
const QString &string();
}

#endif  // SETTINGS_H
