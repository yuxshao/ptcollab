#include "VolumeMeter.h"

#include <QDebug>
#include <cmath>

VolumeMeter::VolumeMeter(uint32_t window_size, uint32_t peak_duration)
    : m_peak(peak_duration), m_window_size(window_size), m_squared_sum(0) {}

void VolumeMeter::insert(int64_t sample) {
  sample = sample * sample;

  m_squared_sum += sample;
  m_window.push(sample);
  while (m_window.size() > m_window_size) {
    m_squared_sum -= m_window.front();
    m_window.pop();
  }
  m_peak.insert(current_volume_dbfs());
}

double VolumeMeter::last_peak_dbfs() const { return m_peak.max(); }

double VolumeMeter::current_volume_dbfs() const {
  // https://dsp.stackexchange.com/questions/8785/how-to-compute-dbfs
  return 20 * log10(sqrt(double(m_squared_sum) / m_window_size) / (1 << 15)) +
         3.0103;
}

RunningMax::RunningMax(uint32_t window) {}
void RunningMax::insert(double x) {}

double RunningMax::max() const { return 0; }

void InterpolatedVolumeMeter::commit() const {
  int samples_to_commit = m_batch_timer.nsecsElapsed() * 44100 / 1e9;
  m_batch_timer.restart();
  for (int i = 0; i < samples_to_commit && m_pending_samples.size() > 0; ++i) {
    m_meter.insert(m_pending_samples.front());
    m_pending_samples.pop_front();
  }
}

InterpolatedVolumeMeter::InterpolatedVolumeMeter(uint32_t window_size,
                                                 uint32_t peak_duration)
    : m_meter(window_size, peak_duration) {
  m_batch_timer.start();
}

void InterpolatedVolumeMeter::insert(int64_t sample) {
  m_pending_samples.push_back(sample);
}

void InterpolatedVolumeMeter::new_batch() {
  for (int64_t sample : m_pending_samples) m_meter.insert(sample);
  m_pending_samples.clear();
}

double InterpolatedVolumeMeter::last_peak_dbfs() const {
  commit();
  return m_meter.last_peak_dbfs();
}

double InterpolatedVolumeMeter::current_volume_dbfs() const {
  commit();
  return m_meter.current_volume_dbfs();
}
