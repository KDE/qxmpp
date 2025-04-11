// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "GstWrapper.h"

#include <gst/gst.h>

namespace QXmpp::Private {

bool checkGstFeature(QLatin1String name)
{
    if (auto *gstFeature = gst_registry_lookup_feature(gst_registry_get(), name.latin1())) {
        gst_object_unref(gstFeature);
        return true;
    }
    return false;
}

GCharPtr getCharProperty(gpointer object, QLatin1String propertyName)
{
    GCharPtr value;
    g_object_get(object, propertyName.data(), value.reassignRef(), nullptr);
    return std::move(value);
}

int getIntProperty(gpointer object, QLatin1String propertyName, int defaultValue)
{
    int value = defaultValue;
    g_object_get(object, propertyName.data(), &value, nullptr);
    return value;
}

}  // namespace QXmpp::Private
