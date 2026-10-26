// Light/dark switch. The reader's last choice wins; without one the operating
// system preference decides. The class lands on <html> so both the theme and the
// dark syntax stylesheet can key off it.
(function () {
  "use strict";

  var key = "fincore-docs-theme";

  function stored() {
    try {
      return window.localStorage.getItem(key);
    } catch (error) {
      return null;
    }
  }

  function preferred() {
    if (window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches) {
      return "dark";
    }
    return "light";
  }

  function apply(theme) {
    var dark = theme === "dark";
    document.documentElement.classList.toggle("fc-dark", dark);
    var button = document.querySelector(".fc-theme-toggle");
    if (button) {
      button.setAttribute("aria-pressed", dark ? "true" : "false");
      var label = document.documentElement.lang &&
                  document.documentElement.lang.toLowerCase().indexOf("zh") === 0
        ? (dark ? "切换到浅色" : "切换到深色")
        : (dark ? "Switch to light theme" : "Switch to dark theme");
      button.setAttribute("aria-label", label);
      button.setAttribute("title", label);
      button.textContent = dark ? "☀" : "☾";
    }
  }

  function current() {
    return document.documentElement.classList.contains("fc-dark") ? "dark" : "light";
  }

  function start() {
    apply(stored() || preferred());
    var button = document.querySelector(".fc-theme-toggle");
    if (!button) {
      return;
    }
    button.addEventListener("click", function () {
      var next = current() === "dark" ? "light" : "dark";
      apply(next);
      try {
        window.localStorage.setItem(key, next);
      } catch (error) {
        /* storage can be blocked; the switch still works for this page */
      }
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", start);
  } else {
    start();
  }
})();
