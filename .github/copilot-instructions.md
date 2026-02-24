<!--
SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>

SPDX-License-Identifier: CC0-1.0
-->

# QXmpp Copilot Instructions

QXmpp is a C++/Qt XMPP client and server library (LGPL-2.1-or-later), targeting Qt 5.15+ and Qt 6.x, written in C++20.

## Build

```bash
mkdir build && cd build

# Minimal build
cmake .. -DBUILD_TESTING=ON

# Full build (docs, examples, GStreamer, OMEMO on Linux)
cmake .. -DBUILD_DOCUMENTATION=ON -DBUILD_EXAMPLES=ON -DWITH_GSTREAMER=ON -DBUILD_OMEMO=ON

# Force Qt6
cmake .. -DBUILD_WITH_QT6=ON

make -j$(nproc)
```

Optional features require external packages:
- **OMEMO** (`-DBUILD_OMEMO=ON`): needs `libomemo-c` and QCA
- **GStreamer Jingle calls** (`-DWITH_GSTREAMER=ON`): needs `gstreamer-1.0 >= 1.20`
- **Encrypted file sharing** (`-DWITH_QCA=ON`): needs `Qca-qt5/6`
- **Internal tests** (`-DBUILD_INTERNAL_TESTS=ON`): tests using unexported symbols

## Testing

```bash
# Run all tests
make test ARGS="--rerun-failed --output-on-failure"

# Run a single test executable from the build directory
./tests/qxmppmessage/tst_qxmppmessage

# Integration tests (requires a real XMPP server)
export QXMPP_TESTS_INTEGRATION_ENABLED=1
export QXMPP_TESTS_JID=user@server.example
export QXMPP_TESTS_PASSWORD=secret
make test
```

## Documentation

```bash
cmake .. -DBUILD_DOCUMENTATION=ON
make doc
# Output: doc/html/index.html
```

## Code Formatting

Uses clang-format (`.clang-format` at repo root). Install the pre-commit hook:
```bash
./utils/setup-hooks.sh  # requires git-clang-format
```

Include ordering (enforced by clang-format):
1. `"QXmpp*"` headers
2. Other local `"..."` headers
3. `<Q...>` Qt headers
4. Other `<...>` system headers

## Architecture

The library is split into three modules in `src/`:

- **`src/base/`** — Data types, stanza classes, and protocol primitives. No Qt networking. Key types: `QXmppStanza`, `QXmppIq`, `QXmppMessage`, `QXmppPresence`, all IQ/data types, and async infrastructure (`QXmppTask`, `QXmppPromise`).

- **`src/client/`** — XMPP client stack. `QXmppClient` is the central class; feature managers inherit from `QXmppClientExtension` and are added via `QXmppClient::addExtension()`. Each manager handles stanzas by overriding `handleStanza()` and advertising XEP features via `discoveryFeatures()`.

- **`src/server/`** — XMPP server components (`QXmppServer`, `QXmppServerExtension`, incoming/outgoing server streams).

- **`src/omemo/`** — Optional OMEMO E2EE module, compiled separately as `QXmppOmemoQt5/6` when `BUILD_OMEMO=ON`.

## Key Conventions

**Async pattern — `QXmppTask<T>` / `QXmppPromise<T>`:**  
All async manager APIs return `QXmppTask<T>`. Create a `QXmppPromise<T>`, keep it alive, and call `promise.finish(value)` when done. Use `task.then(context, handler)` to consume results. Helper `chain<Result>()` in `QXmppAsync_p.h` transforms one task into another.

**Result types:**  
Success/failure is expressed as `QXmpp::Result<T>` = `std::variant<T, QXmppError>`. `QXmppError` carries a human-readable `description` and a type-erased `std::any error` field (cast with `.value<T>()`). `QXmpp::Success` is a unit struct for void-success variants.

**Private implementation pattern:**  
Public headers expose `QXmpp_EXPORT` classes. Internal headers are named `*_p.h` and placed in `QXmpp::Private` namespace. The CMake build compiles an internal static object library (`QXmppQt5/6_internal`) with all symbols visible; tests link against it to access unexported symbols.

**Rule-of-six macros:**  
For pimpl types, use `QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(ClassName)` in the `.h` and `QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(ClassName)` in the `.cpp`.

**Deprecation:**  
Use `QXMPP_DEPRECATED_SINCE(major, minor)` guarded blocks (mirrors Qt's `QT_DEPRECATED_SINCE`). Removed API implementations go in `src/base/compat/removed_api.cpp` or `src/client/compat/removed_api.cpp`.

**String literals:**  
Include `StringLiterals.h` (private) to use `u"..."_s` (QString) and `"..."_L1` (QLatin1String) across all Qt versions.

**XML serialization:**  
Stanza types implement `parse(const QDomElement &)` for deserialization and `toXml(QXmpp::Private::XmlWriter &)` for serialization. `XmlWriter` wraps `QXmlStreamWriter` with helpers for typed attribute writing.

**Namespace constants:**  
XMPP XML namespaces are `constexpr QStringView` values in `QXmppConstants_p.h` under `QXmpp::Private`.

**REUSE compliance:**  
Every file must have an SPDX copyright header. Library source files use `LGPL-2.1-or-later`; build/config files use `CC0-1.0`. Binary/image files without inline headers get a companion `.license` file.

**CMake targets:**  
The main library target is `QXmppQt5` or `QXmppQt6` (variable `${QXMPP_TARGET}`). OMEMO is `QXmppOmemoQt5/6` (`${QXMPPOMEMO_TARGET}`). Tests link against `${QXMPP_TARGET}_internal` for access to private symbols.

## Test Utilities (`tests/util.h`)

- `parsePacket(obj, xmlBytes)` / `serializePacket(obj, xmlBytes)` — parse/serialize round-trip helpers
- `expectVariant<T>(variant)` — asserts a `std::variant` holds `T` and returns it
- `unwrap(variant<T, QXmppError>)` — fails the test on error, returns value
- `expectFutureVariant<T>(task)` — asserts task is finished and holds `T`
