#ifndef LINEARSMOOTHER_H
#define LINEARSMOOTHER_H

#include <QElapsedTimer>
#include <queue>

/* Because Moo gives us time updates only periodically, this class helps us give
 * the illusion of smooth time progression. */
class LinearSmoother {
 public:
  // m is in values / milliseconds
  LinearSmoother(uint lookback, double start_m, double start_b)
      : m_lookback(lookback),
        m_start_m(start_m),
        m_start_b(start_b),
        initial(){};

  int get() {
    if (timers.size() < m_lookback)
      return initial.elapsed() * m_start_m + m_start_b;
    // look at most recent actual, compare to oldest estimate, extrapolate.
    double m = (actual_values.back() - estimated_values.front()) /
               timers.back().msecsTo(timers.front());
    return estimated_values.back() + m * timers.back().elapsed();
  }

  int reset(double m) {
    m_start_b = get();
    m_start_m = m;
    std::queue<int>().swap(estimated_values);
    std::queue<int>().swap(actual_values);
    std::queue<QElapsedTimer>().swap(timers);
  }

  void record(int value) {
    estimated_values.push(get());
    timers.emplace();
    actual_values.push(value);
    if (estimated_values.size() > m_lookback) {
      estimated_values.pop();
      timers.pop();
      actual_values.pop();
    }
  };

 private:
  uint m_lookback;
  double m_start_m, m_start_b;
  QElapsedTimer initial;
  std::queue<QElapsedTimer> timers;
  std::queue<int> actual_values;
  std::queue<int> estimated_values;
};

#endif  // LINEARSMOOTHER_H
