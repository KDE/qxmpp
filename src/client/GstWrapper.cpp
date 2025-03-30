// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "GstWrapper.h"

#include <gst/gst.h>

bool QXmpp::Private::checkGstFeature(QLatin1String name)
{
    if (auto *gstFeature = gst_registry_lookup_feature(gst_registry_get(), name.latin1())) {
        gst_object_unref(gstFeature);
        return true;
    }
    return false;
}
