// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppDataForm.h"

#include "QXmppConstants_p.h"
#include "QXmppDataFormBase.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <optional>

#include <QDebug>
#include <QDomElement>
#include <QMimeDatabase>
#include <QMimeType>
#include <QSize>
#include <QStringList>
#include <QUrl>

using namespace QXmpp::Private;

template<>
struct Enums::Data<QXmppDataForm::Field::Type> {
    using enum QXmppDataForm::Field::Type;
    static constexpr auto Values = makeValues<QXmppDataForm::Field::Type>({
        { BooleanField, u"boolean" },
        { FixedField, u"fixed" },
        { HiddenField, u"hidden" },
        { JidMultiField, u"jid-multi" },
        { JidSingleField, u"jid-single" },
        { ListMultiField, u"list-multi" },
        { ListSingleField, u"list-single" },
        { TextMultiField, u"text-multi" },
        { TextPrivateField, u"text-private" },
        { TextSingleField, u"text-single" },
    });
};

template<>
struct Enums::Data<QXmppDataForm::Type> {
    using enum QXmppDataForm::Type;
    static constexpr auto Values = makeValues<QXmppDataForm::Type>({
        { None, {} },
        { Form, u"form" },
        { Submit, u"submit" },
        { Cancel, u"cancel" },
        { Result, u"result" },
    });
};

class QXmppDataFormMediaSourcePrivate : public QSharedData
{
public:
    QUrl uri;
    QMimeType contentType;
};

///
/// \class QXmppDataForm::MediaSource
///
/// The QXmppDataForm::MediaSource class represents a link to one of possibly
/// multiple sources for a media element from \xep{0221, Data Forms Media
/// Element} consisting of a MIME type and a QUrl.
///
/// \since QXmpp 1.1
///

///
/// Default constructor
///
QXmppDataForm::MediaSource::MediaSource()
    : d(new QXmppDataFormMediaSourcePrivate)
{
}

///
/// Constructs a MediaSource and sets its uri and contentType.
///
/// \param uri
/// \param contentType
///
QXmppDataForm::MediaSource::MediaSource(const QUrl &uri, const QMimeType &contentType)
    : d(new QXmppDataFormMediaSourcePrivate)
{
    d->uri = uri;
    d->contentType = contentType;
}

/// Default copy-constructor
QXmppDataForm::MediaSource::MediaSource(const QXmppDataForm::MediaSource &) = default;
/// Default move-constructor
QXmppDataForm::MediaSource::MediaSource(QXmppDataForm::MediaSource &&) = default;
QXmppDataForm::MediaSource::~MediaSource() = default;
/// Default assignment operator
QXmppDataForm::MediaSource &QXmppDataForm::MediaSource::operator=(const QXmppDataForm::MediaSource &) = default;
/// Default move-assignment operator
QXmppDataForm::MediaSource &QXmppDataForm::MediaSource::operator=(QXmppDataForm::MediaSource &&) = default;

///
/// Returns the media URI as QUrl. This can be i.e. a \c http:// URL or a
/// \c cid: Bits of Binary URI.
///
QUrl QXmppDataForm::MediaSource::uri() const
{
    return d->uri;
}

///
/// Sets the URI.
///
void QXmppDataForm::MediaSource::setUri(const QUrl &uri)
{
    d->uri = uri;
}

///
/// Returns the content type of the source
///
QMimeType QXmppDataForm::MediaSource::contentType() const
{
    return d->contentType;
}

///
/// Sets the content type of the media source.
///
void QXmppDataForm::MediaSource::setContentType(const QMimeType &contentType)
{
    d->contentType = contentType;
}

///
/// Returns true if two media sources are identical.
///
bool QXmppDataForm::MediaSource::operator==(const QXmppDataForm::MediaSource &other) const
{
    return d->uri == other.uri() && d->contentType == other.contentType();
}

class QXmppDataFormMediaPrivate : public QSharedData
{
public:
    QSize size;
    QList<QPair<QString, QString>> uris;
};

