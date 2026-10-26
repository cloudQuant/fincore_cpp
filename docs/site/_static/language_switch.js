// Keep the reader on the same page when they switch documentation language.
// Both builds share one page tree, so the sibling URL is the current path with
// only the language segment replaced; search strings and anchors survive. The
// Chinese build directory is spelled `zh-cn` on GitHub Pages and `zh_CN` in local
// builds and on Read the Docs, so both spellings are probed. If no sibling page
// answers, the click falls back to that language's home page instead of a 404.
(function () {
  "use strict";

  var languages = ["en", "zh-cn", "zh_CN", "zh"];
  var spellings = { "en": ["en"], "zh-cn": ["zh-cn", "zh_CN", "zh"] };
  var storageKey = "fincore-docs-language";
  // One documentation project on Read the Docs serves the English source; the
  // Chinese build lives on GitHub Pages. Both hosts mount pages at
  // /<language>/<version>/<page>, so the suffix can be carried across hosts.
  var pagesHome = "https://cloudquant.github.io/fincore_cpp/";

  function onReadTheDocs() {
    return /readthedocs\.io$/i.test(window.location.hostname);
  }

  function versionSuffix() {
    var parts = document.location.pathname.split("/");
    var index = languageIndex(parts);
    return index === -1 ? "" : parts.slice(index + 2).join("/");
  }

  function languageIndex(parts) {
    for (var i = 0; i < parts.length; i += 1) {
      if (languages.indexOf(parts[i]) !== -1) {
        return i;
      }
    }
    return -1;
  }

  function siblingPath(language, spelling) {
    var parts = document.location.pathname.split("/");
    var index = languageIndex(parts);
    if (index === -1) {
      return null;
    }
    parts[index] = spelling;
    return parts.join("/") + document.location.search + document.location.hash;
  }

  function homePath(spelling) {
    var parts = document.location.pathname.split("/");
    var index = languageIndex(parts);
    if (index === -1) {
      return null;
    }
    return parts.slice(0, index).join("/") + "/" + spelling + "/latest/";
  }

  function candidates(language) {
    if (onReadTheDocs() && language === "zh-cn") {
      return [pagesHome + "zh-cn/latest/" + versionSuffix() +
              document.location.search + document.location.hash];
    }
    var names = spellings[language] || [language];
    var urls = [];
    for (var i = 0; i < names.length; i += 1) {
      var url = siblingPath(language, names[i]);
      if (url) {
        urls.push(url);
      }
    }
    return urls;
  }

  function homeCandidates(language) {
    var names = spellings[language] || [language];
    var urls = [];
    for (var i = 0; i < names.length; i += 1) {
      var url = homePath(names[i]);
      if (url) {
        urls.push(url);
      }
    }
    return urls;
  }

  function remember(language) {
    try {
      window.localStorage.setItem(storageKey, language);
    } catch (error) {
      /* storage can be blocked; the switch still works without it */
    }
  }

  function reachable(urls, index, done) {
    if (index >= urls.length) {
      done(null);
      return;
    }
    fetch(urls[index], { method: "HEAD" }).then(function (response) {
      if (response.ok) {
        done(urls[index]);
      } else {
        reachable(urls, index + 1, done);
      }
    }).catch(function () {
      reachable(urls, index + 1, done);
    });
  }

  function apply() {
    var links = document.querySelectorAll("[data-fc-language]");
    for (var i = 0; i < links.length; i += 1) {
      var language = links[i].getAttribute("data-fc-language");
      var direct = candidates(language);
      if (direct.length > 0) {
        links[i].setAttribute("href", direct[0]);
      }
      links[i].addEventListener("click", (function (chosen) {
        return function (event) {
          remember(chosen);
          if (!window.fetch) {
            return;
          }
          event.preventDefault();
          reachable(candidates(chosen), 0, function (url) {
            if (url) {
              document.location.href = url;
              return;
            }
            reachable(homeCandidates(chosen), 0, function (home) {
              document.location.href = home || homeCandidates(chosen)[0] ||
                                       document.location.href;
            });
          });
        };
      })(language));
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", apply);
  } else {
    apply();
  }
})();
