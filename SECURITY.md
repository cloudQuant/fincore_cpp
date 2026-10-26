# Security policy

## Supported versions

Security fixes are applied to the `master` branch, which is the published branch.
Older commits and iteration branches are not maintained.

## Reporting a vulnerability

Please do not open a public issue for a suspected vulnerability. Use GitHub's
private vulnerability reporting on
<https://github.com/cloudQuant/fincore_cpp/security/advisories/new>, or contact
the maintainers listed in the repository profile.

Include the version or commit, the platform and compiler, a minimal reproduction,
and what an attacker could achieve. We aim to acknowledge a report within a few
working days and to publish an advisory once a fix is available.

## Scope

The library parses numeric data, serialized reports, manifests and CSV exports.
Reports about memory safety, arithmetic that silently produces a plausible but
wrong result, or a serialization path that accepts malformed input are in scope.
Benchmark numbers and documentation wording are not.