///
/// \class QXmppDataForm::Media
///
/// The QXmppDataForm::Media class represents a media field as defined by
/// \xep{0221, Data Forms Media Element}.
///
/// \deprecated This class is deprecated since QXmpp 1.1.
///

///
/// Constructs an empty QXmppDataForm::Media.
///
/// \deprecated This class is deprecated since QXmpp 1.1.
///
QXmppDataForm::Media::Media()
    : d(new QXmppDataFormMediaPrivate)
{
}

///
/// Constructs a copy of \a other.
///
/// \deprecated This class is deprecated since QXmpp 1.1.
///
QXmppDataForm::Media::Media(const QXmppDataForm::Media &other) = default;

///
/// Destroys the media.
///
/// \deprecated This class is deprecated since QXmpp 1.1.
///
QXmppDataForm::Media::~Media() = default;

///
/// Assigns \a other to this media.
///
/// \deprecated This class is deprecated since QXmpp 1.1.
///
QXmppDataForm::Media &QXmppDataForm::Media::operator=(const QXmppDataForm::Media &other) = default;

///
/// Returns media's height.
///
/// \deprecated This method is deprecated since QXmpp 1.1. Use
/// \c QXmppDataForm::Field::mediaSize().height() instead.
///
int QXmppDataForm::Media::height() const
{
    return d->size.height();
}

///
/// Sets media's \a height.
///
/// \deprecated This method is deprecated since QXmpp 1.1. Use
/// \c QXmppDataForm::Field::mediaSize().setHeight() instead.
///
void QXmppDataForm::Media::setHeight(int height)
{
    d->size.setHeight(height);
}

///
/// Returns media's width.
///
/// \deprecated This method is deprecated since QXmpp 1.1. Use
/// \c QXmppDataForm::Field::mediaSize().width() instead.
///
int QXmppDataForm::Media::width() const
{
    return d->size.width();
}

///
/// Sets media's \a width.
///
/// \deprecated This method is deprecated since QXmpp 1.1. Use
/// \c QXmppDataForm::Field::mediaSize().setWidth() instead.
///
void QXmppDataForm::Media::setWidth(int width)
{
    d->size.setWidth(width);
}

///
/// Returns media's uris.
///
/// \deprecated This method is deprecated since QXmpp 1.1. Use
/// \c QXmppDataForm::Field::mediaSources() instead.
///
QList<QPair<QString, QString>> QXmppDataForm::Media::uris() const
{
    return d->uris;
}

///
/// Sets media's \a uris.
///
/// \deprecated This method is deprecated since QXmpp 1.1. Use
/// \c QXmppDataForm::Media::setMediaSources() instead.
///
void QXmppDataForm::Media::setUris(const QList<QPair<QString, QString>> &uris)
{
    d->uris = uris;
}

///
/// Returns true if no media tag present.
///
bool QXmppDataForm::Media::isNull() const
{
    return d->uris.isEmpty();
}

struct DataFormListOption {
    QString label;
    QString value;

    void toXml(XmlWriter &w) const
    {
        w.write(Element {
            u"option",
            OptionalAttribute { u"label", label },
            TextElement { u"value", value },
        });
    }

    bool operator==(const auto &other) const
    {
        return label == other.label && value == other.value;
    }
};

class QXmppDataFormFieldPrivate : public QSharedData
{
public:
    QString description;
    QString key;
    QString label;
    QList<DataFormListOption> options;
    bool required = false;
    QXmppDataForm::Field::Type type = QXmppDataForm::Field::TextSingleField;
    QVariant value;
    QSize mediaSize;
    QVector<QXmppDataForm::MediaSource> mediaSources;
};

///
/// \class QXmppDataForm::Field
///
/// The QXmppDataForm::Field class represents a data form field as defined by
/// \xep{0004, Data Forms}.
///

