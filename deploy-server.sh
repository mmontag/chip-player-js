#!/bin/sh
set -e # Exit immediately if a command fails.

# Load environment variables: .env (defaults) followed by .env.local (overrides)
[ -f .env ]       && set -a && source .env       && set +a
[ -f .env.local ] && set -a && source .env.local && set +a

echo "🚀 Deploying server to ${REMOTE_HOST}..."

# 1. Use rsync to sync the current directory (.) to the remote destination.
rsync -avz --delete \
  --exclude '/node_modules' \
  --exclude '.env' \
  --exclude '._*' \
  --exclude '.DS_Store' \
  --exclude '*.local' \
  --exclude 'users.*' \
  ./server/ "${REMOTE_SSH_HOST}:${REMOTE_SERVER_DIR}/"

# 2. SSH into the server to install dependencies and reload the app.
LC_ALL=C ssh "${REMOTE_SSH_HOST}" "
  set -e
  # Load NVM
  export NVM_DIR=\"\$HOME/.nvm\"
  [ -s \"\$NVM_DIR/nvm.sh\" ] && \\. \"\$NVM_DIR/nvm.sh\"
  echo 'Connected to server, running post-deploy commands...'
  cd ${REMOTE_SERVER_DIR}
  echo 'Installing/updating production dependencies...'
  npm install --omit=dev --no-fund
  echo 'Reloading pm2 process...'
  pm2 reload ecosystem.config.js
  echo '✅ Deployment finished on server.'
"

echo "✅ Server deployment script finished."
