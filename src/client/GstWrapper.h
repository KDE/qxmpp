// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef GSTWRAPPER_H
#define GSTWRAPPER_H

#include <gst/gst.h>
#include <memory>

#include <QLatin1String>

namespace QXmpp::Private {

template<typename T, typename DeleterType, void(destruct)(DeleterType *)>
class CustomUniquePtr
{
    T *m_ptr = nullptr;

public:
    CustomUniquePtr(T *ptr = nullptr) : m_ptr(ptr) { }
    CustomUniquePtr(CustomUniquePtr &&other) : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
    CustomUniquePtr(const CustomUniquePtr &) = delete;
    ~CustomUniquePtr()
    {
        if (m_ptr) {
            destruct(m_ptr);
        }
    }
    CustomUniquePtr &operator=(const CustomUniquePtr &) = delete;
    CustomUniquePtr &operator=(CustomUniquePtr &&other)
    {
        reset(other.m_ptr);
        other.m_ptr = nullptr;
        return *this;
    };
    CustomUniquePtr &operator=(T *ptr)
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

using GstElementPtr = CustomUniquePtr<GstElement, void, gst_object_unref>;
using GstElementFactoryPtr = CustomUniquePtr<GstElementFactory, void, gst_object_unref>;
using GstPadPtr = CustomUniquePtr<GstPad, void, gst_object_unref>;
using GstSamplePtr = CustomUniquePtr<GstSample, GstSample, gst_sample_unref>;
using GstBufferPtr = CustomUniquePtr<GstBuffer, GstBuffer, gst_buffer_unref>;
using GCharPtr = CustomUniquePtr<gchar, void, g_free>;

bool checkGstFeature(QLatin1String feature);

}  // namespace QXmpp::Private

#endif
