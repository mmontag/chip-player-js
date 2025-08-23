import { useEffect, useContext } from "react";
import { UserContext } from "./UserProvider";

const ThemeHandler = () => {
  const { settings } = useContext(UserContext);

  useEffect(() => {
    if (settings?.theme) {
      applyTheme(settings.theme);
    }
  }, [settings?.theme]); // Reacts to settings changes

  return null; // No UI, just handles side effects
};

// Apply the theme to the DOM
function applyTheme(theme) {
  document.documentElement.setAttribute("data-theme", theme);
  setThemeColor(theme === "winamp" ? "#1e1e2f" : "#000088");
}

// Set theme color in meta tag
function setThemeColor(color) {
  let metaThemeColor = document.querySelector("meta[name='theme-color']");
  if (!metaThemeColor) {
    metaThemeColor = document.createElement("meta");
    metaThemeColor.name = "theme-color";
    document.head.appendChild(metaThemeColor);
  }
  metaThemeColor.content = color;
}

export default ThemeHandler;
