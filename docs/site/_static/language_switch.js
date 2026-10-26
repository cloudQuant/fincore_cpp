// Rewrite the language links so switching keeps the current page and version.
// Without JavaScript the links still land on the other language's home page.
(function () {
  "use strict";

  // Read the Docs may serve the Chinese build under zh_CN or zh.
  var languages = ["en", "zh-cn", "zh_CN", "zh"];

  function currentPath() {
    return document.location.pathname;
  }

  function targetFor(language) {
    var path = currentPath();
    var parts = path.split("/");
    for (var i = 0; i < parts.length; i += 1) {
      if (languages.indexOf(parts[i]) !== -1) {
        parts[i] = language;
        // Drop any page-specific suffix so the trailing segment stays valid
        // even when the other language has not built this page yet.
        return parts.slice(0, i + 3).join("/") + "/";
      }
    }
    return null;
  }

  function apply() {
    var links = document.querySelectorAll("[data-fc-language]");
    for (var i = 0; i < links.length; i += 1) {
      var language = links[i].getAttribute("data-fc-language");
      var href = targetFor(language);
      if (href) {
        links[i].setAttribute("href", href);
      }
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", apply);
  } else {
    apply();
  }
})();
