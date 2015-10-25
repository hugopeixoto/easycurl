#ifndef RESULT_H_
#define RESULT_H_

#include <functional>

template <typename T> struct Optional;
template <typename T, typename E> class Result;

template <typename T, typename E> class Result {
  bool ok_;
  T value;
  E error;

public:
  typedef T ValueType;
  typedef E ErrorType;

  bool is_ok() const { return ok_; }

  Optional<E> err() const {
    if (is_ok()) {
      return Optional<E>::none();
    } else {
      return Optional<E>::some(error);
    }
  }

  static Result<T, E> from_error(const E &e) {
    Result<T, E> r;
    r.ok_ = false;
    r.error = e;

    return r;
  }

  static Result<T, E> from_result(const T &v) {
    Result<T, E> r;
    r.ok_ = true;
    r.value = v;

    return r;
  }

  template <typename F> auto map(F tr) const {
    typedef decltype(tr(value)) T2;

    if (is_ok()) {
      return Result<T2, E>::from_result(tr(value));
    } else {
      return Result<T2, E>::from_error(error);
    }
  }

  template <typename F> auto and_then(F tr) const {
    typedef typename decltype(tr(value))::ValueType T2;

    if (is_ok()) {
      return tr(value);
    } else {
      return Result<T2, E>::from_error(error);
    }
  }

  T value_or(const T &fallback) {
    if (is_ok()) {
      return value;
    } else {
      return fallback;
    }
  }

  Optional<T> ok() const {
    if (is_ok()) {
      return Optional<T>::some(value);
    } else {
      return Optional<T>::none();
    }
  }
};

template <typename T> class Optional {
  bool ok;
  T value;

public:
  typedef T ValueType;

  static Optional<T> none() {
    Optional<T> o;
    o.ok = false;
    return o;
  }

  static Optional<T> some(const T &v) {
    Optional<T> o;
    o.ok = true;
    o.value = v;
    return o;
  }

  bool is_none() const { return !ok; }

  bool is_some() const { return ok; }

  template <typename F>
  auto and_then(F tr) const {
    typedef typename decltype(tr(value))::ValueType T2;

    if (ok) {
      return tr(value);
    } else {
      return Optional<T2>::none();
    }
  }

  template <typename T2> auto and_(const T2& v) const {
    return and_then([&v](const T&){ return v; });
  }

  template <typename F>
  auto or_else(F tr) {
    if (ok) {
      return *this;
    } else {
      return tr();
    }
  }

  template <typename F> auto map(F tr) {
    typedef decltype(tr(value)) T2;

    if (ok) {
      return Optional<T2>::some(tr(value));
    } else {
      return Optional<T2>::none();
    }
  }

  T value_or(const T &fallback) {
    if (ok) {
      return value;
    } else {
      return fallback;
    }
  }

  template <typename F>
  auto value_or_else(F tr) {
    if (ok) {
      return value;
    } else {
      return tr();
    }
  }

  template <typename E> Result<T, E> ok_or(const E &error) {
    if (ok) {
      return Result<T, E>::from_result(value);
    } else {
      return Result<T, E>::from_error(error);
    }
  }

  template <typename F> auto ok_or_else(const F &f) {
    typedef decltype(f()) E;

    if (ok) {
      return Result<T, E>::from_result(value);
    } else {
      return Result<T, E>::from_error(f());
    }
  }
};

#endif
