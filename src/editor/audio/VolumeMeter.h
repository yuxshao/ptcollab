#ifndef VOLUMEMETER_H
#define VOLUMEMETER_H
#include <cstdint>
#include <queue>
#include <stack>
#include <vector>

class RunningMax {
  // https://stackoverflow.com/questions/4802038/implement-a-queue-in-which-push-rear-pop-front-and-get-min-are-all-consta
  std::stack<std::pair<double, double>> s1, s2;

 public:
  RunningMax(uint32_t window);
  void insert(double x);
  double max() const;
};

class VolumeMeter {
  std::queue<uint64_t> m_window;
  RunningMax m_peak;
  uint32_t m_window_size;
  uint64_t m_squared_mean;

 public:
  VolumeMeter(uint32_t window_size, uint32_t peak_duration);
  void insert(int64_t sample);
  double last_peak_dbfs() const;
  double current_volume_dbfs() const;
};

#endif  // VOLUMEMETER_H
