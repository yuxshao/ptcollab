#include "VolumeMeter.h"

#include <QDebug>
#include <cmath>

VolumeMeter::VolumeMeter(uint32_t window_size, uint32_t peak_duration)
    : m_peak(peak_duration), m_window_size(window_size), m_squared_mean(0) {}

void VolumeMeter::insert(int64_t sample) {
  sample = sample * sample;

  m_squared_mean += sample;
  m_window.push(sample);
  while (m_window.size() > m_window_size) {
    m_squared_mean -= m_window.front();
    m_window.pop();
  }
  m_peak.insert(current_volume_dbfs());
}

double VolumeMeter::last_peak_dbfs() const { return m_peak.max(); }

double VolumeMeter::current_volume_dbfs() const {
  // https://dsp.stackexchange.com/questions/8785/how-to-compute-dbfs
  return 20 * log10(sqrt(m_squared_mean) / (1 << 16)) + 3.0103;
}

RunningMax::RunningMax(uint32_t window) {}
void RunningMax::insert(double x) {}

double RunningMax::max() const { return 0; }
