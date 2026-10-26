"""Make element ids unique within each document.

Breathe emits the same section anchor (for example
``breathe-section-title-public-members``) once per class that has a members
section, so a module page can carry the same id several times. Duplicate ids are
invalid HTML and make every later anchor point at the first occurrence. The
first occurrence keeps its id; later ones get a numeric suffix, so existing
references (which resolved to the first one anyway) keep working.
"""

from __future__ import annotations

from docutils import nodes


def _dedupe(doctree: nodes.document) -> int:
    seen: set[str] = set()
    renamed = 0
    for node in doctree.findall(nodes.Element):
        ids = node.get("ids")
        if not ids:
            continue
        unique = []
        for identifier in ids:
            candidate = identifier
            suffix = 2
            while candidate in seen:
                candidate = f"{identifier}-{suffix}"
                suffix += 1
            if candidate != identifier:
                renamed += 1
            seen.add(candidate)
            unique.append(candidate)
        node["ids"] = unique
    return renamed


def setup(app):
    def on_doctree_resolved(app, doctree, docname):
        _dedupe(doctree)

    app.connect("doctree-resolved", on_doctree_resolved)
    return {"version": "1.0", "parallel_read_safe": True,
            "parallel_write_safe": True}
