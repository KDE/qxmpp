<!--
SPDX-FileCopyrightText: 2023 Melvin Keskin <melvo@olomono.de>

SPDX-License-Identifier: CC0-1.0
-->

# Contributing

This is a guideline for contributing to QXmpp.

## Git Hooks Setup

QXmpp uses [ECM's KDEGitCommitHooks](https://api.kde.org/ecm/kde-module/KDEGitCommitHooks.html) to set up a pre-commit hook that checks code formatting via `git clang-format`.
The hook is automatically installed when running CMake, as long as `clang-format` and `extra-cmake-modules` are installed.

## Merge-requests (MRs)

You can submit suggestions for changes via merge-requests on [KDE invent][invent].
The maintainers of QXmpp will review your code. However, everybody is welcome to review the code and give feedback.
You can find the current maintainers and other people involved in the project in the `AUTHORS` file.

### Guidelines for Creating MRs

1. Do not use the *master* branch of your personal fork to create MRs. It leads to confusion and
   makes it harder to check out your branch, making changes to it and in the end also merging your
   MR into the *master* branch of the main repository.
1. For fixes, target the branch of the latest release (e.g. *1.3*) instead of the *master* branch.
1. Write short commit messages starting with an upper case letter and the imperative.
1. Split your commits logically.
1. Do not mix unrelated changes in the same MR.
1. Check the tasks in the pull-request template when they are done or no changes were needed.

## Testing

After building QXmpp with `-DBUILD_TESTING=ON`, you can run QXmpp's unit tests in the build directory with:
```
make test
```

### Internal Tests

There are some additional internal tests that only work with debug builds because they are using unexported symbols from the QXmpp library.
You can enable them using `-DBUILD_INTERNAL_TESTS=ON`.
They are also run using `make test`, along with the regular tests.

### Integration Tests

There are some tests that also provide integration testing with a real XMPP server.
The code of those test cases is always built, but the cases are skipped when running `make test` without further configuration.

You can enable them by exporting the following environment variables:

* `QXMPP_TESTS_INTEGRATION_ENABLED=1`
* `QXMPP_JID=<jid>`
* `QXMPP_PASSWORD=<password>`

Replace `<jid>` and `<password>` with the credentials of the account for connecting to the server.

## Naming of New APIs

New public APIs use the `QXmpp` namespace instead of the `QXmpp` class prefix.
The existing `QXmpp*` classes are renamed in QXmpp 2.0.

1. Entry points are declared directly in `QXmpp`: the client, the managers, the storage
   interfaces and their implementations, the top-level stanzas and types shared by several
   features (e.g. `QXmpp::OmemoManager`, `QXmpp::OmemoStorage`, `QXmpp::OmemoMemoryStorage`,
   `QXmpp::TrustLevel`).
   A manager is always named `QXmpp::<Feature>Manager`, even if a namespace `QXmpp::<Feature>`
   exists.
1. The types of a single feature are declared in `QXmpp::<Feature>`, e.g. the types a manager
   returns, takes or emits (`QXmpp::Omemo::Device`, `QXmpp::Omemo::DeviceWatch`,
   `QXmpp::Muc::Role`).
   A feature without such types does not get a namespace.
1. Public namespaces are at most two levels deep (`QXmpp::<Feature>`).
   The name of a namespace must not be the name of a type in `QXmpp`.
1. A feature has the same name everywhere: in the namespace, in the manager and in the other
   entry points (`QXmpp::Omemo`, `QXmpp::OmemoManager`, `QXmpp::OmemoStorage`).
1. Internal code is declared in `QXmpp::Private` or `QXmpp::<Feature>::Private`; these rules do
   not apply to it.

Signals, slots and properties (`Q_PROPERTY`) need fully qualified types for moc and QML.
Keeping the entry points in `QXmpp` keeps those types short.

## Documentation

The API documentation is generated with QDoc (the Qt documentation generator)
via [extra-cmake-modules ≥ 6.11][ecm]. Enable it with `-DBUILD_DOCUMENTATION=ON`
and build it explicitly:

```
cmake --build build --target generate_docs_QXmppQt6
```

The output ends up in `build/doc/qdoc/qxmpp/` (override with
`-DDOC_DESTDIR=...`); open `index.html` to read the docs.

QDoc is used (instead of Doxygen as in earlier QXmpp releases) so that
[api.kde.org][apikde] can resolve cross-references into the Qt
documentation.

Use `/*! ... */` blocks (not `///`) for new documentation, refer to
parameters with `\a paramname` inline in prose (QDoc has no `\param`
command), and keep custom XEP links via `\xep{NUM}` or `\xep{NUM} (Title)`.

To correctly view the list of supported XEPs, a local webserver is needed, see the [DOAP rendering documentation][doap-rendering] for details.

[ecm]: https://api.kde.org/ecm/
[apikde]: https://api.kde.org/

## Releasing

Maintainers: see [the release documentation][releasing] for how to cut and
publish a new QXmpp release.

## Copyright

QXmpp is [REUSE-compatible][reuse].
Thus, you need to add an appropriate copyright header to each new file.
If do significant changes to a file (e.g. no search and replace, typo fixes or just adding of pure
non-copyrightable data like XML namespaces), you may add yourself as a copyright holder in the
copyright header. This is recommended for large changes.

If the file does not allow such a header, add a `.license` file for the copyright information.

[doap-rendering]: /doc/doap-rendering/README.md
[releasing]: /docs/RELEASING.md
[invent]: https://invent.kde.org/libraries/qxmpp
[reuse]: https://api.reuse.software/info/github.com/qxmpp-project/qxmpp
