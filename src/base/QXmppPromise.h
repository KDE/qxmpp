// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPPROMISE_H
#define QXMPPPROMISE_H

#include "QXmppTask.h"

///
/// \brief Create and update QXmppTask objects to communicate results of asynchronous operations.
///
/// Unlike QFuture, this is not thread-safe. This avoids the need to do mutex locking at every
/// access though.
///
/// \ingroup Core classes
///
/// \since QXmpp 1.5
///
template<typename T>
class QXmppPromise
{
    static_assert(!std::is_abstract_v<T>);

public:
    QXmppPromise() : d(std::make_shared<QXmpp::Private::TaskData<T>>()) { }

    ///
    /// Report that the asynchronous operation has finished, and call the connected handler of the
    /// QXmppTask<T> belonging to this promise.
    ///
    /// \param value The result of the asynchronous computation
    ///
#ifdef QXMPP_DOC
    void reportFinished(T &&value)
#else
    template<typename U, typename TT = T>
        requires(!std::is_void_v<T> && std::is_same_v<TT, U>)
    void finish(U &&value)
#endif
    {
        Q_ASSERT(!d->finished);
        d->finished = true;
        d->result = std::move(value);
        if (d->continuation) {
            d->continuation(*d);
            // clear continuation to avoid "deadlocks" in case the user captured this QXmppTask
            d->continuation = {};
        }
    }

    /// \cond
    template<typename U, typename TT = T>
        requires(!std::is_void_v<T> && std::is_constructible_v<TT, U> && !std::is_same_v<TT, U>)
    void finish(U &&value)
    {
        Q_ASSERT(!d->finished);
        d->finished = true;
        d->result = T { std::move(value) };
        if (d->continuation) {
            d->continuation(*d);
            // clear continuation to avoid "deadlocks" in case the user captured this QXmppTask
            d->continuation = {};
        }
    }

    template<typename U = T>
        requires(std::is_void_v<T>)
    void finish()
    {
        Q_ASSERT(!d->finished);
        d->finished = true;
        if (d->continuation) {
            d->continuation(*d);
            // clear continuation to avoid "deadlocks" in case the user captured this QXmppTask
            d->continuation = {};
        }
    }
    /// \endcond

    ///
    /// Obtain a handle to this promise that allows to obtain the value that will be produced
    /// asynchronously.
    ///
    QXmppTask<T> task() { return QXmppTask<T> { d }; }

private:
    std::shared_ptr<QXmpp::Private::TaskData<T>> d;
};

#endif  // QXMPPPROMISE_H