///
/// Constructs a QXmppDataForm::Field with the specified attributes.
///
/// \since QXmpp 1.3
///
QXmppDataForm::Field::Field(QXmppDataForm::Field::Type type,
                            const QString &key,
                            const QVariant &value,
                            bool isRequired,
                            const QString &label,
                            const QString &description,
                            const QList<QPair<QString, QString>> &options)
    : d(new QXmppDataFormFieldPrivate)
{
    d->type = type;
    d->key = key;
    d->value = value;
    d->required = isRequired;
    d->label = label;
    d->description = description;
    setOptions(options);
}

/// Constructs a copy of \a other.
QXmppDataForm::Field::Field(const QXmppDataForm::Field &other) = default;
/// Default move constructor.
QXmppDataForm::Field::Field(QXmppDataForm::Field &&) = default;
/// Destroys the form field.
QXmppDataForm::Field::~Field() = default;
/// Assigns \a other to this field.
QXmppDataForm::Field &QXmppDataForm::Field::operator=(const QXmppDataForm::Field &other) = default;
/// Default move-assignment operator.
QXmppDataForm::Field &QXmppDataForm::Field::operator=(QXmppDataForm::Field &&) = default;

///
/// Returns the field's description.
///
QString QXmppDataForm::Field::description() const
{
    return d->description;
}

///
/// Sets the field's description.
///
/// \param description
///
void QXmppDataForm::Field::setDescription(const QString &description)
{
    d->description = description;
}

///
/// Returns the field's key.
///
QString QXmppDataForm::Field::key() const
{
    return d->key;
}

///
/// Sets the field's key.
///
/// \param key
///
void QXmppDataForm::Field::setKey(const QString &key)
{
    d->key = key;
}

///
/// Returns the field's label.
///
QString QXmppDataForm::Field::label() const
{
    return d->label;
}

///
/// Sets the field's label.
///
/// \param label
///
void QXmppDataForm::Field::setLabel(const QString &label)
{
    d->label = label;
}

///
/// Returns the field's options.
///
QList<QPair<QString, QString>> QXmppDataForm::Field::options() const
{
    return transform<QList<QPair<QString, QString>>>(d->options, [](const auto &option) {
        return QPair<QString, QString> { option.label, option.value };
    });
}

///
/// Sets the field's options.
///
/// \param options
///
void QXmppDataForm::Field::setOptions(const QList<QPair<QString, QString>> &options)
{
    d->options = transform<QList<DataFormListOption>>(options, [](const auto &pair) {
        return DataFormListOption { pair.first, pair.second };
    });
}

///
/// Returns true if the field is required, false otherwise.
///
bool QXmppDataForm::Field::isRequired() const
{
    return d->required;
}

///
/// Set to true if the field is required, false otherwise.
///
/// \param required
///
void QXmppDataForm::Field::setRequired(bool required)
{
    d->required = required;
}

///
/// Returns the field's type.
///
QXmppDataForm::Field::Type QXmppDataForm::Field::type() const
{
    return d->type;
}

///
/// Sets the field's type.
///
/// \param type
///
void QXmppDataForm::Field::setType(QXmppDataForm::Field::Type type)
{
    d->type = type;
}

///
/// Returns the field's value.
///
QVariant QXmppDataForm::Field::value() const
{
    return d->value;
}

///
/// Sets the field's value.
///
/// \param value
///
void QXmppDataForm::Field::setValue(const QVariant &value)
{
    d->value = value;
}

///
/// Returns the size of the attached media according to \xep{0221, Data Forms Media Element}.
///
/// \since QXmpp 1.1
///
QSize QXmppDataForm::Field::mediaSize() const
{
    return d->mediaSize;
}

///
/// Sets the size of the attached media according to \xep{0221, Data Forms Media Element}.
///
/// \since QXmpp 1.1
///
void QXmppDataForm::Field::setMediaSize(const QSize &size)
{
    d->mediaSize = size;
}

