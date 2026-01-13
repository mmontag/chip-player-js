#!/bin/sh
set -e # Exit immediately if a command fails.

# Load environment variables: .env (defaults) followed by .env.local (overrides)
[ -f .env ]       && set -a && source .env       && set +a
[ -f .env.local ] && set -a && source .env.local && set +a

echo "🚀 Deploying client to ${REMOTE_HOST}..."

# 1. Build the client
npm run build-lite

# 2. Use rsync to sync the build directory to the remote destination.
rsync -avz --delete --exclude '._*' --exclude '.DS_Store' ./build/ "${REMOTE_SSH_HOST}:${REMOTE_STATIC_DIR}/"

# 3. Restart the server to pick up changes
echo "🔄 Restarting server..."
LC_ALL=C ssh "${REMOTE_SSH_HOST}" "
  set -e
  # Load NVM
  export NVM_DIR=\"\$HOME/.nvm\"
  [ -s \"\$NVM_DIR/nvm.sh\" ] && \\. \"\$NVM_DIR/nvm.sh\"
  cd ${REMOTE_SERVER_DIR}
  echo 'Reloading pm2 process...'
  pm2 reload ecosystem.config.js
"

echo "✅ Client deployment script finished."
