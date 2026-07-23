<!--
SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>

SPDX-License-Identifier: CC0-1.0
-->

# Releasing QXmpp

This describes how to cut and publish a new QXmpp release. Releases are made
from a version branch (e.g. `1.16`), never from `master`. A patch release
(`X.Y.Z` → `X.Y.Z+1`) collects the fixes that landed on the version branch
since the previous release.

## 1. Prepare the release commits

Two commits are made, matching the pattern of earlier releases (see the git
history for `Release QXmpp 1.16.2`).

**Commit 1 — "Add changelog entries for X.Y.Z":** add a new section at the top
of [`CHANGELOG.md`](../CHANGELOG.md), above the previous release:

```
QXmpp X.Y.Z (Unreleased)
-------------------------

 - Area: Short description of the change (@author[, !MR])
```

List every change that landed since the previous release. Use one bullet per
change, starting with the affected area, and reference the author and (if
applicable) the merge request.

**Commit 2 — "Release QXmpp X.Y.Z":**

1. In `CHANGELOG.md`, change `(Unreleased)` to the release date,
   e.g. `(July 23, 2026)`, and adjust the underline length to match.
2. In [`CMakeLists.txt`](../CMakeLists.txt), bump `project(qxmpp VERSION ...)`.
   Only change `SO_VERSION` when the ABI changed (not for a normal patch
   release).
3. In [`docs/doap.xml`](doap.xml), add a `<release>` block as the first entry
   of the release list (directly after `</implements>`):

   ```xml
   <release>
     <Version>
       <revision>X.Y.Z</revision>
       <created>YYYY-MM-DD</created>
       <file-release rdf:resource='https://download.kde.org/unstable/qxmpp/qxmpp-X.Y.Z.tar.xz'/>
     </Version>
   </release>
   ```

## 2. Tag the release

The tag **must be an annotated and GPG-signed tag** — KDE's pre-receive hook
rejects lightweight/unannotated tags
(`Pushing an unannotated tag is not permitted`):

```
git tag -s vX.Y.Z <release-commit> -m "Release QXmpp X.Y.Z"
```

## 3. Push to KDE invent

```
git push upstream <version-branch>   # e.g. 1.16
git push upstream vX.Y.Z
```

`upstream` is `git@invent.kde.org:libraries/qxmpp.git`.

## 4. Build, sign, upload and request publication

Run the release helper with the version number:

```
utils/upload-release.sh X.Y.Z
```

This script ([`utils/upload-release.sh`](../utils/upload-release.sh)):

1. Clones the repository at tag `vX.Y.Z`.
2. Creates `qxmpp-X.Y.Z.tar.xz` and GPG-signs it (`.sig`). Signing reuses the
   passphrase cached by the GPG agent from the tag signing in step 2.
3. Uploads the tarball and signature to `ftp://upload.kde.org/incoming/`.
4. Opens a KDE sysadmin ticket form (via `xdg-open`) pre-filled with the
   SHA-1/SHA-256 checksums, to request publishing to
   `download.kde.org/unstable/qxmpp`. **You must submit this ticket** — it is
   the step that actually publishes the release.
5. If a sibling `../craft-blueprints-kde` checkout exists, updates the qxmpp
   Craft blueprint (version + digest) and opens a merge request for it.

Requires `jq`, `curl` and `gpg`. The signed tarball and `.sig` are left in the
current directory afterwards; they are release artifacts and can be removed once
the sysadmin ticket has been processed.

## 5. Merge the release branch into master

Finally, merge the version branch back into `master` so that the release
commits (changelog, version bump, `doap.xml` entry) are also present on the
development branch:

```
git checkout master
git merge <version-branch>   # e.g. 1.16
git push upstream master
```

## Checklist

- [ ] Changelog entries commit
- [ ] Release commit (changelog date, `CMakeLists.txt` version, `doap.xml`)
- [ ] Signed annotated tag `vX.Y.Z`
- [ ] Branch and tag pushed to `upstream`
- [ ] `utils/upload-release.sh X.Y.Z` run
- [ ] KDE sysadmin ticket submitted
- [ ] Craft blueprint merge request merged
- [ ] Version branch merged into `master`
