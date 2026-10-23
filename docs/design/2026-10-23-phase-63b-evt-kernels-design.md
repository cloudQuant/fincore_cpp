# Phase 63 Slice 2 Design — EVT Hill and GPD Fits

## Source contract

- Finite filtering; lower tail = reflected losses, upper = gains.
- Default threshold = 90th linear percentile of selected magnitudes;
  explicit thresholds finite/positive.
- Strict exceedances; at least 10 required.
- Hill: mean log ratio to the threshold.
- GPD PWM: `b0`, `b1`, `l2`, `xi = 2 - b0/l2`, `beta = b0*(1-xi)`.
- GPD MLE: **source minimizes the raw log-likelihood** under
  `xi in [-0.5,1]`, `beta >= 1e-6` from `[0.1, std(excesses)]`. This is a
  source sign defect (a maximization written as a minimization); on the
  oracle fixture it drives `xi` to the 1.0 bound while beta stays positive.
  The native port reproduces the observed behaviour and records the defect
  instead of silently switching objectives, because iteration acceptance is
  parity with the pinned oracle.

## Native decisions

- Enums replace the `tail`/`method` strings (invalid strings are impossible at
  the boundary; the enum branches are tested).
- Threshold/percentile code is shared with the other native quantile helpers
  (linear interpolation), and strict exceedance selection preserves order.
- MLE bounds use transforms (`xi` sigmoid over [-0.5,1], `beta = 1e-6+exp`) and
  Phase 60 Nelder-Mead at the source's 1e-4 tolerances (600 iterations), so
  the acceptance path mirrors SciPy's stopping regime.
- Returned types own their observations/parameters; no spans or retained
  storage.

## Oracle evidence

Root `/tmp/fincore-phase63b-python.agf0sS`, detached at `4b8f31b`, exit `0`,
zero stderr. Script SHA-256
`4ef7ca003a48ad5c0cb9f0720fe5a1ad2b9ae961ae9572b7a32a230528f5d1e7`; raw
output SHA-256
`dfd206b0c84cae3ca3e6634622ae70cd7f6b4ce1ba61786c23c7e0941df62314`.

500-point fixture: Hill lower `0.5949755669081104` with 26 observations;
upper `0.07553762175129869` with 25. GPD PWM `xi 0.6969927826510689`,
`beta 0.00682871573607405`, threshold `0.012776700681180947`, 26
exceedances; MLE `xi 1.0` (bound), `beta 0.0009162186742098094`.
Validation: threshold 0.07 has too few exceedances; explicit 0.5 threshold
leaves too few Hill observations.

## Tests

Four EVT tests: exact Hill/PWM goldens, MLE bound behaviour, and validation
errors.
