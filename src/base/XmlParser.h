// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef XMLPARSER_H
#define XMLPARSER_H

#include "QXmppUtils_p.h"

#include "Enums.h"
#include "StringLiterals.h"

#include <format>

#include <QDomElement>
#include <QString>

namespace QXmpp::Private {

namespace XmlParsing {

template<typename Result = QString>
struct AttributeP {
    QString name;

    auto parse(QDomElement el)
    {
        auto attributeNode = el.attributeNode(name);
        if (attributeNode.isNull()) {
            throw MissingValueError(name, "attribute", el);
        }
        return StringParser<Result>::parse(attributeNode.value());
    }
};

template<typename Result = QString>
struct OptionalAttributeP {
    QString name;

    auto parse(QDomElement el)
    {
        auto attributeNode = el.attributeNode(name);
        if (attributeNode.isNull()) {
            return Result {};
        }
        return StringParser<Result>::parse(attributeNode.value());
    }
};

template<typename Result = QString>
struct TextP {
    auto parse(const QDomElement &el)
    {
        return StringParser<Result>::parse(el.text());
    }
};

template<typename Result>
    requires HasXmlTag<Result>
struct FirstElementP {
    auto parse(const QDomElement &el)
    {
        auto childEl = firstChildElement<Result>(el);
        if (childEl.isNull()) {
            auto &[name, xmlns] = Result::XmlTag;
            throw MissingElementError(name, el);
        }
        // TODO: pass errors from child element parsing
        auto result = parseElement<Result>(childEl);
        if (!result) {
            throw ParseError(std::format("Couldn't parse <{}/> element into {}", childEl.tagName(), typeid(Result).name()));
        }
        return std::move(*result);
    }
};

template<std::ranges::range Result>
    requires HasXmlTag<std::ranges::range_value_t<Result>>
struct ElementsP {
    Result parse(const QDomElement &el)
    {
        using ValueType = std::ranges::range_value_t<Result>;
        return parseChildElements<ValueType>(el);
    }
};

template<typename Result, typename... Contents>
auto parseDom(const QDomElement &el, Contents &&...contents)
{
    return Result { contents.parse(el)... };
}

}  // namespace XmlParsing

}  // namespace QXmpp::Private

#endif  // XMLPARSER_H
