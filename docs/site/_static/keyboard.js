// Ctrl+K (or "/") focuses the documentation search box, the way most modern
// documentation sites behave. Typing in a field or using a modifier for something
// else is left alone, and Escape returns focus to the page.
(function () {
  "use strict";

  function searchBox() {
    return document.querySelector('input[name="q"], #rtd-search-form input');
  }

  function isTypingTarget(target) {
    if (!target) {
      return false;
    }
    var tag = target.tagName;
    return tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT" ||
           target.isContentEditable;
  }

  function focusSearch(event) {
    if (isTypingTarget(event.target)) {
      return;
    }
    var box = searchBox();
    if (!box) {
      return;
    }
    var wantsShortcut = (event.key === "k" || event.key === "K")
      ? (event.ctrlKey || event.metaKey)
      : (event.key === "/" && !event.ctrlKey && !event.metaKey && !event.altKey);
    if (!wantsShortcut) {
      return;
    }
    event.preventDefault();
    box.focus();
    box.select();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", function () {
      document.addEventListener("keydown", focusSearch);
    });
  } else {
    document.addEventListener("keydown", focusSearch);
  }
})();
