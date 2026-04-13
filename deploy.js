#!/usr/bin/env node

/**
 * Chip Player JS Consolidated Deployment Script
 * Combines server, client, and catalog deployment logic.
 * Supports interactive selection and dry-run mode.
 */

const { execSync } = require('child_process');
const dotenvFlow = require('dotenv-flow');
const dotenvExpand = require('dotenv-expand');
const { checkbox } = require('@inquirer/prompts');
const chalk = require('chalk');

// 1. Load and Expand Environment Variables
const myEnv = dotenvFlow.config();
dotenvExpand.expand(myEnv);

const {
  REMOTE_HOST,
  REMOTE_SSH_HOST,
  REMOTE_SERVER_DIR,
  REMOTE_STATIC_DIR
} = process.env;

// Helper to handle dry-runs and command execution
const runCommand = (cmd, options = { stdio: 'inherit' }) => {
  if (process.env.DRY_RUN === 'true') {
    console.log(`${chalk.yellow('[DRY RUN]')} ${cmd}`);
    return;
  }
  return execSync(cmd, options);
};

if (!REMOTE_SSH_HOST || !REMOTE_SERVER_DIR) {
  console.error(chalk.red('Error: Missing required environment variables (REMOTE_SSH_HOST, REMOTE_SERVER_DIR).'));
  console.log(chalk.dim('Ensure they are defined in your .env or .env.local files.'));
  process.exit(1);
}

const purgeCloudflare = async () => {
  const zoneId = process.env.CLOUDFLARE_ZONE_ID;
  const apiToken = process.env.CLOUDFLARE_API_TOKEN;

  if (!zoneId || !apiToken) return;

  console.log(chalk.magenta('Triggering Cloudflare Purge...'));

  const res = await fetch(`https://api.cloudflare.com/client/v4/zones/${zoneId}/purge_cache`, {
    method: 'POST',
    headers: {
      'Authorization': `Bearer ${apiToken}`,
      'Content-Type': 'application/json'
    },
    // Purging only the HTML is safer and faster than "Purge Everything"
    body: JSON.stringify({ files: ['https://chiptune.app/'] })
  });

  if (res.ok) {
    console.log(chalk.green('Purge successful.'));
  } else {
    // Cloudflare returns errors in the JSON body
    const errorData = await res.json().catch(() => ({}));
    const detailedError = errorData.errors?.map(e => e.message).join(', ') || res.statusText;

    console.error(
      chalk.red(`Purge failed [${res.status}]:`),
      detailedError
    );
  }
};

// 2. Deployment Tasks
const tasks = {
  catalog: {
    name: 'Catalog (Database & Music Files)',
    fn: () => {
      console.log(chalk.blue(`Deploying catalog to ${REMOTE_HOST || REMOTE_SSH_HOST}...`));

      runCommand([
        'rsync',
        '-avz',
        './server/catalog.*',
        // './server/csdb.*',
        `"${REMOTE_SSH_HOST}:${REMOTE_SERVER_DIR}/"`
      ].join(' \\\n  '));

      const localMusicPath = '/Users/montag/Music/Chip Archive/';
      const remoteMusicPath = '/var/www/gifx.co/public_html/music/';

      console.log(chalk.blue('Syncing Music Archive...'));
      runCommand([
        'rsync',
        '-auzh',
        '--stats',
        "--exclude='.*'",
        '--perms',
        '--chmod=u=rwX,g=rX,o=rX',
        `"${localMusicPath}"`,
        `"${REMOTE_SSH_HOST}:${remoteMusicPath}"`
      ].join(' \\\n  '));
    }
  },
  server: {
    name: 'Server (Node.js/Express 5)',
    fn: () => {
      console.log(chalk.blue(`Deploying server to ${REMOTE_HOST || REMOTE_SSH_HOST}...`));

      const excludes = [
        'node_modules', '.env', '._*', '.git', '.DS_Store',
        '*.local', 'users.*', '*.db', '*.db-shm', '*.db-wal'
      ].map(e => `--exclude '${e}'`);

      runCommand([
        'rsync',
        '-avz',
        '--delete',
        ...excludes,
        './server/',
        `"${REMOTE_SSH_HOST}:${REMOTE_SERVER_DIR}/"`
      ].join(' \\\n  '));

      console.log(chalk.blue('Installing production dependencies on remote...'));
      const remoteCmd = `
        set -e
        export NVM_DIR="\\$HOME/.nvm"
        [ -s "\\$NVM_DIR/nvm.sh" ] && \\. "\\$NVM_DIR/nvm.sh"
        cd ${REMOTE_SERVER_DIR}
        npm install --omit=dev --no-fund
      `.trim();
      runCommand(`LC_ALL=C ssh ${REMOTE_SSH_HOST} "${remoteCmd}"`);
    }
  },
  client: {
    name: 'Client (Frontend Build)',
    fn: () => {
      console.log(chalk.blue(`Deploying client to ${REMOTE_HOST || REMOTE_SSH_HOST}...`));

      runCommand('npm run build-lite');

      console.log(chalk.blue('Syncing build directory...'));
      runCommand([
        'rsync',
        '-vz', // Don't preserve timestamps
        "--exclude '._*'",
        "--exclude '.DS_Store'",
        './build/',
        `"${REMOTE_SSH_HOST}:${REMOTE_STATIC_DIR}/"`
      ].join(' \\\n  '));

      console.log(chalk.blue('Pruning assets older than 30 days...'));
      const pruneCmd = `find ${REMOTE_STATIC_DIR} -type f -mtime +30 ! -delete`;
      runCommand(`ssh ${REMOTE_SSH_HOST} "${pruneCmd}"`);
    }
  }
};

