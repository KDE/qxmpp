// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef ENUMS_H
#define ENUMS_H

#include <array>
#include <optional>
#include <ranges>

#include <QList>
#include <QString>

namespace QXmpp::Private::Enums {

// std::array helper
namespace detail {

template<class T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N> to_array_impl(T (&&a)[N], std::index_sequence<I...>)
{
    return { { std::move(a[I])... } };
}

}  // namespace detail

template<typename Enum, std::size_t N>
consteval std::array<std::remove_cv_t<std::tuple<Enum, QStringView>>, N> makeValues(std::tuple<Enum, QStringView> (&&a)[N])
{
    return detail::to_array_impl(std::move(a), std::make_index_sequence<N> {});
}

template<typename Enum>
struct Data;

// order check
template<typename Enum, size_t N>
consteval bool checkEnumOrder(const std::array<std::tuple<Enum, QStringView>, N> &values)
{
    size_t offset = 0;
    if constexpr (N > 0) {
        offset = size_t(std::get<0>(values[0]));
    }
    for (size_t i = offset; i < (N + offset); ++i) {
        if (i != size_t(std::get<0>(values[i]))) {
            return false;
        }
    }
    return true;
}

template<typename Enum>
concept SerializableEnum = requires {
    typename Data<Enum>;

    requires std::ranges::random_access_range<decltype(Data<Enum>::Values)>;
    requires std::same_as<
        std::ranges::range_value_t<decltype(Data<Enum>::Values)>,
        std::tuple<Enum, QStringView>>;
    requires checkEnumOrder<Enum>(Data<Enum>::Values);
};

template<typename Enum>
concept SerializableFlags = requires {
    typename Data<Enum>;
    requires Data<Enum>::IsFlags == true;

    requires std::ranges::random_access_range<decltype(Data<Enum>::Values)>;
    requires std::same_as<
        std::ranges::range_value_t<decltype(Data<Enum>::Values)>,
        std::tuple<Enum, QStringView>>;
};

template<typename Enum>
concept NullableEnum = requires(Enum value) {
    typename Data<Enum>;
    { Data<Enum>::NullValue } -> std::convertible_to<Enum>;
};

template<typename Enum>
std::optional<Enum> fromString(QStringView str)
    requires SerializableEnum<Enum> || SerializableFlags<Enum>
{
    constexpr auto values = Data<Enum>::Values;

    auto enumPart = [](const auto &v) { return std::get<0>(v); };
    auto stringPart = [](const auto &v) { return std::get<1>(v); };

    if (auto itr = std::ranges::find(values, str, stringPart); itr != values.end()) {
        return enumPart(*itr);
    }
    return {};
}

template<SerializableEnum Enum>
QStringView toString(Enum value)
{
    // offset for enums that do not start at 0
    constexpr auto offset = size_t(std::get<0>(Data<Enum>::Values[0]));

    auto &[enumerator, string] = Data<Enum>::Values.at(size_t(value) - offset);
    return string;
};

template<SerializableFlags Enum, typename Container>
QFlags<Enum> fromStrings(const Container &container)
{
    QFlags<Enum> result;
    for (const auto &string : container) {
        if (auto flag = fromString<Enum>(string)) {
            result.setFlag(*flag);
        }
    }
    return result;
}

template<SerializableFlags Enum>
auto toStrings(QFlags<Enum> value)
{
    auto testFlag = [value](auto entry) {
        auto &[enumerator, string] = entry;
        return value.testFlag(enumerator);
    };
    auto string = [](auto entry) { return std::get<1>(entry); };

    return std::views::filter(Data<Enum>::Values, testFlag) | std::views::transform(string);
}

template<SerializableFlags Enum>
QList<QString> toStringList(QFlags<Enum> value)
{
    auto strings = toStrings(value) | std::views::transform(&QStringView::toString);
    return QList<QString>(strings.begin(), strings.end());
}

}  // namespace QXmpp::Private::Enums

#endif  // ENUMS_H
