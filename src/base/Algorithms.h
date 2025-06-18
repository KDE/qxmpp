// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include <algorithm>
#include <functional>
#include <optional>
#include <ranges>
#include <variant>

namespace QXmpp::Private {

template<typename T>
concept HasShrinkToFit = requires(const T &t) {
    t.shrink_to_fit();
};

template<typename T>
concept HasSqueeze = requires(const T &t) {
    t.squeeze();
};

template<typename T>
concept HasPushBack = requires(T t, T::value_type value) {
    t.push_back(value);
};

template<typename T>
concept HasInsert = requires(T t, T::value_type value) {
    t.insert(value);
};

template<typename OutputVector, typename InputVector, typename Converter>
auto transform(const InputVector &input, Converter convert)
{
    OutputVector output;
    if constexpr (std::ranges::sized_range<InputVector>) {
        output.reserve(input.size());
    }
    for (const auto &value : input) {
        if constexpr (HasPushBack<OutputVector>) {
            output.push_back(std::invoke(convert, value));
        } else if constexpr (HasInsert<OutputVector>) {
            output.insert(std::invoke(convert, value));
        } else {
            static_assert(false, "OutputVector must support push_back() or insert()");
        }
    }
    return output;
}

template<typename OutputVector, typename InputVector, typename Converter>
auto transform(InputVector &&input, Converter convert)
{
    OutputVector output;
    if constexpr (std::ranges::sized_range<InputVector>) {
        output.reserve(input.size());
    }
    for (auto it = input.begin(); it != input.end(); ++it) {
        if constexpr (HasPushBack<OutputVector>) {
            output.push_back(std::invoke(convert, std::move(*it)));
        } else if constexpr (HasInsert<OutputVector>) {
            output.insert(std::invoke(convert, std::move(*it)));
        } else {
            static_assert(false, "OutputVector must support push_back() or insert()");
        }
    }
    return output;
}

template<typename OutputVector, typename InputVector, typename Converter>
auto transformFilter(const InputVector &input, Converter convert)
{
    using OutputValue = typename OutputVector::value_type;
    OutputVector output;
    if constexpr (std::ranges::sized_range<InputVector>) {
        output.reserve(input.size());
    }
    for (const auto &value : input) {
        if (const std::optional<OutputValue> result = std::invoke(convert, value)) {
            output.push_back(*result);
        }
    }
    if constexpr (std::ranges::sized_range<InputVector>) {
        if constexpr (HasShrinkToFit<OutputVector>) {
            output.shrink_to_fit();
        } else if constexpr (HasSqueeze<OutputVector>) {
            output.squeeze();
        }
    }
    return output;
}

// std::ranges::contains is C++23
template<typename Container, typename... Args>
auto contains(const Container &container, Args &&...args) -> bool
{
    return std::ranges::find(container, std::forward<Args>(args)...) != std::end(container);
}

template<typename Container, typename... Args>
auto find(const Container &container, Args &&...args) -> std::optional<typename Container::value_type>
{
    auto it = std::ranges::find(container, std::forward<Args>(args)...);
    if (it != std::end(container)) {
        return *it;
    }
    return {};
}

template<typename Container, typename... Args>
auto removeIf(Container &container, Args &&...args)
{
    auto removedRange = std::ranges::remove_if(container, std::forward<Args>(args)...);
    container.erase(removedRange.begin(), removedRange.end());
}

template<typename T, typename Function>
auto map(Function mapValue, std::optional<T> &&optValue) -> std::optional<std::invoke_result_t<Function, T &&>>
{
    if (optValue) {
        return mapValue(std::move(*optValue));
    }
    return {};
}

template<typename T, typename Function>
auto map(Function mapValue, const std::optional<T> &optValue) -> std::optional<std::invoke_result_t<Function, T &&>>
{
    if (optValue) {
        return mapValue(*optValue);
    }
    return {};
}

template<typename Out, typename Function, typename... InTypes>
auto map(Function mapValue, std::variant<InTypes...> &&variant)
{
    return std::visit(
        [&](auto &&v) -> Out {
            if constexpr (std::is_invocable_v<Function, decltype(v)>) {
                return { mapValue(std::move(v)) };
            } else {
                return { v };
            }
        },
        std::move(variant));
}

template<typename Out, typename Function, typename... InTypes>
auto map(Function mapValue, const std::variant<InTypes...> &variant)
{
    return std::visit(
        [&](const auto &v) -> Out {
            if constexpr (std::is_invocable_v<Function, decltype(v)>) {
                return { mapValue(v) };
            } else {
                return { v };
            }
        },
        variant);
}

template<typename To, typename From>
auto into(std::optional<From> &&value) -> std::optional<To>
{
    if (value) {
        return To { *value };
    }
    return {};
}

template<typename To, typename From>
auto into(const std::optional<From> &value) -> std::optional<To>
{
    if (value) {
        return To { *value };
    }
    return {};
}

template<typename GreaterVariant, typename... BaseTypes>
auto into(std::variant<BaseTypes...> &&variant)
{
    return std::visit([](auto &&value) -> GreaterVariant { return value; }, std::move(variant));
}

template<typename GreaterVariant, typename... BaseTypes>
auto into(const std::variant<BaseTypes...> &variant)
{
    return std::visit([](const auto &value) -> GreaterVariant { return value; }, variant);
}

}  // namespace QXmpp::Private

#endif  // ALGORITHMS_H
