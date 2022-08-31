#ifndef SERIALIZEVARIANT_H
#define SERIALIZEVARIANT_H

#include <QDataStream>
#include <optional>
#include <set>
#include <stdexcept>
#include <variant>

// Some scourging the internet to find a way to deserialize a variant b/c
// variants work well for deduping code for the msg protocol
// https://github.com/nlohmann/json/issues/1261
namespace detail {
template <std::size_t N>
struct variant_switch {
  template <typename Variant>
  inline void operator()(size_t index, QDataStream &in, Variant &v) const {
    if (index == N) {
      std::variant_alternative_t<N, Variant> a;
      in >> a;
      v.template emplace<N>(a);
    } else
      variant_switch<N - 1>{}(index, in, v);
  }
};

template <>
struct variant_switch<0> {
  template <typename Variant>
  inline void operator()(size_t index, QDataStream &in, Variant &v) const {
    if (index == 0) {
      std::variant_alternative_t<0, Variant> a;
      in >> a;
      v.template emplace<0>(a);
    } else
      throw std::runtime_error("invalid variant index");
  }
};
}  // namespace detail

template <typename... Args>
QDataStream &operator>>(QDataStream &in, std::variant<Args...> &a) {
  quint64 index;
  in >> index;
  ::detail::variant_switch<sizeof...(Args) - 1>{}(size_t(index), in, a);
  return in;
}

template <typename... Args>
QDataStream &operator<<(QDataStream &out, const std::variant<Args...> &a) {
  out << quint64(a.index());
  std::visit([&out](const auto &v) { out << v; }, a);
  return out;
}
template <typename T>
QDataStream &operator<<(QDataStream &out, const std::optional<T> &a) {
  qint8 has_value = a.has_value();
  out << has_value;
  if (has_value != 0) out << a.value();
  return out;
}
template <typename T>
QDataStream &operator>>(QDataStream &in, std::optional<T> &a) {
  qint8 has_value;
  in >> has_value;

  if (has_value != 0) {
    T value;
    in >> value;
    a.emplace(value);
  } else
    a.reset();
  return in;
}

template <typename T>
QDataStream &operator<<(QDataStream &out, const std::list<T> &a) {
  out << quint64(a.size());
  for (const auto &i : a) out << i;
  return out;
}
template <typename T>
QDataStream &operator<<(QDataStream &out, const std::set<T> &a) {
  out << quint64(a.size());
  for (const auto &i : a) out << i;
  return out;
}

template <typename T>
QDataStream &operator>>(QDataStream &in, std::list<T> &a) {
  quint64 size;
  in >> size;
  for (quint64 i = 0; i < size; ++i) {
    T v;
    in >> v;
    a.push_back(v);
  }
  return in;
}

template <typename T>
QDataStream &operator>>(QDataStream &in, std::set<T> &a) {
  quint64 size;
  in >> size;
  for (quint64 i = 0; i < size; ++i) {
    T v;
    in >> v;
    a.insert(v);
  }
  return in;
}

inline QDataStream &operator<<(QDataStream &out, const std::monostate &) {
  return out;
}
inline QDataStream &operator>>(QDataStream &in, std::monostate &) { return in; }

template <typename T, typename U>
QDataStream &operator>>(QDataStream &in, std::map<T, U> &a) {
  quint64 size;
  in >> size;
  for (quint64 i = 0; i < size; ++i) {
    T k;
    U v;
    in >> k >> v;
    a.insert({k, v});
  }
  return in;
}

template <typename T, typename U>
QDataStream &operator<<(QDataStream &out, const std::map<T, U> &a) {
  out << quint64(a.size());
  for (const auto &[k, v] : a) out << k << v;
  return out;
}

#include <QTextStream>

template <typename... Args>
QTextStream &operator<<(QTextStream &out, const std::variant<Args...> &a) {
  std::visit([&out](const auto &v) { out << v; }, a);
  return out;
}

#endif  // SERIALIZEVARIANT_H
