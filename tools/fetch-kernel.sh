#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
version=$(<"$repo_dir/config/kernel.version")
if [[ ! $version =~ ^[0-9]+\.[0-9]+(\.[0-9]+)?$ ]]; then
    echo "Invalid pinned kernel version: $version" >&2
    exit 1
fi

major=${version%%.*}
base_url="https://cdn.kernel.org/pub/linux/kernel/v${major}.x"
archive="linux-${version}.tar.xz"
signature="linux-${version}.tar.sign"
download_dir="$repo_dir/build/downloads"
keyring="$repo_dir/build/kernel-keyring"
mkdir -p "$download_dir" "$keyring"
chmod 700 "$keyring"

if [[ ! -s "$download_dir/$archive" ]]; then
    curl --fail --location --retry 3 --output "$download_dir/$archive" "$base_url/$archive"
fi
curl --fail --location --retry 3 --output "$download_dir/$signature" "$base_url/$signature"

# Linux 7.2.8 is signed by Greg Kroah-Hartman. The fingerprint is published
# by kernel.org; pinning it makes the key server a transport, not a trust root.
signer_fingerprint=647F28654894E3BD457199BE38DBBDC86092693E
signer_key="$download_dir/gregkh-${signer_fingerprint}.asc"
curl --fail --location --retry 3 --output "$signer_key" \
    "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x$signer_fingerprint"
gpg --homedir "$keyring" --batch --import "$signer_key" || true
gpg --homedir "$keyring" --batch --list-keys "$signer_fingerprint" >/dev/null

verification_log="$download_dir/linux-${version}.verify.log"
if ! (set -o pipefail; xz -cd "$download_dir/$archive" | \
    gpg --homedir "$keyring" --batch --status-fd 1 --verify "$download_dir/$signature" - \
    >"$verification_log" 2>&1); then
    cat "$verification_log" >&2
    echo "Kernel signature verification failed" >&2
    exit 1
fi

if ! grep -Fq "[GNUPG:] VALIDSIG $signer_fingerprint " "$verification_log"; then
    cat "$verification_log" >&2
    echo "Kernel signature was not made by an approved kernel.org signer" >&2
    exit 1
fi

echo "Verified Linux $version source: $download_dir/$archive"
