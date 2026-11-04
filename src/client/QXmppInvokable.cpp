// SPDX-FileCopyrightText: 2009 Ian Reinhart Geiser <geiseri@kde.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppInvokable.h"

#include <QMetaMethod>
#include <QStringList>
#include <QVariant>
#include <qdebug.h>

/*!
    \class QXmppInvokable
    \inmodule QXmpp

    \brief The QXmppInvokable class is the base class for all objects that can be invoked via
    RPC.

    All public slots of objects derived from this class are exposed to the RPC interface of the
    QXmppRpcManager. The methods can only use argument and return types that QVariant supports.

    \note This class is only used by QXmppRpcManager, which is deprecated.

    \sa QXmppRpcManager
*/

/*! Constructs a QXmppInvokable with the specified \a parent. */
QXmppInvokable::QXmppInvokable(QObject *parent)
    : QObject(parent)
{
}

/*!
    \fn bool QXmppInvokable::isAuthorized(const QString &jid) const

    Returns whether the entity with the JID \a jid is allowed to execute methods of this object.

    Reimplement this method to restrict access to the object.
*/

/*! Destroys the object. */
QXmppInvokable::~QXmppInvokable() = default;

/*!
    Executes the \a method on this object with the arguments \a args and returns its result.

    This method is reentrant and the method is invoked in a thread safe manner. However, the side
    effects of the invoked method may not be thread safe.

    Returns an invalid QVariant if the method does not exist or the types of the arguments do not
    match.
*/
QVariant QXmppInvokable::dispatch(const QByteArray &method, const QList<QVariant> &args)
{
    buildMethodHash();

    if (!m_methodHash.contains(method)) {
        return QVariant();
    }

    int idx = m_methodHash[method];
    if (paramTypes(args) != metaObject()->method(idx).parameterTypes()) {
        return QVariant();
    }

    const char *typeName = metaObject()->method(idx).typeName();
    QMetaType resultType = metaObject()->method(idx).returnMetaType();

    void *result = resultType.create(nullptr);

    QGenericReturnArgument ret(typeName, result);
    QList<QGenericArgument> genericArgs;
    QList<QVariant>::ConstIterator iter = args.begin();
    while (iter != args.end()) {
        const void *data = iter->data();
        const char *name = iter->typeName();
        genericArgs << QGenericArgument(name, data);
        ++iter;
    }

    if (QMetaObject::invokeMethod(this, method.constData(), ret,
                                  genericArgs.value(0, QGenericArgument()),
                                  genericArgs.value(1, QGenericArgument()),
                                  genericArgs.value(2, QGenericArgument()),
                                  genericArgs.value(3, QGenericArgument()),
                                  genericArgs.value(4, QGenericArgument()),
                                  genericArgs.value(5, QGenericArgument()),
                                  genericArgs.value(6, QGenericArgument()),
                                  genericArgs.value(7, QGenericArgument()),
                                  genericArgs.value(8, QGenericArgument()),
                                  genericArgs.value(9, QGenericArgument()))) {
        QVariant returnValue(resultType, result);
        resultType.destroy(result);
        return returnValue;
    } else {
        qDebug("No such method '%s'", method.constData());
        return QVariant();
    }
}

/*!
    Returns the type names of the values in \a params, e.g. for type checking.
*/
QList<QByteArray> QXmppInvokable::paramTypes(const QList<QVariant> &params)
{
    QList<QByteArray> types;
    for (const auto &variant : std::as_const(params)) {
        types << variant.typeName();
    }
    return types;
}

void QXmppInvokable::buildMethodHash()
{
    QWriteLocker locker(&m_lock);
    if (m_methodHash.size() > 0) {
        return;
    }

    int methodCount = metaObject()->methodCount();
    for (int idx = 0; idx < methodCount; ++idx) {
        QByteArray signature = metaObject()->method(idx).methodSignature();
        m_methodHash[signature.left(signature.indexOf('('))] = idx;
        //         qDebug() << metaObject()->method(idx).parameterTypes();
    }
}

/*!
    Returns the names of all slots of this object for introspection of the provided interface.
*/
QStringList QXmppInvokable::interfaces() const
{
    QStringList results;
    int methodCount = metaObject()->methodCount();
    for (int idx = 0; idx < methodCount; ++idx) {
        if (metaObject()->method(idx).methodType() == QMetaMethod::Slot) {
            QByteArray signature = metaObject()->method(idx).methodSignature();
            results << QString::fromUtf8(signature.left(signature.indexOf(u'(')));
        }
    }
    return results;
}
