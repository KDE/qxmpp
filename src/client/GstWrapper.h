// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef GSTWRAPPER_H
#define GSTWRAPPER_H

#include <gst/gst.h>
#include <memory>

#include <QLatin1String>

namespace QXmpp::Private {

template<typename T, void(destruct)(T *)>
class CustomUniquePtr
{
    T *m_ptr = nullptr;

public:
    CustomUniquePtr(T *ptr = nullptr) : m_ptr(ptr) { }
    CustomUniquePtr(const CustomUniquePtr &) = delete;
    ~CustomUniquePtr()
    {
        if (m_ptr) {
            destruct(m_ptr);
        }
    }
    CustomUniquePtr &operator=(const CustomUniquePtr &) = delete;
    CustomUniquePtr<T, destruct> &operator=(T *ptr)
    {
        reset(ptr);
        return *this;
    }
    operator T *() const { return m_ptr; }
    operator bool() const { return m_ptr != nullptr; }
    T *operator->() const { return m_ptr; }
    T *get() const { return m_ptr; }
    T **reassignRef()
    {
        reset();
        return &m_ptr;
    }
    void reset(T *ptr = nullptr)
    {
        if (m_ptr) {
            destruct(m_ptr);
        }
        m_ptr = ptr;
    }
};

template<typename T>
struct GstDeleter {
    void operator()(T *ptr) const
    {
        if (ptr) {
            gst_object_unref(ptr);
        }
    }
};

template<typename T>
class GstObjectPtr : public std::unique_ptr<T, GstDeleter<T>>
{
public:
    GstObjectPtr(T *ptr = nullptr)
        : std::unique_ptr<T, GstDeleter<T>>(ptr, GstDeleter<T> {}) { }

    T *get() const { return std::unique_ptr<T, GstDeleter<T>>::get(); }
    operator T *() const { return get(); }
    operator gpointer() const { return static_cast<gpointer>(get()); }
};

using GstElementPtr = GstObjectPtr<GstElement>;
using GstElementFactoryPtr = GstObjectPtr<GstElementFactory>;
using GstPadPtr = GstObjectPtr<GstPad>;
using GstSamplePtr = CustomUniquePtr<GstSample, [](GstSample *p) { gst_sample_unref(p); }>;
using GstBufferPtr = CustomUniquePtr<GstBuffer, [](GstBuffer *p) { gst_buffer_unref(p); }>;
using GCharPtr = CustomUniquePtr<gchar, [](gchar *p) { g_free(p); }>;

bool checkGstFeature(QLatin1String feature);

}  // namespace QXmpp::Private

#endif
