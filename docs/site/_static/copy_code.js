// Add a copy button to every highlighted code block. The button is created here
// rather than in the templates so a page without JavaScript simply keeps the
// plain block, and so both language builds get it from one file.
(function () {
  "use strict";

  var label = document.documentElement.lang &&
              document.documentElement.lang.toLowerCase().indexOf("zh") === 0
    ? "复制" : "Copy";
  var copiedLabel = label === "复制" ? "已复制" : "Copied";

  function copyText(text) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      return navigator.clipboard.writeText(text);
    }
    var area = document.createElement("textarea");
    area.value = text;
    area.setAttribute("readonly", "");
    area.style.position = "absolute";
    area.style.left = "-9999px";
    document.body.appendChild(area);
    area.select();
    document.execCommand("copy");
    document.body.removeChild(area);
    return Promise.resolve();
  }

  function attach(block) {
    var pre = block.querySelector("pre");
    if (!pre || block.querySelector(".fc-copy")) {
      return;
    }
    var button = document.createElement("button");
    button.type = "button";
    button.className = "fc-copy";
    button.textContent = label;
    button.setAttribute("aria-label", label + " code");
    button.addEventListener("click", function () {
      copyText(pre.innerText).then(function () {
        button.textContent = copiedLabel;
        button.classList.add("fc-copied");
        window.setTimeout(function () {
          button.textContent = label;
          button.classList.remove("fc-copied");
        }, 1500);
      });
    });
    block.appendChild(button);
  }

  function apply() {
    var blocks = document.querySelectorAll("div.highlight");
    for (var i = 0; i < blocks.length; i += 1) {
      attach(blocks[i]);
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", apply);
  } else {
    apply();
  }
})();
