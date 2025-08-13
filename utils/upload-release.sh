#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025 Melvin Keskin <melvo@olomono.de>
# SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
#
# SPDX-License-Identifier: LGPL-2.1-or-later

set -e

version="$1"
project_name=qxmpp
release_tag_name="v${version}"
project_repo_url=https://invent.kde.org/libraries/qxmpp.git
work_dir=tmp-release-$version
checkout_dir="${project_name}-${version}"
compressed_archive=$project_name-$version.tar.xz
release_files_upload_url="ftp://upload.kde.org/incoming/"
sysadmin_ticket_url="https://go.kde.org/systickets"
target=unstable

if [ -z "$1" ]; then
    echo "Requires [version] as parameter."
    exit
fi

if ! command -v jq &> /dev/null; then
    echo "jq is required but not installed. Please install it and try again."
    exit 1
fi

mkdir -p $work_dir
cd $work_dir

# Download, compress and sign the release files.
echo "Cloning repository ${project_repo_url} at tag ${release_tag_name}..."
if ! git clone --depth=1 --quiet -b "${release_tag_name}" "${project_repo_url}" "${checkout_dir}" >/dev/null 2>&1; then
    echo "Failed to clone repository ${project_repo_url} at tag ${release_tag_name}."
    exit 1
fi
rm -rf "${checkout_dir}/.git"
tar -I "xz -9" -cf "${compressed_archive}" "${checkout_dir}"
gpg -o "${compressed_archive}.sig" -abs "${compressed_archive}"

# Upload the release files.
release_files="${compressed_archive} ${compressed_archive}.sig"
release_files_string=$(echo ${release_files} | tr "[:blank:]" ",")
curl -T "{${release_files_string}}" "${release_files_upload_url}"

task_title="Publish $compressed_archive"
task_description="Publish $compressed_archive to ${target}/${project_name}.

SHA-1:
\`\`\`
$(sha1sum ${release_files})
\`\`\`

SHA-256:
\`\`\`
$(sha256sum ${release_files})
\`\`\`"

echo "Now, create a sysadmin ticket to publish on download.kde.org:"
echo "https://phabricator.kde.org/maniphest/task/edit/form/2/?title=$(jq -sRr @uri <<< "$task_title")&priority=normal&description=$(jq -sRr @uri <<< "$task_description")"

cd - >/dev/null
mv $work_dir/$compressed_archive $work_dir/$compressed_archive.sig ./

# clean up
rm -rf ${work_dir}

