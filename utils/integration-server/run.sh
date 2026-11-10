#!/bin/bash
# SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
#
# SPDX-License-Identifier: CC0-1.0

# Starts a local Prosody server for the integration tests in a container and prints the
# environment variables for running them, e.g.:
#
#   eval "$(utils/integration-server/run.sh)"
#   ctest --test-dir build -R Omemo
#
# Stop it with: podman stop qxmpp-integration-server
#
# Set PODMAN="sudo podman" if rootless podman does not work on your system.

set -euo pipefail

PODMAN=${PODMAN:-podman}
IMAGE=${IMAGE:-docker.io/prosodyim/prosody:13.0}
NAME=qxmpp-integration-server
PASSWORD=integration-test
DIR=$(cd "$(dirname "$0")" && pwd)

# self-signed certificate for HTTP File Upload
CERTS=$(mktemp -d)
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost" \
    -keyout "$CERTS/localhost.key" -out "$CERTS/localhost.crt" 2>/dev/null
chmod 755 "$CERTS"
chmod 644 "$CERTS/localhost.key"

$PODMAN rm -f "$NAME" >/dev/null 2>&1 || true
$PODMAN run -d --rm --name "$NAME" --network host \
    -v "$DIR/prosody.cfg.lua:/etc/prosody/prosody.cfg.lua:ro,Z" \
    -v "$CERTS:/etc/prosody/certs:ro,Z" \
    "$IMAGE" >/dev/null

# wait until the server accepts connections
for _ in $(seq 50); do
    if $PODMAN exec "$NAME" prosodyctl status >/dev/null 2>&1; then
        break
    fi
    sleep 0.2
done

for user in alice bob; do
    $PODMAN exec "$NAME" prosodyctl register "$user" localhost "$PASSWORD" >&2
done

echo "export QXMPP_TESTS_INTEGRATION_ENABLED=1"
echo "export QXMPP_JID=alice@localhost"
echo "export QXMPP_PASSWORD=$PASSWORD"
echo "export QXMPP_JID_2=bob@localhost"
echo "export QXMPP_PASSWORD_2=$PASSWORD"
echo "export QXMPP_TESTS_MUC_SERVICE=muc.localhost"
# the certificate of HTTP File Upload is self-signed
echo "export QXMPP_TESTS_IGNORE_SSL_ERRORS=1"
