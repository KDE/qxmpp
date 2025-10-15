// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "GstWrapper.h"

#include <gst/gst.h>
#include <sstream>
#include <stdexcept>

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

void linkPads(GstPad *srcPad, GstPad *sinkPad)
{
    auto result = gst_pad_link(srcPad, sinkPad);
    if (result == GST_PAD_LINK_NOFORMAT) {
        auto caps = GstCapsPtr(gst_pad_query_caps(srcPad, nullptr));
        g_print(" src pad caps: %s\n", gst_caps_to_string(caps));

        caps = gst_pad_query_caps(sinkPad, nullptr);
        g_print(" sink pad caps: %s\n", gst_caps_to_string(caps));
    }
    if (result != GST_PAD_LINK_OK) {
        std::ostringstream oss;
        oss << "gst pad link error (" << gst_pad_get_name(srcPad) << " -> " << gst_pad_get_name(sinkPad) << "): " << gst_pad_link_get_name(result);
        throw std::runtime_error(oss.str());
    }
}

}  // namespace QXmpp::Private