const reloadServer = () => {
  console.log(chalk.magenta('Reloading PM2 process on server...'));
  const reloadCmd = `
    set -e
    export NVM_DIR="\\$HOME/.nvm"
    [ -s "\\$NVM_DIR/nvm.sh" ] && \\. "\\$NVM_DIR/nvm.sh"
    cd ${REMOTE_SERVER_DIR}
    pm2 reload ecosystem.config.js
  `.trim();
  runCommand(`LC_ALL=C ssh ${REMOTE_SSH_HOST} "${reloadCmd}"`);
  if (process.env.DRY_RUN !== 'true') {
    console.log(chalk.green('Server successfully reloaded.'));
  }
};

async function run() {
  const args = process.argv.slice(2);
  let selectedKeys = [];

  // Check for dry-run
  if (args.includes('--dry-run')) {
    process.env.DRY_RUN = 'true';
    console.log(chalk.yellow('Dry-run mode enabled. No commands will be executed.\n'));
  }

  // Parse CLI flags
  if (args.includes('--server')) selectedKeys.push('server');
  if (args.includes('--client')) selectedKeys.push('client');
  if (args.includes('--catalog')) selectedKeys.push('catalog');
  if (args.includes('--all')) selectedKeys = ['server', 'client', 'catalog'];

  // If no flags, show interactive menu
  if (selectedKeys.length === 0) {
    console.log('Chip Player JS Deployment Script');
    console.log(chalk.dim('--------------------------------'));

    try {
      selectedKeys = await checkbox({
        message: 'Select targets to deploy:',
        choices: Object.keys(tasks).map(key => ({
          name: tasks[key].name,
          value: key
        })),
        theme: {
          prefix: '',
          icon: {
            checked: chalk.green(' [x]'),
            unchecked: ' [ ]',
            cursor: chalk.blue('>')
          }
        }
      });
    } catch (error) {
      if (error.name === 'ExitPromptError') {
        console.log(chalk.yellow('\nDeployment cancelled.'));
        process.exit(0);
      }
      throw error;
    }
  }

  if (selectedKeys.length === 0) {
    console.log(chalk.yellow('No targets selected. Exiting.'));
    return;
  }

  try {
    for (const key of selectedKeys) {
      tasks[key].fn();
    }

    reloadServer();

    await purgeCloudflare();

    const status = process.env.DRY_RUN === 'true' ? 'simulation' : 'process';
    console.log(chalk.green(`\nDeployment ${status} finished.`));
  } catch (error) {
    console.error(chalk.red('\nDeployment failed:'));
    console.error(chalk.red(error.message));
    process.exit(1);
  }
}

run();
