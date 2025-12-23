module.exports = {
  apps: [{
    name: 'chip-player-api',
    script: 'index.js',
    env_production: {
      NODE_ENV: 'production',
      BROWSE_LOCAL_FILESYSTEM: 'false',
      PUBLIC_CATALOG_URL: 'https://your-website.com/music',
      LOCAL_CATALOG_ROOT: '/path/to/your/music/archive'
    }
  }]
};
