#ifndef VOLUMEMETER_H
#define VOLUMEMETER_H
#include <QElapsedTimer>
#include <cstdint>
#include <queue>
#include <stack>
#include <vector>

class RunningMax {
  // https://stackoverflow.com/questions/4802038/implement-a-queue-in-which-push-rear-pop-front-and-get-min-are-all-consta
  std::stack<std::pair<double, double>> s1, s2;
  double s1max(), s2max();

 public:
  RunningMax(uint32_t window);
  void insert(double x);
  double zmax() const;
};

class VolumeMeter {
  std::queue<uint64_t> m_window;
  RunningMax m_peak;
  uint32_t m_window_size;
  uint64_t m_squared_sum;

 public:
  VolumeMeter(uint32_t window_size, uint32_t peak_duration);
  void insert(int64_t sample);
  double last_peak_dbfs() const;
  double current_volume_dbfs() const;
};

// In practice you get samples to the volume meter in batches.
// So as with MooClock some timing hacks to unbatch them according to how much
// time has elapsed.
class InterpolatedVolumeMeter {
  mutable std::deque<int64_t> m_pending_samples;
  mutable QElapsedTimer m_batch_timer;
  mutable VolumeMeter m_meter;
  void commit() const;

 public:
  InterpolatedVolumeMeter(uint32_t window_size, uint32_t peak_duration);
  void insert(int64_t sample);
  void new_batch();
  double last_peak_dbfs() const;
  double current_volume_dbfs() const;
};

#endif  // VOLUMEMETER_H
