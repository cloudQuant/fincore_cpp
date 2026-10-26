"""Sphinx configuration for the fincore_cpp bilingual documentation site."""

from __future__ import annotations

import os
import re
from pathlib import Path

SITE_DIR = Path(__file__).resolve().parent
BUILD_DIR = SITE_DIR.parent / "_build"
DOXYGEN_XML = BUILD_DIR / "doxygen" / "xml"

project = "fincore_cpp"
copyright = "2026, fincore_cpp contributors"
author = "fincore_cpp contributors"
version = "0.1.0"
release = "0.1.0"

extensions = [
    "breathe",
    "myst_parser",
    "sphinx.ext.autosectionlabel",
    "sphinx.ext.intersphinx",
]

breathe_projects = {"fincore_cpp": str(DOXYGEN_XML)}
breathe_default_project = "fincore_cpp"
breathe_default_members = ("members", "undoc-members")
breathe_show_include = False

myst_enable_extensions = [
    "colon_fence",
    "deflist",
    "fieldlist",
    "tasklist",
    "attrs_inline",
]
myst_heading_anchors = 3
myst_fence_as_directive = {"mermaid"}

source_suffix = {".md": "markdown", ".rst": "restructuredtext"}
master_doc = "index"
_raw_language = (os.environ.get("FINCORE_DOCS_LANGUAGE") or
                 os.environ.get("READTHEDOCS_LANGUAGE") or "en")
language = "zh_CN" if _raw_language.lower().startswith("zh") else "en"
locale_dirs = ["locale"]
gettext_compact = False
gettext_additional_targets = ["literal-block"]
exclude_patterns = [
    "_build",
    "locale/**/*.mo",
    "tools",
    "README.md",
]

autosectionlabel_prefix_document = True
nitpicky = True
keep_warnings = True
# Pilot exceptions: aliases/constants are not yet rendered as standalone
# compounds. They are listed in the API ledger and will be resolved as the
# module pages are expanded; no global warning suppression is used.
nitpick_ignore = [
    ("cpp:identifier", "fincore"),
    ("cpp:identifier", "dataframe"),
    ("cpp:identifier", "timestamp"),
    ("cpp:identifier", "daily_annualization"),
    ("cpp:identifier", "returns_column"),
    ("cpp:identifier", "report_section"),
    ("cpp:identifier", "report_document_schema_version"),
    ("cpp:identifier", "report_metric"),
    ("cpp:identifier", "report_table"),
    ("cpp:identifier", "report_series"),
    # External DataFrame namespace (hmdf) is not part of this API surface.
    ("cpp:identifier", "hmdf"),
    ("cpp:identifier", "hmdf::StdDataFrame<timestamp>"),
    # Namespace and member typedefs that Breathe does not emit as standalone
    # C++ domain targets.
    ("cpp:identifier", "fincore::extensions"),
    ("cpp:identifier", "extensions"),
    ("cpp:identifier", "storage_type::const_iterator"),
    ("cpp:identifier", "storage_type::value_type"),
    # Public string-literal constant whose multi-line initializer is not
    # representable by the C++ domain; it remains documented in the source.
    ("cpp:identifier", "basel_reference_disclosure"),
]

html_theme = "sphinx_rtd_theme"
_DOCS_BASE = (os.environ.get("FINCORE_DOCS_BASEURL") or
              os.environ.get("READTHEDOCS_CANONICAL_URL") or
              "https://docs.example.invalid/fincore_cpp/")
# Everything before the "<language>/latest/" segment, so the sidebar switcher
# can build a sibling-language URL without JavaScript as a fallback.
fc_docs_root = re.sub(r"(en|zh-cn|zh_CN)/.*$", "", _DOCS_BASE)
html_theme_options = {
    "navigation_depth": 3,
    "collapse_navigation": False,
    "sticky_navigation": True,
    "includehidden": True,
    "titles_only": False,
}
html_title = "fincore_cpp documentation"
html_short_title = "fincore_cpp"
templates_path = ["_templates"]
html_static_path = ["_static"]
html_css_files = ["custom.css"]
html_js_files = ["language_switch.js"]
html_baseurl = (os.environ.get("FINCORE_DOCS_BASEURL") or
                os.environ.get("READTHEDOCS_CANONICAL_URL") or
                "https://docs.example.invalid/fincore_cpp/")
html_context = {
    "display_language_switch": True,
    "fc_docs_root": fc_docs_root,
    "languages": [("English", "/en/latest/"), ("简体中文", "/zh-cn/latest/")],
}
html_show_sourcelink = True
html_sourcelink_suffix = ""

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
}
