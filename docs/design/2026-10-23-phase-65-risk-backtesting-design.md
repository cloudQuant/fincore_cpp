# Phase 65 Design — Risk Backtesting

## Source contract

`risk/backtesting.py`:

- `_align`: timestamp alignment, NaN-pair drop, no-overlap error.
- `kupiec_lr`: `max(0, 2*(x*log(x/(n*p)) + (n-x)*log((n-x)/(n*(1-p)))))`
  with the 0·log(0) limit; zero observations → 0.
- `christoffersen_lr`: multinomial transition counts with `0*log(0)=0`.
- `_chi2_pvalue`: chi-square survival; df is always 1 here, so the native
  implementation uses the closed form `erfc(sqrt(lr/2))`.
- `backtest_var`: strict realized-below-forecast exceptions; status gates
  (n<3 or expected<5 → inconclusive; p<significance → fail; else pass);
  digest of the aligned frame.
- `backtest_es`: `1-confidence` linear realized quantile, tail mean,
  aligned forecast mean, seeded bootstrap tail standard deviation, z-score,
  always `experimental`.

## Native decisions

- Status and method become enums; diagnostics keep numeric entries
  (`significance`, `small_sample` as 0/1, ES realized/forecast/z/bootstrap
  count/seed).
- **Digest divergence:** Python SHA-256 hashes `DataFrame.to_csv` bytes,
  which depend on pandas float formatting. The native digest is SHA-256 over
  a canonical binary serialization (date ns + IEEE-754 bits, big-endian).
  It is deterministic and input-sensitive natively, but not byte-equal to
  the Python digest; this is declared instead of approximated.
- **Bootstrap RNG divergence:** NumPy `default_rng` streams are not
  reproduced; the native bootstrap uses `mt19937_64` with the same default
  seed. Realized/forecast ES fields and exception counts are oracle-exact;
  the z-score is deterministic natively but not oracle-equal.
- A compiled-in SHA-256 implementation keeps the library dependency-free.

## Oracle evidence

Root `/tmp/fincore-phase65-python.wu18Ed`, detached at `4b8f31b`, exit `0`,
zero stderr. Script SHA-256
`55e7d38cdd671c010af7481eca961ca5af06c4d06049a4f3806b7d85263e02ef`; raw
output SHA-256
`3007b2a49a6b6af19cea86df8aa4ae4f569063a10b0fc43be65ac19d6ce80d93`.

Goldens: Kupiec `(250,0)→5.025167926750753`, `(250,3)→0.09494012266442442`,
`(10,10)→92.10340371976181`, `(0,0)→0`; Christoffersen pattern
`0.004560555262997923`, single `0`; VaR backtest 250 observations, 0
exceptions, expected `2.500000000000002`, Kupiec/p `0.02498150305344931`,
status inconclusive (small sample); 0.9 confidence → fail with Kupiec
`52.68025782891318` and p `3.925168885725146e-13`; short → inconclusive;
ES with 100 bootstrap draws: 7 exceptions, realized ES
`-0.009935211388974519`, forecast `-0.012`, experimental.

## Tests

Three tests: exact LR values, VaR fields/statuses/validation, ES
deterministic fields and digest determinism.
