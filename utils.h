#pragma once

#include <variant>

template <typename T, typename E>
class result : private std::variant<T, E>
{
    typedef std::variant<T, E> VariantType;
    VariantType m_variant;
    bool m_isError = false;

    result<T, E>(const E &e, bool /*isErrorDummy*/) : m_variant(e), m_isError(true){};

public:
    result<T, E>() = default;
    ~result<T, E>() = default;

    result<T, E>(const result<T, E> &) = default;
    result<T, E> &operator=(const result<T, E> &) = default;

    result<T, E>(result<T, E> &&) = default;
    result<T, E> &operator=(result<T, E> &&) = default;

    // explicit result<T, E>(T v) : m_variant(v) {}
    result<T, E>(T &v) : m_variant(v) {}
    result<T, E>(const T &v) : m_variant(v) {}
    explicit result<T, E>(T &&v) : m_variant(std::move(v)) {}
    explicit result<T, E>(const E &e) : m_variant(e), m_isError(true) {}

    constexpr T *operator->() { return &value(); }
    constexpr const T *operator->() const { return &value(); }
    constexpr T &operator*() { return value(); }
    constexpr const T &operator*() const { return value(); }

    static result<T, E> Ok(const T &v) { return v; }
    static result<T, E> Err(const E &e) { return result<T, E>(e, true); }

    constexpr bool isOk() const { return !m_isError; }
    constexpr bool isErr() const { return !isOk(); }

    constexpr T &value() { return std::get<T>(m_variant); }
    constexpr const T &value() const { return std::get<const T>(m_variant); }
    constexpr E &error() { return std::get<E>(m_variant); }
    constexpr const E &error() const { return std::get<const E>(m_variant); }
};
