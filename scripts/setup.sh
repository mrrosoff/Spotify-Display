#!/usr/bin/env bash
# One-time setup: walk the user through Spotify OAuth, install the systemd
# unit, and write /etc/spotifydisplay.env with the refresh token.

set -euo pipefail

CLIENT_ID="${SPOTIFY_CLIENT_ID:-a9a84f65fc9f47568870f4c0c0185e3a}"
CLIENT_SECRET="${SPOTIFY_CLIENT_SECRET:-7cb7fe064e1844c19e87a2d475573948}"
REDIRECT_URI="http://127.0.0.1/callback"
SCOPE="user-read-currently-playing"
ENV_FILE="/etc/spotifydisplay.env"
UNIT_FILE="/etc/systemd/system/spotifydisplay.service"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

require() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "error: missing '$1' — install it and re-run." >&2
        exit 1
    }
}
require curl
require grep
require sudo

# URL-encoded form of REDIRECT_URI.
REDIRECT_ENC="http%3A%2F%2F127.0.0.1%2Fcallback"
AUTH_URL="https://accounts.spotify.com/authorize?client_id=${CLIENT_ID}&response_type=code&redirect_uri=${REDIRECT_ENC}&scope=${SCOPE}"

cat <<EOF

Spotify-Display setup
=====================

1. Open this URL in a browser on any device:

   ${AUTH_URL}

2. Sign in and click 'Agree'. Spotify will redirect to a page that fails to
   load (that's expected — the URL bar is what matters). The address will
   look like:

   http://127.0.0.1/callback?code=AQ...lots-of-characters...

EOF

read -r -p "Paste the full redirect URL here: " CALLBACK
CODE="$(echo "$CALLBACK" | grep -oE 'code=[^&]+' | head -n1 | cut -d= -f2 || true)"
if [[ -z "$CODE" ]]; then
    echo "error: could not find a 'code=' parameter in that URL." >&2
    exit 1
fi

echo "Exchanging code for refresh token..."
RESPONSE="$(
    curl --silent --show-error --fail-with-body \
        -X POST https://accounts.spotify.com/api/token \
        -u "${CLIENT_ID}:${CLIENT_SECRET}" \
        -d grant_type=authorization_code \
        -d "code=${CODE}" \
        -d "redirect_uri=${REDIRECT_URI}"
)" || {
    echo "error: token exchange failed:" >&2
    echo "$RESPONSE" >&2
    exit 1
}

REFRESH_TOKEN="$(echo "$RESPONSE" | grep -oE '"refresh_token"[[:space:]]*:[[:space:]]*"[^"]+"' | sed -E 's/.*"([^"]+)"$/\1/')"
if [[ -z "$REFRESH_TOKEN" ]]; then
    echo "error: no refresh_token in Spotify response:" >&2
    echo "$RESPONSE" >&2
    exit 1
fi

echo "Writing ${ENV_FILE}..."
sudo install -m 0600 /dev/stdin "$ENV_FILE" <<EOF
SPOTIFY_REFRESH_TOKEN=${REFRESH_TOKEN}
EOF

if [[ ! -f "$UNIT_FILE" ]]; then
    echo "Installing ${UNIT_FILE}..."
    sudo install -m 0644 "${REPO_ROOT}/startup/spotifydisplay.service" "$UNIT_FILE"
    sudo systemctl daemon-reload
fi

cat <<EOF

Setup complete.

Build and start:

    make
    sudo systemctl enable --now spotifydisplay
    sudo journalctl -u spotifydisplay -f
EOF
