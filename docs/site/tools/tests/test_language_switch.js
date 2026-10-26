// Checks the host-aware language switch without a browser.
//
// One Read the Docs project serves English; the Chinese build lives on GitHub
// Pages. These cases pin the three host situations the script has to handle:
// RTD (Chinese leaves for Pages), Pages-English and Pages-Chinese.
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const script = fs.readFileSync(
  path.join(__dirname, "..", "..", "_static", "language_switch.js"), "utf8");

function evaluate(hostname, pathname) {
  const links = ["en", "zh-cn"].map(function (language) {
    return {
      attrs: { "data-fc-language": language },
      getAttribute: function (name) { return this.attrs[name] || null; },
      setAttribute: function (name, value) { this.attrs[name] = value; },
      addEventListener: function () {},
    };
  });
  const location = { hostname, pathname, search: "", hash: "", href: "" };
  const sandbox = {
    document: {
      location,
      readyState: "complete",
      querySelectorAll: function () { return links; },
      addEventListener: function () {},
      documentElement: { lang: "en" },
    },
    navigator: {},
    console: { log() {}, warn() {}, error() {} },
  };
  sandbox.window = {
    location,
    document: sandbox.document,
    localStorage: { getItem: function () { return null; }, setItem: function () {} },
    fetch: function () { return new Promise(function () {}); },
    setTimeout: function () {},
  };
  vm.createContext(sandbox);
  vm.runInContext(script, sandbox);
  return { en: links[0].attrs.href, zh: links[1].attrs.href };
}

const cases = [
  {
    name: "Read the Docs sends Chinese to Pages",
    host: "fincore-cpp.readthedocs.io",
    page: "/en/latest/quickstart.html",
    expect: {
      en: "/en/latest/quickstart.html",
      zh: "https://cloudquant.github.io/fincore_cpp/zh-cn/latest/quickstart.html",
    },
  },
  {
    name: "Pages English stays on Pages",
    host: "cloudquant.github.io",
    page: "/fincore_cpp/en/latest/quickstart.html",
    expect: {
      en: "/fincore_cpp/en/latest/quickstart.html",
      zh: "/fincore_cpp/zh-cn/latest/quickstart.html",
    },
  },
  {
    name: "Pages Chinese switches back to English",
    host: "cloudquant.github.io",
    page: "/fincore_cpp/zh-cn/latest/quickstart.html",
    expect: {
      en: "/fincore_cpp/en/latest/quickstart.html",
      zh: "/fincore_cpp/zh-cn/latest/quickstart.html",
    },
  },
];

let failures = 0;
for (const entry of cases) {
  const actual = evaluate(entry.host, entry.page);
  const ok = actual.en === entry.expect.en && actual.zh === entry.expect.zh;
  console.log((ok ? "ok   " : "FAIL ") + entry.name);
  if (!ok) {
    failures += 1;
    console.log("  expected", entry.expect);
    console.log("  actual  ", actual);
  }
}
process.exit(failures === 0 ? 0 : 1);
