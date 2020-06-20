#ifndef SERIALIZEVARIANT_H
#define SERIALIZEVARIANT_H

#include <QDataStream>
#include <variant>

// Some scourging the internet to find a way to deserialize a variant b/c
// variants work well for deduping code for the msg protocol
// https://github.com/nlohmann/json/issues/1261
namespace detail {
template <std::size_t N>
struct variant_switch {
  template <typename Variant>
  inline void operator()(qsizetype index, QDataStream &in, Variant &v) const {
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
  inline void operator()(qsizetype index, QDataStream &in, Variant &v) const {
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
  size_t index;
  in >> *(qsizetype *)(&index);
  ::detail::variant_switch<sizeof...(Args) - 1>{}(index, in, a);
  return in;
}

template <typename... Args>
QDataStream &operator<<(QDataStream &out, const std::variant<Args...> &a) {
  out << qsizetype(a.index());
  std::visit([&out](auto &&v) { out << v; }, a);
  return out;
}

#include <QTextStream>
#include <typeinfo>

template <typename... Args>
QTextStream &operator<<(QTextStream &out, const std::variant<Args...> &a) {
  std::visit([&out](auto &&v) { out << v; }, a);
  return out;
}

#endif  // SERIALIZEVARIANT_H
