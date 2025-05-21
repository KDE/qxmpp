// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef XMLWRITER_H
#define XMLWRITER_H

#include <optional>

#include <QXmlStreamWriter>

class XmlWriter
{
public:
    explicit XmlWriter(QXmlStreamWriter *writer) noexcept : w(writer) { }

    // attributes
    void writeOptionalAttribute(QStringView name, QStringView value);

    // empty element
    void writeEmptyElement(QStringView name, QStringView xmlns);

    // text elements
    void writeTextElement(QStringView name, QStringView value);
    void writeTextElement(QStringView name, QStringView xmlns, QStringView value);
    void writeOptionalTextElement(QStringView name, QStringView value);

    // empty elements with one attribute
    void writeSingleAttributeElement(QStringView name, QStringView attribute, QStringView value);
    template<typename Container>
    void writeSingleAttributeElements(QStringView name, QStringView attribute, const Container &values)
    {
        for (const auto &value : values) {
            writeSingleAttributeElement(w, name, attribute, value);
        }
    }
    template<typename Container>
    void writeTextElements(QStringView name, const Container &values)
    {
        for (const auto &value : values) {
            writeXmlTextElement(name, value);
        }
    }

    // serialization of other objects
    template<typename T>
    inline void writeOptionalElement(const std::optional<T> &value)
    {
        if (value) {
            value->toXml(w);
        }
    }
    void writeElements(const auto &elements)
    {
        for (const auto &element : elements) {
            element.toXml(w);
        }
    }

private:
    QXmlStreamWriter *w;
};

#endif  // XMLWRITER_H
