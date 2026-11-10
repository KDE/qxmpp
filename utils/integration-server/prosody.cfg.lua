-- SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
--
-- SPDX-License-Identifier: CC0-1.0

-- Local server for the integration tests, see run.sh. Not for production use: no TLS for
-- clients, plain authentication is allowed and HTTPS uses a self-signed certificate.

admins = {}
modules_enabled = {
    "disco";
    "roster";
    "saslauth";
    "carbons";
    "pep";
    "private";
    "smacks";
    "blocklist";
    "bookmarks";
    "vcard4";
    "vcard_legacy";
    "mam";
    "ping";
    "time";
    "version";
}
c2s_require_encryption = false
s2s_require_encryption = false
allow_unencrypted_plain_auth = true
authentication = "internal_hashed"
storage = "internal"
log = { info = "*console" }
http_ports = {}
https_ports = { 5281 }
https_interfaces = { "127.0.0.1", "::1" }
https_certificate = "/etc/prosody/certs/localhost.crt"
https_key = "/etc/prosody/certs/localhost.key"
http_external_url = "https://localhost:5281/"
pidfile = "/var/run/prosody/prosody.pid"

VirtualHost "localhost"

Component "muc.localhost" "muc"
    modules_enabled = { "muc_mam" }

Component "upload.localhost" "http_file_share"
    -- upload.localhost does not resolve, serve it on the main host
    http_host = "localhost"