///
/// Returns the sources for the attached media according to \xep{0221, Data Forms Media Element}.
///
/// \since QXmpp 1.1
///
QVector<QXmppDataForm::MediaSource> QXmppDataForm::Field::mediaSources() const
{
    return d->mediaSources;
}

///
/// Sets the sources to the attached media of the field according to \xep{0221, Data Forms Media Element}.
///
/// \since QXmpp 1.1
///
void QXmppDataForm::Field::setMediaSources(const QVector<QXmppDataForm::MediaSource> &mediaSources)
{
    d->mediaSources = mediaSources;
}

///
/// Returns true if the other field is identical to this one.
///
/// \since QXmpp 1.1
///
bool QXmppDataForm::Field::operator==(const QXmppDataForm::Field &other) const
{
    return d->description == other.description() &&
        d->key == other.key() &&
        d->label == other.label() &&
        d->options == other.d->options &&
        d->required == other.isRequired() &&
        d->type == other.type() &&
        d->value == other.value() &&
        d->mediaSources == other.mediaSources() &&
        d->mediaSize == other.mediaSize();
}

/// \cond
std::optional<QXmppDataForm::Field> QXmppDataForm::Field::fromDom(const QDomElement &el)
{
    QXmppDataForm::Field field;

    /* field type */
    field.setType(Enums::fromString<Type>(el.attribute(u"type"_s)).value_or(Field::TextSingleField));

    /* field attributes */
    field.setLabel(el.attribute(u"label"_s));
    field.setKey(el.attribute(u"var"_s));

    /* field value(s) */
    switch (field.type()) {
    case Field::BooleanField: {
        const auto valueStr = el.firstChildElement(u"value"_s).text();
        field.setValue(valueStr == u"1" || valueStr == u"true");
        break;
    }
    case Field::ListMultiField:
    case Field::JidMultiField:
    case Field::TextMultiField: {
        field.setValue(parseTextElements<QStringList>(el, u"value", ns_data));
        break;
    }
    default:
        field.setValue(el.firstChildElement(u"value"_s).text());
    }

    /* field media */
    if (const auto mediaElement = firstChildElement(el, u"media", ns_media_element);
        !mediaElement.isNull()) {
        field.setMediaSize(QSize(mediaElement.attribute(u"width"_s, u"-1"_s).toInt(),
                                 mediaElement.attribute(u"height"_s, u"-1"_s).toInt()));

        QMimeDatabase database;

        for (const auto &element : iterChildElements<MediaSource>(mediaElement)) {
            field.d->mediaSources << MediaSource(
                QUrl(element.text()),
                database.mimeTypeForName(element.attribute(u"type"_s)));
        }
    }

    /* field options */
    switch (field.type()) {
    case Field::ListMultiField:
    case Field::ListSingleField: {
        QList<QPair<QString, QString>> options;
        for (const auto &element : iterChildElements(el, u"option")) {
            options << qMakePair(element.attribute(u"label"_s),
                                 firstChildElement(element, u"value").text());
        }
        field.setOptions(options);
        break;
    }
    default:
        break;
    }

    /* other properties */
    field.setDescription(firstChildElement(el, u"description").text());
    field.setRequired(!firstChildElement(el, u"required").isNull());

    return field;
}

