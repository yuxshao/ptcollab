#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>
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

namespace TextSize {
int get();
void increase();
void decrease();
}  // namespace TextSize

namespace Version {
const QString &string();
}

namespace ChordPreview {
bool get();
void set(bool);
}  // namespace ChordPreview
namespace RenderFileDestination {
QString get();
void set(QString);
}  // namespace RenderFileDestination

#endif  // SETTINGS_H
