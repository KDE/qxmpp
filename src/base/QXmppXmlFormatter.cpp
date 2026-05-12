// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@jahnson.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppXmlFormatter.h"

#include "StringLiterals.h"

#include <QStringBuilder>
#include <QXmlStreamReader>

namespace {

constexpr QStringView CReset = u"\x1b[0m";
constexpr QStringView CDelim = u"\x1b[90m";    // bright black: '<' '>' '/' '?' '='
constexpr QStringView CName = u"\x1b[34m";     // blue: element local name
constexpr QStringView CPrefix = u"\x1b[35m";   // magenta: ns prefix
constexpr QStringView CAttr = u"\x1b[36m";     // cyan: attribute name
constexpr QStringView CValue = u"\x1b[32m";    // green: attribute value
constexpr QStringView CComment = u"\x1b[90m";  // dim grey: comment / PI

struct Frame {
    QString qname;
    bool tagOpen = false;
    bool hadChild = false;
    bool hadText = false;
};

QString escapeText(QStringView text)
{
    QString out;
    out.reserve(text.size());
    for (auto c : text) {
        if (c == u'<') {
            out += u"&lt;"_s;
        } else if (c == u'>') {
            out += u"&gt;"_s;
        } else if (c == u'&') {
            out += u"&amp;"_s;
        } else {
            out += c;
        }
    }
    return out;
}

QString escapeAttr(QStringView text)
{
    QString out;
    out.reserve(text.size());
    for (auto c : text) {
        if (c == u'<') {
            out += u"&lt;"_s;
        } else if (c == u'&') {
            out += u"&amp;"_s;
        } else if (c == u'"') {
            out += u"&quot;"_s;
        } else if (c == u'\n') {
            out += u"&#10;"_s;
        } else if (c == u'\r') {
            out += u"&#13;"_s;
        } else if (c == u'\t') {
            out += u"&#9;"_s;
        } else {
            out += c;
        }
    }
    return out;
}

void emitName(QString &out, QStringView prefix, QStringView localName, QStringView nameColor, bool colorize)
{
    if (!prefix.isEmpty()) {
        if (colorize) {
            out += CPrefix;
        }
        out += prefix.toString();
        if (colorize) {
            out += CReset + CDelim;
        }
        out += u':';
        if (colorize) {
            out += CReset;
        }
    }
    if (colorize) {
        out += nameColor;
    }
    out += localName.toString();
    if (colorize) {
        out += CReset;
    }
}

void emitStartTag(const QXmlStreamReader &r, QString &out, bool colorize)
{
    if (colorize) {
        out += CDelim;
    }
    out += u'<';
    if (colorize) {
        out += CReset;
    }
    emitName(out, r.prefix(), r.name(), CName, colorize);

    for (const auto &decl : r.namespaceDeclarations()) {
        out += u' ';
        if (colorize) {
            out += CAttr;
        }
        if (decl.prefix().isEmpty()) {
            out += u"xmlns"_s;
        } else {
            out += u"xmlns:"_s + decl.prefix().toString();
        }
        if (colorize) {
            out += CReset + CDelim;
        }
        out += u'=';
        if (colorize) {
            out += CReset + CValue;
        }
        out += u'"' + escapeAttr(decl.namespaceUri()) + u'"';
        if (colorize) {
            out += CReset;
        }
    }

    for (const auto &attr : r.attributes()) {
        out += u' ';
        if (!attr.prefix().isEmpty()) {
            if (colorize) {
                out += CPrefix;
            }
            out += attr.prefix().toString();
            if (colorize) {
                out += CReset + CDelim;
            }
            out += u':';
            if (colorize) {
                out += CReset;
            }
        }
        if (colorize) {
            out += CAttr;
        }
        out += attr.name().toString();
        if (colorize) {
            out += CReset + CDelim;
        }
        out += u'=';
        if (colorize) {
            out += CReset + CValue;
        }
        out += u'"' + escapeAttr(attr.value()) + u'"';
        if (colorize) {
            out += CReset;
        }
    }
}

void emitEndTag(const QString &qname, QString &out, bool colorize)
{
    if (colorize) {
        out += CDelim;
    }
    out += u"</"_s;
    if (colorize) {
        out += CReset;
    }
    // qname has form "prefix:local" or just "local"
    auto colon = qname.indexOf(u':');
    if (colon >= 0) {
        emitName(out, QStringView(qname).left(colon), QStringView(qname).mid(colon + 1), CName, colorize);
    } else {
        emitName(out, {}, qname, CName, colorize);
    }
    if (colorize) {
        out += CDelim;
    }
    out += u'>';
    if (colorize) {
        out += CReset;
    }
}

void emitSelfClose(QString &out, bool colorize)
{
    if (colorize) {
        out += CDelim;
    }
    out += u"/>"_s;
    if (colorize) {
        out += CReset;
    }
}

void emitOpenClose(QString &out, bool colorize)
{
    if (colorize) {
        out += CDelim;
    }
    out += u'>';
    if (colorize) {
        out += CReset;
    }
}

// Returns the formatted output if `trimmed` is a single end tag like
// "</stream:stream>" — used to colorize XMPP stream-close fragments which
// can't be parsed as a complete document.
std::optional<QString> tryFormatSoloEndTag(QStringView trimmed, bool colorize)
{
    if (!trimmed.startsWith(u"</") || !trimmed.endsWith(u'>')) {
        return std::nullopt;
    }
    auto inner = trimmed.mid(2, trimmed.size() - 3).trimmed();
    if (inner.isEmpty()) {
        return std::nullopt;
    }
    for (auto c : inner) {
        if (!(c.isLetterOrNumber() || c == u'_' || c == u'-' || c == u'.' || c == u':')) {
            return std::nullopt;
        }
    }
    QStringView prefix;
    QStringView local = inner;
    if (auto colon = inner.indexOf(u':'); colon >= 0) {
        prefix = inner.left(colon);
        local = inner.mid(colon + 1);
    }
    QString out;
    if (colorize) {
        out += CDelim;
    }
    out += u"</"_s;
    if (colorize) {
        out += CReset;
    }
    emitName(out, prefix, local, CName, colorize);
    if (colorize) {
        out += CDelim;
    }
    out += u'>';
    if (colorize) {
        out += CReset;
    }
    return out;
}

}  // namespace