void QXmppDataForm::Field::toXml(QXmlStreamWriter *w) const
{
    XmlWriter writer(w);
    writer.write(Element {
        u"field",
        Attribute { u"type", d->type },
        OptionalAttribute { u"label", d->label },
        OptionalAttribute { u"var", d->key },
        /* field value(s) */
        [&] {
            switch (d->type) {
            case Field::BooleanField:
                writer.write(TextElement { u"value", d->value.toBool() });
                break;
            case Field::ListMultiField:
            case Field::JidMultiField:
            case Field::TextMultiField: {
                writer.write(TextElements { u"value", d->value.toStringList() });
                break;
            }
            default:
                writer.write(OptionalTextElement { u"value", d->value.toString() });
                break;
            }
        },
        /* field media */
        OptionalContent {
            !d->mediaSources.isEmpty(),
            Element {
                { u"media", ns_media_element },
                OptionalAttribute { u"width", d->mediaSize.width() > 0 ? std::make_optional(d->mediaSize.width()) : std::nullopt },
                OptionalAttribute { u"height", d->mediaSize.height() > 0 ? std::make_optional(d->mediaSize.height()) : std::nullopt },
                [&] {
                    for (const auto &source : d->mediaSources) {
                        writer.write(Element {
                            u"uri",
                            OptionalAttribute { u"type", source.contentType().name() },
                            Characters { source.uri() },
                        });
                    }
                },
            },
        },
        /* field options */
        OptionalContent {
            d->type == Field::ListMultiField || d->type == Field::ListSingleField,
            d->options,
        },
        OptionalTextElement { u"description", d->description },
        OptionalContent { d->required, Element { u"required" } },
    });
}

QXmppDataForm::Media QXmppDataForm::Field::media() const
{
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    Media media;
    QList<QPair<QString, QString>> pairUris;
    pairUris.reserve(d->mediaSources.size());

    for (const auto &source : std::as_const(d->mediaSources)) {
        pairUris << qMakePair<QString, QString>(
            source.contentType().name(),
            source.uri().toString());
    }

    media.setHeight(d->mediaSize.height());
    media.setWidth(d->mediaSize.width());
    media.setUris(pairUris);
    return media;
    QT_WARNING_POP
}

void QXmppDataForm::Field::setMedia(const QXmppDataForm::Media &media)
{
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    const QList<QPair<QString, QString>> &uris = media.uris();

    QVector<QXmppDataForm::MediaSource> sources;
    sources.reserve(uris.size());

    for (const auto &pairUri : uris) {
        sources << QXmppDataForm::MediaSource(
            QUrl(pairUri.second),
            QMimeDatabase().mimeTypeForName(pairUri.first));
    }

    d->mediaSources = sources;
    d->mediaSize = QSize(media.width(), media.height());
    QT_WARNING_POP
}

QVector<QXmppDataForm::MediaSource> &QXmppDataForm::Field::mediaSources()
{
    return d->mediaSources;
}

QSize &QXmppDataForm::Field::mediaSize()
{
    return d->mediaSize;
}
/// \endcond

class QXmppDataFormPrivate : public QSharedData
{
public:
    QString instructions;
    QList<QXmppDataForm::Field> fields;
    QString title;
    QXmppDataForm::Type type = QXmppDataForm::None;
};

///
/// \class QXmppDataForm
///
/// The QXmppDataForm class represents a data form as defined by \xep{0004,
/// Data Forms}.
///

///
/// Constructs a QXmppDataForm with the specified attributes.
///
/// \since QXmpp 1.3
///
QXmppDataForm::QXmppDataForm(Type type,
                             const QList<Field> &fields,
                             const QString &title,
                             const QString &instructions)
    : d(new QXmppDataFormPrivate)
{
    d->type = type;
    d->fields = fields;
    d->title = title;
    d->instructions = instructions;
}

///
/// Constructs a data form from any type based on QXmppDataFormBase.
///
/// \since QXmpp 1.5
///
QXmppDataForm::QXmppDataForm(const QXmppDataFormBase &based)
{
    *this = based.toDataForm();
}

/// Constructs a copy of \a other.
QXmppDataForm::QXmppDataForm(const QXmppDataForm &other) = default;
/// Default move constructor.
QXmppDataForm::QXmppDataForm(QXmppDataForm &&other) = default;
/// Destroys the form.
QXmppDataForm::~QXmppDataForm() = default;
/// Assigns \a other to this form.
QXmppDataForm &QXmppDataForm::operator=(const QXmppDataForm &other) = default;
/// Default move-assignment operator.
QXmppDataForm &QXmppDataForm::operator=(QXmppDataForm &&) = default;

/// Returns all fields.
QList<QXmppDataForm::Field> QXmppDataForm::fields() const
{
    return d->fields;
}

