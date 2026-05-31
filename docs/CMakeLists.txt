# SPDX-FileCopyrightText: 2017 Niels Ole Salscheider <niels_ole@salscheider-online.de>
# SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
#
# SPDX-License-Identifier: CC0-1.0

# QDoc-based documentation generation. Replaces previous Doxygen setup so
# that api.kde.org (KApiDox) can resolve cross-references into Qt's docs.

if(NOT ECM_FOUND)
    message(FATAL_ERROR
        "BUILD_DOCUMENTATION/BUILD_DOCBOOK requires extra-cmake-modules >= 6.11 "
        "(provides ECMGenerateQDoc).")
endif()

include(KDEInstallDirs)
include(ECMQueryQt)

# ECMGenerateQDoc hard-codes the output to ${CMAKE_BINARY_DIR}/.doc unless
# DOC_DESTDIR is set. Default to a non-hidden ${CMAKE_BINARY_DIR}/doc/qdoc
# so file managers / shells without dotfile listing find the output.
if(NOT DOC_DESTDIR)
    set(DOC_DESTDIR "${CMAKE_BINARY_DIR}/doc/qdoc")
endif()

# qxmpp.qdocconf includes $KDE_DOCS/global/qt-module-defaults.qdocconf for
# the shared KDE/Qt documentation infrastructure. On api.kde.org KDE_DOCS
# points at kde-qdoc-common. For local builds, fall back to Qt's installed
# doc tree (same directory layout). We inject the env var via a wrapper
# script around qdoc because ECMGenerateQDoc builds the qdoc invocation
# itself and doesn't accept extra environment variables.
if(DEFINED ENV{KDE_DOCS})
    set(_KDE_DOCS "$ENV{KDE_DOCS}")
else()
    ecm_query_qt(_KDE_DOCS QT_INSTALL_DOCS)
endif()

find_package(Qt6 REQUIRED COMPONENTS Tools)
get_target_property(_qdoc_actual Qt6::qdoc LOCATION)
set(_qdoc_wrapper "${CMAKE_CURRENT_BINARY_DIR}/qdoc-wrapper.sh")
file(WRITE ${_qdoc_wrapper}
    "#!/bin/sh\nexec env KDE_DOCS='${_KDE_DOCS}' '${_qdoc_actual}' \"\$@\"\n")
file(CHMOD ${_qdoc_wrapper} PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(QDOC_BIN ${_qdoc_wrapper})

include(ECMGenerateQDoc)

# ecm_generate_qdoc resolves the qdocconf path relative to the target's
# SOURCE_DIR. The QXmppQt6 target lives in ${PROJECT_SOURCE_DIR}/src, so we
# pass the qdocconf via a path that is valid both from
# CMAKE_CURRENT_SOURCE_DIR (= docs/) and from src/ via ".." normalization.
ecm_generate_qdoc(${QXMPP_TARGET} ../docs/qxmpp.qdocconf)

# DOAP rendering files used by xep.qdoc (iframe in the XEP support page).
# Copy them next to the generated HTML so the iframe can resolve them in
# both the local build and the installed location.
set(DOAP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/doap.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/doap-rendering/doap.xsl
    ${CMAKE_CURRENT_SOURCE_DIR}/doap-rendering/xeplist.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/doap-rendering/doap.css
)

set(QDOC_HTML_DIR ${DOC_DESTDIR}/qxmpp)
add_custom_command(
    TARGET generate_docs_${QXMPP_TARGET}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${QDOC_HTML_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${DOAP_FILES} ${QDOC_HTML_DIR}/
    COMMENT "Copying DOAP rendering files into QDoc HTML output"
    VERBATIM
)

if(BUILD_DOCUMENTATION)
    install(FILES ${DOAP_FILES} DESTINATION ${CMAKE_INSTALL_DOCDIR}/qxmpp)
endif()
