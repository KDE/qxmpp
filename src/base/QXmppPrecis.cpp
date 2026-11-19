// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppPrecisData_p.h"
#include "QXmppPrecis_p.h"

#include "StringLiterals.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <vector>

#include <QList>

namespace QXmpp::Private::Precis {

namespace {

enum class StringClass {
    Identifier,
    Freeform,
};

// RFC 8264, section 8
enum class DerivedProperty {
    PValid,
    FreePValid,  // ID_DIS or FREE_PVAL
    ContextJ,
    ContextO,
    Disallowed,
    Unassigned,
};

using CodePoints = std::vector<char32_t>;

template<std::size_t N>
bool contains(const CodePointRange (&ranges)[N], char32_t cp)
{
    auto it = std::ranges::upper_bound(ranges, cp, {}, &CodePointRange::first);
    return it != std::begin(ranges) && cp <= std::prev(it)->last;
}

QString codePointName(char32_t cp)
{
    return u"U+%1"_s.arg(uint(cp), 4, 16, QChar(u'0')).toUpper();
}

std::optional<CodePoints> toCodePoints(QStringView string)
{
    CodePoints cps;
    cps.reserve(string.size());
    for (qsizetype i = 0; i < string.size(); ++i) {
        auto c = string[i].unicode();
        if (QChar::isHighSurrogate(c) && i + 1 < string.size() && QChar::isLowSurrogate(string[i + 1].unicode())) {
            cps.push_back(QChar::surrogateToUcs4(c, string[++i].unicode()));
        } else if (QChar::isSurrogate(c)) {
            return {};
        } else {
            cps.push_back(c);
        }
    }
    return cps;
}

QString fromCodePoints(const CodePoints &cps)
{
    return QString::fromUcs4(cps.data(), qsizetype(cps.size()));
}

// RFC 5892, section 2.6
std::optional<DerivedProperty> exception(char32_t cp)
{
    switch (cp) {
    case 0x00DF:
    case 0x03C2:
    case 0x06FD:
    case 0x06FE:
    case 0x0F0B:
    case 0x3007:
        return DerivedProperty::PValid;
    case 0x00B7:
    case 0x0375:
    case 0x05F3:
    case 0x05F4:
    case 0x30FB:
        return DerivedProperty::ContextO;
    case 0x0640:
    case 0x07FA:
    case 0x302E:
    case 0x302F:
    case 0x3031:
    case 0x3032:
    case 0x3033:
    case 0x3034:
    case 0x3035:
    case 0x303B:
        return DerivedProperty::Disallowed;
    }
    if ((cp >= 0x0660 && cp <= 0x0669) || (cp >= 0x06F0 && cp <= 0x06F9)) {
        return DerivedProperty::ContextO;
    }
    return {};
}

bool hasCompat(char32_t cp)
{
    auto string = QString::fromUcs4(&cp, 1);
    return string.normalized(QString::NormalizationForm_KC) != string;
}

// RFC 8264, section 8 (the order of the checks is normative)
DerivedProperty derivedProperty(char32_t cp)
{
    if (auto value = exception(cp)) {
        return *value;
    }
    // BackwardCompatible (RFC 5892, section 2.7) is empty.
    const auto category = QChar::category(cp);
    if (category == QChar::Other_NotAssigned && !QChar::isNonCharacter(cp)) {
        return DerivedProperty::Unassigned;
    }
    if (cp >= 0x21 && cp <= 0x7E) {
        return DerivedProperty::PValid;
    }
    if (cp == 0x200C || cp == 0x200D) {
        return DerivedProperty::ContextJ;
    }
    if (contains(oldHangulJamo, cp)) {
        return DerivedProperty::Disallowed;
    }
    if (contains(defaultIgnorable, cp) || QChar::isNonCharacter(cp)) {
        return DerivedProperty::Disallowed;
    }
    if (category == QChar::Other_Control) {
        return DerivedProperty::Disallowed;
    }
    if (hasCompat(cp)) {
        return DerivedProperty::FreePValid;
    }

    switch (category) {
    case QChar::Letter_Lowercase:
    case QChar::Letter_Uppercase:
    case QChar::Letter_Other:
    case QChar::Letter_Modifier:
    case QChar::Number_DecimalDigit:
    case QChar::Mark_NonSpacing:
    case QChar::Mark_SpacingCombining:
        return DerivedProperty::PValid;
    case QChar::Letter_Titlecase:
    case QChar::Number_Letter:
    case QChar::Number_Other:
    case QChar::Mark_Enclosing:
    case QChar::Separator_Space:
    case QChar::Symbol_Math:
    case QChar::Symbol_Currency:
    case QChar::Symbol_Modifier:
    case QChar::Symbol_Other:
    case QChar::Punctuation_Connector:
    case QChar::Punctuation_Dash:
    case QChar::Punctuation_Open:
    case QChar::Punctuation_Close:
    case QChar::Punctuation_InitialQuote:
    case QChar::Punctuation_FinalQuote:
    case QChar::Punctuation_Other:
        return DerivedProperty::FreePValid;
    default:
        return DerivedProperty::Disallowed;
    }
}

constexpr unsigned char viramaCombiningClass = 9;

// RFC 5892, appendix A.1 and A.2
bool contextJAllowed(const CodePoints &cps, std::size_t i)
{
    if (i > 0 && QChar::combiningClass(cps[i - 1]) == viramaCombiningClass) {
        return true;
    }
    if (cps[i] != 0x200C) {
        return false;
    }

    // (Joining_Type:{L,D})(Joining_Type:T)* U+200C (Joining_Type:T)*(Joining_Type:{R,D})
    auto isTransparent = [](char32_t cp) { return QChar::joiningType(cp) == QChar::Joining_Transparent; };

    auto before = std::ranges::find_if_not(cps | std::views::take(i) | std::views::reverse, isTransparent);
    if (before == (cps | std::views::take(i) | std::views::reverse).end()) {
        return false;
    }
    auto beforeType = QChar::joiningType(*before);
    if (beforeType != QChar::Joining_Left && beforeType != QChar::Joining_Dual) {
        return false;
    }

    auto after = std::find_if_not(cps.begin() + i + 1, cps.end(), isTransparent);
    if (after == cps.end()) {
        return false;
    }
    auto afterType = QChar::joiningType(*after);
    return afterType == QChar::Joining_Right || afterType == QChar::Joining_Dual;
}

// RFC 5892, appendix A.3 to A.9
bool contextOAllowed(const CodePoints &cps, std::size_t i)
{
    const auto cp = cps[i];
    const auto hasBefore = i > 0;
    const auto hasAfter = i + 1 < cps.size();

    switch (cp) {
    case 0x00B7:  // MIDDLE DOT
        return hasBefore && hasAfter && cps[i - 1] == 0x006C && cps[i + 1] == 0x006C;
    case 0x0375:  // GREEK LOWER NUMERAL SIGN (KERAIA)
        return hasAfter && QChar::script(cps[i + 1]) == QChar::Script_Greek;
    case 0x05F3:  // HEBREW PUNCTUATION GERESH
    case 0x05F4:  // HEBREW PUNCTUATION GERSHAYIM
        return hasBefore && QChar::script(cps[i - 1]) == QChar::Script_Hebrew;
    case 0x30FB:  // KATAKANA MIDDLE DOT
        return std::ranges::any_of(cps, [](char32_t c) {
            auto script = QChar::script(c);
            return script == QChar::Script_Hiragana || script == QChar::Script_Katakana || script == QChar::Script_Han;
        });
    }
    if (cp >= 0x0660 && cp <= 0x0669) {  // ARABIC-INDIC DIGITS
        return std::ranges::none_of(cps, [](char32_t c) { return c >= 0x06F0 && c <= 0x06F9; });
    }
    if (cp >= 0x06F0 && cp <= 0x06F9) {  // EXTENDED ARABIC-INDIC DIGITS
        return std::ranges::none_of(cps, [](char32_t c) { return c >= 0x0660 && c <= 0x0669; });
    }
    return false;
}

std::optional<Error> checkStringClass(const CodePoints &cps, StringClass stringClass)
{
    for (std::size_t i = 0; i < cps.size(); ++i) {
        bool allowed = false;
        switch (derivedProperty(cps[i])) {
        case DerivedProperty::PValid:
            allowed = true;
            break;
        case DerivedProperty::FreePValid:
            allowed = stringClass == StringClass::Freeform;
            break;
        case DerivedProperty::ContextJ:
            allowed = contextJAllowed(cps, i);
            break;
        case DerivedProperty::ContextO:
            allowed = contextOAllowed(cps, i);
            break;
        case DerivedProperty::Disallowed:
        case DerivedProperty::Unassigned:
            break;
        }
        if (!allowed) {
            return Error { u"Character %1 at position %2 is not allowed."_s.arg(codePointName(cps[i]), QString::number(i)) };
        }
    }
    return {};
}

// RFC 5893, section 2
std::optional<Error> checkBidiRule(const CodePoints &cps)
{
    auto isRtl = [](char32_t cp) {
        auto dir = QChar::direction(cp);
        return dir == QChar::DirR || dir == QChar::DirAL || dir == QChar::DirAN;
    };
    if (cps.empty() || std::ranges::none_of(cps, isRtl)) {
        return {};
    }

    auto error = [] { return Error { u"String violates the Bidi Rule (RFC 5893)."_s }; };

    const auto first = QChar::direction(cps.front());
    if (first != QChar::DirL && first != QChar::DirR && first != QChar::DirAL) {
        return error();
    }
    const bool rtl = first != QChar::DirL;

    auto last = std::ranges::find_if_not(cps | std::views::reverse, [](char32_t cp) { return QChar::direction(cp) == QChar::DirNSM; });
    if (last == (cps | std::views::reverse).end()) {
        return error();
    }
    const auto lastDir = QChar::direction(*last);

    bool hasEN = false;
    bool hasAN = false;
    for (auto cp : cps) {
        switch (QChar::direction(cp)) {
        case QChar::DirEN:
            hasEN = true;
            break;
        case QChar::DirAN:
            if (!rtl) {
                return error();
            }
            hasAN = true;
            break;
        case QChar::DirR:
        case QChar::DirAL:
            if (!rtl) {
                return error();
            }
            break;
        case QChar::DirL:
            if (rtl) {
                return error();
            }
            break;
        case QChar::DirES:
        case QChar::DirCS:
        case QChar::DirET:
        case QChar::DirON:
        case QChar::DirBN:
        case QChar::DirNSM:
            break;
        default:
            return error();
        }
    }

    if (rtl) {
        if (hasEN && hasAN) {
            return error();
        }
        if (lastDir != QChar::DirR && lastDir != QChar::DirAL && lastDir != QChar::DirEN && lastDir != QChar::DirAN) {
            return error();
        }
    } else if (lastDir != QChar::DirL && lastDir != QChar::DirEN) {
        return error();
    }
    return {};
}

Error invalidUtf16Error()
{
    return Error { u"String contains an unpaired surrogate."_s };
}

Error emptyError()
{
    return Error { u"String is empty."_s };
}

// RFC 8265, section 3.3.2 and 3.3.3
Result applyUsernameCaseMapped(QStringView input)
{
    auto cps = toCodePoints(input);
    if (!cps) {
        return invalidUtf16Error();
    }

    // width mapping
    CodePoints mapped;
    mapped.reserve(cps->size());
    for (auto cp : *cps) {
        auto tag = QChar::decompositionTag(cp);
        if (tag == QChar::Wide || tag == QChar::Narrow) {
            auto decomposition = QChar::decomposition(cp).toUcs4();
            mapped.insert(mapped.end(), decomposition.begin(), decomposition.end());
        } else {
            mapped.push_back(cp);
        }
    }

    if (auto error = checkStringClass(mapped, StringClass::Identifier)) {
        return *error;
    }

    // QString::toLower() does not apply the Final_Sigma context of toLowerCase(), which is
    // exactly what RFC 7622, section 3.5 expects (U+03A3 always maps to U+03C3, never to U+03C2).
    auto output = fromCodePoints(mapped).toLower().normalized(QString::NormalizationForm_C);

    auto outputCps = toCodePoints(output);
    if (auto error = checkBidiRule(*outputCps)) {
        return *error;
    }
    if (output.isEmpty()) {
        return emptyError();
    }
    return output;
}

// RFC 8265, section 4.2.1 and 4.2.2
Result applyOpaqueString(QStringView input)
{
    auto cps = toCodePoints(input);
    if (!cps) {
        return invalidUtf16Error();
    }

    if (auto error = checkStringClass(*cps, StringClass::Freeform)) {
        return *error;
    }

    for (auto &cp : *cps) {
        if (QChar::category(cp) == QChar::Separator_Space) {
            cp = u' ';
        }
    }

    auto output = fromCodePoints(*cps).normalized(QString::NormalizationForm_C);
    if (output.isEmpty()) {
        return emptyError();
    }
    return output;
}

// RFC 8264, section 7: re-apply until the output is stable, at most three additional times
template<typename Apply>
Result enforce(QStringView input, Apply apply)
{
    if (input.isEmpty()) {
        return emptyError();
    }

    auto result = apply(input);
    for (int i = 0; i < 3; ++i) {
        auto *output = std::get_if<QString>(&result);
        if (!output) {
            return result;
        }
        auto next = apply(*output);
        if (auto *nextOutput = std::get_if<QString>(&next); nextOutput && *nextOutput == *output) {
            return result;
        }
        result = std::move(next);
    }
    if (std::holds_alternative<Error>(result)) {
        return result;
    }
    return Error { u"String does not stabilize under PRECIS enforcement."_s };
}

bool isPrintableAscii(QStringView input, char16_t first)
{
    return std::ranges::all_of(input, [first](QChar c) { return c.unicode() >= first && c.unicode() <= 0x7E; });
}

}  // namespace

Result enforceUsernameCaseMapped(QStringView input)
{
    // Fast path: printable ASCII without space is always PVALID and only needs case mapping.
    if (!input.isEmpty() && isPrintableAscii(input, 0x21)) {
        QString output(input.size(), Qt::Uninitialized);
        std::ranges::transform(input, output.begin(), [](QChar c) {
            return c.unicode() >= u'A' && c.unicode() <= u'Z' ? QChar(c.unicode() + 0x20) : c;
        });
        return output;
    }
    return enforce(input, applyUsernameCaseMapped);
}

Result enforceOpaqueString(QStringView input)
{
    // Fast path: printable ASCII including space is always allowed and never mapped.
    if (!input.isEmpty() && isPrintableAscii(input, 0x20)) {
        return input.toString();
    }
    return enforce(input, applyOpaqueString);
}

}  // namespace QXmpp::Private::Precis