/// Returns all fields.
/// \since QXmpp 1.12
const QList<QXmppDataForm::Field> &QXmppDataForm::constFields() const
{
    return d->fields;
}

/// Sets the form's fields.
void QXmppDataForm::setFields(const QList<QXmppDataForm::Field> &fields)
{
    d->fields = fields;
}

/// Appends a field.
/// \since QXmpp 1.12
void QXmppDataForm::appendField(Field &&field)
{
    d->fields.append(std::move(field));
}

///
/// Look up field by name.
///
/// \since QXmpp 1.12
///
std::optional<QXmppDataForm::Field> QXmppDataForm::field(QStringView fieldName) const
{
    auto itr = std::ranges::find(d->fields, fieldName, &Field::key);
    if (itr != d->fields.constEnd()) {
        return *itr;
    }
    return {};
}

///
/// Look up field by name and return value if found.
///
/// \since QXmpp 1.12
///
std::optional<QVariant> QXmppDataForm::fieldValue(QStringView fieldName) const
{
    auto itr = std::ranges::find(d->fields, fieldName, &Field::key);
    if (itr != d->fields.constEnd()) {
        return itr->value();
    }
    return {};
}

///
/// Returns the form's instructions.
///
QString QXmppDataForm::instructions() const
{
    return d->instructions;
}

///
/// Sets the form's instructions.
///
/// \param instructions
///
void QXmppDataForm::setInstructions(const QString &instructions)
{
    d->instructions = instructions;
}

///
/// Returns the form's title.
///
QString QXmppDataForm::title() const
{
    return d->title;
}

///
/// Sets the form's title.
///
/// \param title
///
void QXmppDataForm::setTitle(const QString &title)
{
    d->title = title;
}

///
/// Returns the form's type.
///
QXmppDataForm::Type QXmppDataForm::type() const
{
    return d->type;
}

///
/// Sets the form's type.
///
/// \param type
///
void QXmppDataForm::setType(QXmppDataForm::Type type)
{
    d->type = type;
}

///
/// Searches for a hidden field called 'FORM_TYPE' and returns its value.
///
/// \returns The string value of the field or a null string if the field
/// couldn't be found.
///
/// \since QXmpp 1.5
///
QString QXmppDataForm::formType() const
{
    const auto formTypeItr = std::find_if(d->fields.begin(), d->fields.end(), [](const QXmppDataForm::Field &field) {
        return field.type() == QXmppDataForm::Field::HiddenField &&
            field.key() == u"FORM_TYPE";
    });

    if (formTypeItr != d->fields.end()) {
        return formTypeItr->value().toString();
    }
    return {};
}

///
/// Returns true if the form has an unknown type.
///
bool QXmppDataForm::isNull() const
{
    return d->type == QXmppDataForm::None;
}

/// \cond
void QXmppDataForm::parse(const QDomElement &element)
{
    if (element.isNull()) {
        return;
    }

    /* form type */
    const auto typeStr = element.attribute(u"type"_s);
    if (const auto type = Enums::fromString<Type>(typeStr)) {
        d->type = *type;
    } else {
        qWarning() << "Unknown form type" << typeStr;
        return;
    }

    /* form properties */
    d->title = firstChildElement(element, u"title").text();
    d->instructions = firstChildElement(element, u"instructions").text();
    // compat: allow parsing multiple forms into one QXmppDataForm
    d->fields.append(parseChildElements<QList<Field>>(element));
}

void QXmppDataForm::toXml(QXmlStreamWriter *writer) const
{
    if (isNull()) {
        return;
    }

    XmlWriter(writer).write(Element {
        XmlTag,
        Attribute { u"type", d->type },
        OptionalTextElement { u"title", d->title },
        OptionalTextElement { u"instructions", d->instructions },
        d->fields,
    });
}

QList<QXmppDataForm::Field> &QXmppDataForm::fields() { return d->fields; }
/// \endcond
