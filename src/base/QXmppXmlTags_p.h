// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPXMLTAGS_P_H
#define QXMPPXMLTAGS_P_H

#include <tuple>

#include <QStringView>

namespace QXmpp::Private {

template<typename Name, std::convertible_to<QStringView> Xmlns>
struct Tag {
    Name name;
    Xmlns xmlns;

    using NameType = Name;
    using XmlnsType = Xmlns;

    Tag(Name name, Xmlns xmlns)
        : name(std::forward<Name>(name)), xmlns(std::forward<Xmlns>(xmlns)) { }

    template<typename N, typename X>
    Tag(std::tuple<N, X> &&t) : name(std::get<0>(t)), xmlns(std::get<1>(t)) { }
    template<typename N, typename X>
    Tag(const std::tuple<N, X> &t) : name(std::get<0>(t)), xmlns(std::get<1>(t)) { }
};

template<size_t N, std::convertible_to<QStringView> Xmlns>
Tag(const char16_t (&str)[N], Xmlns &&) -> Tag<QStringView, Xmlns>;

template<typename Name, std::convertible_to<QStringView> Xmlns>
Tag(Name &&, Xmlns &&) -> Tag<Name, Xmlns>;

template<typename Name, typename Xmlns>
Tag(std::tuple<Name, Xmlns> &&) -> Tag<Name, Xmlns>;

template<typename T>
concept IsXmlTag =
    std::is_convertible_v<T, Tag<QStringView, QStringView>> || requires() {
        typename T::NameType;
        typename T::XmlnsType;
        requires std::same_as<std::remove_cvref_t<T>, Tag<typename T::NameType, typename T::XmlnsType>>;
    };

template<typename T>
concept HasXmlTag =
    !std::is_void_v<T> &&
    requires { { T::XmlTag }; } &&
    IsXmlTag<decltype(T::XmlTag)>;

template<typename T>
concept HasPayloadXmlTagDirect =
    requires { { T::PayloadXmlTag }; } &&
    IsXmlTag<decltype(T::PayloadXmlTag)>;

template<typename T>
concept HasPayloadXmlTagIndirect =
    requires { typename T::PayloadType; } &&
    IsXmlTag<decltype(T::PayloadType::XmlTag)>;

template<typename T>
concept HasPayloadXmlTag = HasPayloadXmlTagDirect<T> || HasPayloadXmlTagIndirect<T>;

template<HasPayloadXmlTag T>
constexpr auto PayloadXmlTag = [] {
    if constexpr (HasPayloadXmlTagIndirect<T>) {
        return T::PayloadType::XmlTag;
    } else if constexpr (HasPayloadXmlTagDirect<T>) {
        return T::PayloadXmlTag;
    }
}();

template<typename T>
concept HasCustomCheckIqType =
    requires(const QString &s1, const QString &s2) {
        { T::checkIqType(s1, s2) } -> std::convertible_to<bool>;
    };

template<typename T, IsXmlTag Tag>
bool isPayloadType(Tag xmlTag)
{
    if constexpr (Private::HasPayloadXmlTag<T>) {
        return xmlTag == Private::PayloadXmlTag<T>;
    } else if constexpr (HasCustomCheckIqType<T>) {
        static_assert(std::convertible_to<std::tuple_element_t<0, Tag>, QString>);
        static_assert(std::convertible_to<std::tuple_element_t<1, Tag>, QString>);

        auto &[name, xmlns] = xmlTag;
        return T::checkIqType(name, xmlns);
    } else {
        static_assert(false, "T must offer PayloadXmlTag, PayloadType::XmlTag or checkIqType()");
    }
}

}  // namespace QXmpp::Private

#endif  // QXMPPXMLTAGS_P_H