namespace QXmpp {

QString formatXmlForDebug(QStringView raw, bool indent, int indentWidth, bool colorize)
{
    if (raw.isEmpty()) {
        return raw.toString();
    }

    // Strip leading XML declaration so we can wrap the rest in a synthetic root.
    QStringView trimmed = raw.trimmed();
    QString xmlDecl;
    if (trimmed.startsWith(u"<?xml")) {
        auto end = trimmed.indexOf(u"?>");
        if (end >= 0) {
            xmlDecl = trimmed.left(end + 2).toString();
            trimmed = trimmed.mid(end + 2).trimmed();
        }
    }

    auto withXmlDecl = [&](QString tail) -> QString {
        if (xmlDecl.isEmpty()) {
            return tail;
        }
        QString head;
        if (colorize) {
            head = CComment.toString() + xmlDecl + CReset.toString();
        } else {
            head = xmlDecl;
        }
        if (indent && !tail.isEmpty()) {
            head += u'\n';
        }
        return head + tail;
    };

    // Solo end tag like "</stream:stream>" — can't be parsed as a document.
    if (auto endTag = tryFormatSoloEndTag(trimmed, colorize)) {
        return withXmlDecl(*endTag);
    }

    // Wrap with a synthetic root that pre-declares common XMPP prefixes so
    // fragments like "<stream:features>..." parse successfully.
    static constexpr QStringView WrapStart =
        u"<__qxmpp_root__"
        u" xmlns:stream=\"http://etherx.jabber.org/streams\""
        u" xmlns:db=\"jabber:server:dialback\""
        u">";
    static constexpr QStringView WrapEnd = u"</__qxmpp_root__>";

    QString wrapped = WrapStart.toString() + trimmed.toString() + WrapEnd.toString();
    QXmlStreamReader r(wrapped);

    // Read until we are inside the synthetic root.
    bool entered = false;
    while (r.readNext() != QXmlStreamReader::Invalid) {
        if (r.hasError()) {
            return raw.toString();
        }
        if (r.tokenType() == QXmlStreamReader::StartElement &&
            r.name() == u"__qxmpp_root__") {
            entered = true;
            break;
        }
    }
    if (!entered) {
        return raw.toString();
    }

    QString out;
    if (!xmlDecl.isEmpty()) {
        if (colorize) {
            out += CComment + xmlDecl + CReset;
        } else {
            out += xmlDecl;
        }
    }

    QList<Frame> stack;

    auto indentString = [&](int level) {
        return indent ? (u'\n' + QString(level * indentWidth, u' ')) : QString();
    };

    while (!r.atEnd()) {
        r.readNext();
        if (r.hasError()) {
            // Recovery: if we successfully entered the wrapper and have at
            // least one element on the stack, the error is most likely the
            // wrapper-close mismatch caused by an unclosed fragment (such as
            // a `<stream:stream ...>` opener). Finalize the innermost open
            // start tag and return what we have. Only the innermost frame
            // can still be tagOpen since pushing a child closes its parent.
            if (!stack.isEmpty()) {
                if (stack.last().tagOpen) {
                    emitOpenClose(out, colorize);
                }
                return out;
            }
            return raw.toString();
        }

        switch (r.tokenType()) {
        case QXmlStreamReader::StartElement: {
            // Close parent's open tag if needed
            if (!stack.isEmpty() && stack.last().tagOpen) {
                emitOpenClose(out, colorize);
                stack.last().tagOpen = false;
            }
            if (!stack.isEmpty()) {
                stack.last().hadChild = true;
            }
            // Newline + indent unless this is the very first emitted byte
            if (!out.isEmpty()) {
                out += indentString(stack.size());
            }
            emitStartTag(r, out, colorize);
            stack.append({ r.qualifiedName().toString(), true, false, false });
            break;
        }
        case QXmlStreamReader::EndElement: {
            if (r.qualifiedName() == u"__qxmpp_root__") {
                return out;
            }
            if (stack.isEmpty()) {
                return raw.toString();
            }
            Frame f = stack.takeLast();
            if (f.tagOpen) {
                emitSelfClose(out, colorize);
            } else {
                if (f.hadChild) {
                    out += indentString(stack.size());
                }
                emitEndTag(f.qname, out, colorize);
            }
            break;
        }
        case QXmlStreamReader::Characters: {
            if (stack.isEmpty()) {
                break;
            }
            auto text = r.text();
            bool whitespaceOnly = text.trimmed().isEmpty();
            if (indent && whitespaceOnly) {
                // Drop existing whitespace between elements; we add our own.
                break;
            }
            if (stack.last().tagOpen) {
                emitOpenClose(out, colorize);
                stack.last().tagOpen = false;
            }
            out += escapeText(text);
            if (!whitespaceOnly) {
                stack.last().hadText = true;
            }
            break;
        }
        case QXmlStreamReader::Comment: {
            if (!stack.isEmpty() && stack.last().tagOpen) {
                emitOpenClose(out, colorize);
                stack.last().tagOpen = false;
                stack.last().hadChild = true;
            } else if (!stack.isEmpty()) {
                stack.last().hadChild = true;
            }
            if (!out.isEmpty()) {
                out += indentString(stack.size());
            }
            if (colorize) {
                out += CComment;
            }
            out += u"<!--"_s + r.text().toString() + u"-->"_s;
            if (colorize) {
                out += CReset;
            }
            break;
        }
        case QXmlStreamReader::ProcessingInstruction: {
            if (!stack.isEmpty() && stack.last().tagOpen) {
                emitOpenClose(out, colorize);
                stack.last().tagOpen = false;
                stack.last().hadChild = true;
            } else if (!stack.isEmpty()) {
                stack.last().hadChild = true;
            }
            if (!out.isEmpty()) {
                out += indentString(stack.size());
            }
            if (colorize) {
                out += CComment;
            }
            out += u"<?"_s + r.processingInstructionTarget().toString();
            if (!r.processingInstructionData().isEmpty()) {
                out += u' ' + r.processingInstructionData().toString();
            }
            out += u"?>"_s;
            if (colorize) {
                out += CReset;
            }
            break;
        }
        default:
            break;
        }
    }

    return out;
}

}  // namespace QXmpp
