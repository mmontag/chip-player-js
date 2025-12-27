#!/bin/sh
set -e # Exit immediately if a command fails.

REMOTE_HOST="your-server.com"
REMOTE_USER="username"
REMOTE_SERVER_DIR="chip-player-service"
REMOTE_STATIC_DIR="/var/www/your-server/public_html"
REMOTE_SSH_HOST="${REMOTE_USER}@${REMOTE_HOST}"

echo "🚀 Deploying client to ${REMOTE_HOST}..."

# 1. Build the client
npm run build-lite

# 2. Use rsync to sync the build directory to the remote destination.
rsync -avz --delete --exclude '._*' --exclude '.DS_Store' ./build/ "${REMOTE_SSH_HOST}:${REMOTE_STATIC_DIR}/"

# 3. Copy index.html to the server directory as a template
scp ./build/index.html "${REMOTE_SSH_HOST}:${REMOTE_SERVER_DIR}/index.html"

echo "✅ Client deployment script finished."
