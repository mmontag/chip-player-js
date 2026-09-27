import ReactGA from 'react-ga4';

const GA_ID = process.env.REACT_APP_GOOGLE_ANALYTICS_ID;

function isBot() {
  if (typeof navigator === 'undefined') return true;
  if (navigator.webdriver) return true;
  const botRegex = /bot|crawler|spider|crawling|slurp|duckduckbot|baiduspider|yandex|bingbot|googlebot/i;
  return botRegex.test(navigator.userAgent || '');
}

export function initGA() {
  if (!GA_ID) {
    return;
  }

  if (isBot()) {
    return;
  }

  ReactGA.initialize(GA_ID, {
    testMode: process.env.NODE_ENV !== 'production',
  });
}

export default ReactGA;
