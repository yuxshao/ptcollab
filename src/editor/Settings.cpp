#include "Settings.h"

#include <optional>

#include "ComboOptions.h"

#ifndef PTCOLLAB_VERSION
#error Undefined PTCOLLAB_VERSION!
#endif

const QString BUFFER_LENGTH_KEY("buffer_length");
const double DEFAULT_BUFFER_LENGTH = 0.15;
const QString VOLUME_KEY("volume");

const QString PTREC_SAVE_FILE_KEY("ptrec_save_dir");
const QString DISPLAY_NAME_KEY("display_name");

const QString HOST_SERVER_PORT_KEY("host_server_port");
const int DEFAULT_PORT = 15835;

const QString SAVE_RECORDING_ENABLED_KEY("save_recording_enabled");
const QString HOSTING_ENABLED_KEY("hosting_enabled");

const QString CONNECT_SERVER_KEY("connect_server");

namespace Settings {
namespace Version {
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
static QString v(TOSTRING(PTCOLLAB_VERSION));
const QString &string() { return v; }
#undef TOSTRING
#undef STRINGIFY
}  // namespace Version

std::map<QString, std::optional<QVariant>> cache;
QSettings &settings() {
  static QSettings s;
  return s;
}

void setValue(const QString &key, const QVariant &value) {
  settings().setValue(key, value);
  cache[key] = value;
}

void remove(const QString &key) {
  settings().remove(key);
  cache[key] = std::nullopt;
}

std::optional<QVariant> valueOpt(const QString &key) {
  auto it = cache.find(key);
  if (it != cache.end()) return it->second;

  if (settings().contains(key))
    cache[key] = settings().value(key);
  else
    cache[key] = std::nullopt;

  return cache[key];
}

bool contains(const QString &key) { return valueOpt(key).has_value(); }
QVariant value(const QString &key, const QVariant &default_) {
  return valueOpt(key).value_or(default_);
}

void clear() {
  settings().clear();
  for (auto &[_, v] : cache) v = std::nullopt;
}

namespace TextSize {
const char *TEXT_SIZE_KEY = "text_size";
int default_size = 6;
int max_size = 18;
int min_size = 4;

int get() {
  bool ok;
  int v = value(TEXT_SIZE_KEY, default_size).toInt(&ok);
  if (!ok) return default_size;
  return std::clamp(v, min_size, max_size);
}

void increase() { setValue(TEXT_SIZE_KEY, std::min(get() + 1, max_size)); }

void decrease() { setValue(TEXT_SIZE_KEY, std::max(get() - 1, min_size)); }
}  // namespace TextSize

namespace ChordPreview {
const char *KEY = "ChordPreview";
bool get() { return value(KEY, false).toBool(); }
void set(bool value) { setValue(KEY, value); }
}  // namespace ChordPreview

namespace SpacebarStop {
const char *KEY = "SpacebarStop";
bool get() { return value(KEY, false).toBool(); }
void set(bool value) { setValue(KEY, value); }
}  // namespace SpacebarStop

namespace VelocityDrag {
const char *KEY = "VelocityDrag";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { setValue(KEY, value); }
}  // namespace VelocityDrag

namespace SwapScrollOrientation {
const char *KEY = "SwapScrollOrientation";
bool get() { return value(KEY, false).toBool(); }
void set(bool value) { setValue(KEY, value); }
}  // namespace SwapScrollOrientation

namespace SwapZoomOrientation {
const char *KEY = "SwapZoomOrientation";
bool get() { return value(KEY, false).toBool(); }
void set(bool value) { setValue(KEY, value); }
}  // namespace SwapZoomOrientation

namespace AutoAdvance {
const char *KEY = "AutoAdvance";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { setValue(KEY, value); }
}  // namespace AutoAdvance

namespace AutoAddUnit {
const char *KEY = "AutoAddUnit";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { setValue(KEY, value); }
}  // namespace AutoAddUnit

namespace ChangeDialogDirectory {
const char *KEY = "ChangeDialogDirectory";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { setValue(KEY, value); }
}  // namespace ChangeDialogDirectory

namespace PolyphonicMidiNotePreview {
const char *KEY = "PolyphonicMidiNotePreview";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { setValue(KEY, value); }
}  // namespace PolyphonicMidiNotePreview

namespace UnitPreviewClick {
const char *KEY = "UnitPreviewClick";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { setValue(KEY, value); }
}  // namespace UnitPreviewClick

namespace ShowWelcomeDialog {
const char *KEY = "ShowWelcomeDialog";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { setValue(KEY, value); }
}  // namespace ShowWelcomeDialog

namespace RenderFileDestination {
const char *KEY = "render_file_destination";
QString get() { return value(KEY, "").toString(); }
void set(QString value) { setValue(KEY, value); }
}  // namespace RenderFileDestination

namespace RenderDirectoryDestination {
const char *KEY = "render_directory_destination";
QString get() { return value(KEY, "").toString(); }
void set(QString value) { setValue(KEY, value); }
}  // namespace RenderDirectoryDestination

namespace StyleName {
const char *KEY = "style_name";
const char *default_included_with_distribution = "ptCollage";
QString get() {
  return value(KEY, default_included_with_distribution).toString();
}
void set(QString value) { setValue(KEY, value); }
}  // namespace StyleName

QList<int> intListOfVariant(const QVariant &v, const QList<int> &def) {
  QList<QVariant> vl = v.toList();
  QList<int> l;
  bool ok = true;
  for (const auto &a : vl) {
    l.push_back(a.toInt(&ok));
    if (!ok) return def;
  }
  return l;
}
QVariant intListToVariant(const QList<int> &l) {
  QList<QVariant> vl;
  for (const auto &a : l) vl.push_back(a);
  return vl;
}

QList<int> getIntList(QString key, const QList<int> &def) {
  return intListOfVariant(value(key, intListToVariant(def)), def);
}
namespace SideMenuWidth {
const char *KEY = "side_menu_width";
QList<int> get() { return getIntList(KEY, QList{10, 10000}); }
void set(const QList<int> &value) { setValue(KEY, intListToVariant(value)); }
}  // namespace SideMenuWidth

namespace BottomBarHeight {
const char *KEY = "bottom_bar_height";
QList<int> get() { return getIntList(KEY, QList{10000, 10}); }
void set(const QList<int> &value) { setValue(KEY, intListToVariant(value)); }
}  // namespace BottomBarHeight

namespace CopyKinds {
const char *KEY = "copy_kinds";
QList<int> default_value() {
  QList<int> v;
  for (auto [name, kind] : paramOptions())
    if (kind != EVENTKIND_VOICENO) v.push_back(kind);
  return v;
}
QList<int> get() {
  static QList<int> v(default_value());
  return getIntList(KEY, v);
}
void set(const QList<int> &value) {
  return setValue(KEY, intListToVariant(value));
}
}  // namespace CopyKinds

namespace SearchWoiceState {
const char *KEY = "search_woice_state";
bool isSet() { return contains(KEY); }
QByteArray get() { return value(KEY, QVariant()).toByteArray(); }
void set(const QByteArray &value) { return setValue(KEY, value); }
}  // namespace SearchWoiceState

namespace SearchWoiceLastSelection {
const char *KEY = "search_woice_last_selection";
QString get() { return value(KEY, QVariant()).toString(); }
void set(const QString &value) { return setValue(KEY, value); }
}  // namespace SearchWoiceLastSelection

namespace OpenProjectState {
const char *KEY = "open_project_state";
bool isSet() { return contains(KEY); }
QByteArray get() { return value(KEY, QVariant()).toByteArray(); }
void set(const QByteArray &value) { return setValue(KEY, value); }
}  // namespace OpenProjectState

namespace OpenProjectLastSelection {
const char *KEY = "open_project_last_selection";
QString get() { return value(KEY, QVariant()).toString(); }
void set(const QString &value) { return setValue(KEY, value); }
}  // namespace OpenProjectLastSelection

namespace BrowseWoiceState {
const char *KEY = "browse_woice_state";
bool isSet() { return contains(KEY); }
QByteArray get() { return value(KEY, QVariant()).toByteArray(); }
void set(const QByteArray &value) { return setValue(KEY, value); }
}  // namespace BrowseWoiceState

namespace SearchOnType {
const char *KEY = "search_on_type";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { return setValue(KEY, value); }
}  // namespace SearchOnType

namespace ShowVolumeMeterLabels {
const char *KEY = "show_volume_meter_labels";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { return setValue(KEY, value); }
}  // namespace ShowVolumeMeterLabels

namespace AutoConnectMidi {
const char *KEY = "auto_connect_midi";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { return setValue(KEY, value); }
}  // namespace AutoConnectMidi

namespace AutoConnectMidiName {
const char *KEY = "auto_connect_midi_name";
QString get() { return value(KEY, "").toString(); }
void set(const QString &value) { return setValue(KEY, value); }
}  // namespace AutoConnectMidiName

namespace AdvancedQuantizeY {
const char *KEY = "advanced_quantize_y";
bool get() { return value(KEY, false).toBool(); }
void set(bool value) { return setValue(KEY, value); }
}  // namespace AdvancedQuantizeY

namespace OctaveDisplayA {
const char *KEY = "octave_display_a";
bool get() { return value(KEY, false).toBool(); }
void set(bool value) { return setValue(KEY, value); }
}  // namespace OctaveDisplayA

namespace PinnedUnitLabels {
const char *KEY = "pinned_unit_labels";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { return setValue(KEY, value); }
}  // namespace PinnedUnitLabels

namespace MeasureViewClickToJumpUnit {
const char *KEY = "measure_view_click_to_jump_unit";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { return setValue(KEY, value); }
}  // namespace MeasureViewClickToJumpUnit

namespace NewUnitDefaultVolume {
const char *KEY = "new_unit_default_volume";
int get() { return value(KEY, EVENTDEFAULT_VOLUME).toInt(); }
void set(int value) { return setValue(KEY, value); }
}  // namespace NewUnitDefaultVolume

namespace DisplayEdo {
const char *KEY = "display_edo";
QList<int> get() {
  QList<int> r = getIntList(KEY, QList{0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1});
  if (r.size() < 2) r = {0, 1};
  if (r.size() > 35) r = r.mid(0, 35);
  return r;
}
void set(const QList<int> &value) { setValue(KEY, intListToVariant(value)); }
void clear() { return remove(KEY); }
}  // namespace DisplayEdo

namespace RecordMidi {
const char *KEY = "editor_recording";
bool get() { return value(KEY, false).toBool(); }
void set(bool value) { return setValue(KEY, value); }
}  // namespace RecordMidi

namespace StrictFollowSeek {
const char *KEY = "strict_follow_seek";
bool get() { return value(KEY, false).toBool(); }
void set(bool value) { return setValue(KEY, value); }
}  // namespace StrictFollowSeek

namespace VelocitySensitivity {
const char *KEY = "velocity_sensitivity";
bool get() { return value(KEY, true).toBool(); }
void set(bool value) { return setValue(KEY, value); }
}  // namespace VelocitySensitivity

namespace DisplayScale {
const char *KEY = "display_scale";
int get() { return std::max(1, value(KEY, 1).toInt()); }
void set(int value) { return setValue(KEY, value); }
}  // namespace DisplayScale

namespace LeftPianoWidth {
const char *KEY = "left_piano_width";
int get() { return std::max(0, value(KEY, 40).toInt()); }
void set(int value) { return setValue(KEY, value); }
}  // namespace LeftPianoWidth

namespace Language {
const char *KEY = "language";
QString get() { return value(KEY, "").toString(); }
void set(QString value) { return setValue(KEY, value); }
}  // namespace Language
}  // namespace Settings
