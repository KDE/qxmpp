// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPJID_H
#define QXMPPJID_H

#include "QXmppGlobal.h"

#include <compare>
#include <optional>
#include <variant>

#include <QHashFunctions>
#include <QString>
#include <QStringBuilder>

class QDebug;

namespace QXmpp {

struct QXMPP_EXPORT JidParsingError {
    enum Type {
        Empty,
        InvalidLocalpart,
        InvalidDomainpart,
        InvalidResourcepart,
    };

    Type type = Empty;
    QString description;

    bool operator==(const JidParsingError &) const = default;
};

class QXMPP_EXPORT Jid
{
public:
    Jid() = default;

    static std::variant<Jid, JidParsingError> parse(QStringView jid);
    static std::optional<Jid> fromString(QStringView jid);
    static std::variant<Jid, JidParsingError> fromParts(QStringView localpart, QStringView domain, QStringView resource = {});
    static std::variant<Jid, JidParsingError> fromUserInput(QStringView input);

    static QString escapeLocalpart(QStringView localpart);
    static QString unescapeLocalpart(QStringView localpart);

    bool isNull() const { return m_jid.isEmpty(); }
    bool isBare() const { return !isNull() && m_slashPos < 0; }
    bool isFull() const { return m_slashPos >= 0; }
    bool isDomain() const { return !isNull() && m_atPos < 0 && m_slashPos < 0; }
    bool hasLocalpart() const { return m_atPos >= 0; }
    bool hasResource() const { return m_slashPos >= 0; }

    QStringView localpart() const;
    QString unescapedLocalpart() const;
    QStringView domain() const;
    QByteArray domainAce() const;
    QStringView resource() const;

    Jid bare() const;
    Jid domainJid() const;
    std::variant<Jid, JidParsingError> withResource(QStringView resource) const;

    QString toString() const { return m_jid; }
    operator QString() const { return m_jid; }
    QString toDisplayString() const;

    friend bool operator==(const Jid &a, const Jid &b) { return a.m_jid == b.m_jid; }
    friend std::strong_ordering operator<=>(const Jid &a, const Jid &b) { return a.m_jid.compare(b.m_jid) <=> 0; }
    friend size_t qHash(const Jid &jid, size_t seed = 0) noexcept { return qHash(jid.m_jid, seed); }

private:
    Jid(QString jid, qsizetype atPos, qsizetype slashPos);
    static std::variant<Jid, JidParsingError> build(std::optional<QStringView> localpart, QStringView domain, std::optional<QStringView> resource);

    QString m_jid;
    qsizetype m_atPos = -1;
    qsizetype m_slashPos = -1;
};

QXMPP_EXPORT QDebug operator<<(QDebug debug, const Jid &jid);

}  // namespace QXmpp

// Allows concatenating a Jid with strings, also with QStringBuilder.
template<>
struct QConcatenable<QXmpp::Jid> : QConcatenable<QString> {
    using type = QXmpp::Jid;
    static qsizetype size(const QXmpp::Jid &jid) { return jid.toString().size(); }
    static void appendTo(const QXmpp::Jid &jid, QChar *&out) { QConcatenable<QString>::appendTo(jid.toString(), out); }
};

#endif  // QXMPPJID_H
