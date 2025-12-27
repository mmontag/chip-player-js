#!/bin/sh
set -e # Exit immediately if a command fails.

REMOTE_HOST="your-server.com"
REMOTE_USER="username"
REMOTE_DEST_DIR="chip-player-service"
REMOTE_SSH_HOST="${REMOTE_USER}@${REMOTE_HOST}"

echo "🚀 Deploying server to ${REMOTE_HOST}..."

# 1. Use rsync to sync the current directory (.) to the remote destination.
rsync -avz --delete \
  --exclude '/node_modules' \
  --exclude '.env' \
  --exclude '._*' \
  --exclude '.DS_Store' \
  ./server "${REMOTE_SSH_HOST}:${REMOTE_DEST_DIR}/"

# 2. Copy index.html to the server directory as a template
scp ./build/index.html "${REMOTE_SSH_HOST}:${REMOTE_SERVER_DIR}/index.html"

# 3. SSH into the server to install dependencies and reload the app.
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
