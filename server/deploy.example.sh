#!/bin/sh
set -e # Exit immediately if a command fails.

# --- Configuration ---
REMOTE_HOST="your-server.com"
REMOTE_USER="username"
# The destination directory on your server where the server code lives
REMOTE_DEST_DIR="chip-player-service"
# --- End Configuration ---

REMOTE_SSH_HOST="${REMOTE_USER}@${REMOTE_HOST}"

echo "🚀 Deploying server to ${REMOTE_HOST}..."

# 1. Use rsync to sync the current directory (.) to the remote destination.
# This is much more efficient than scp as it only transfers changed files.
# We exclude node_modules and the local .env file.
rsync -avz --delete \
  --exclude '/node_modules' \
  --exclude '.env' \
  ./ "${REMOTE_SSH_HOST}:${REMOTE_DEST_DIR}/"

# 2. SSH into the server to install dependencies and reload the app.
# This command block is executed remotely on your server.
ssh "${REMOTE_SSH_HOST}" "
  set -e
  # Load NVM
  export NVM_DIR=\"\$HOME/.nvm\"
  [ -s \"\$NVM_DIR/nvm.sh\" ] && \\. \"\$NVM_DIR/nvm.sh\"
  echo 'Connected to server, running post-deploy commands...'
  cd ${REMOTE_DEST_DIR}
  echo 'Installing/updating production dependencies...'
  npm install --omit=dev --no-fund
  echo 'Reloading pm2 process...'
  pm2 reload ecosystem.config.js --env production
  echo '✅ Deployment finished on server.'
"

echo "✅ Local deployment script finished."
