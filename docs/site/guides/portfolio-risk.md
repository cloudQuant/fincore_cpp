# Portfolio, risk, optimization and simulation

Portfolio inputs own their typed positions, transactions, prices and weights.
The returned models copy or own their data; callers should not rely on a view
remaining valid after the source frame changes.

Risk and optimization APIs report infeasible, nonconvergent or unavailable
fits through documented statuses/exceptions instead of silently returning a
plausible-looking number. Simulation APIs take an explicit seed where
documented; matching a seed does not imply cross-language RNG identity.

The API reference lists the exact result fields, failure statuses and warm-up
requirements. This guide only describes the shared ownership and failure
model; it does not turn a deferred optimization experiment into a supported
feature.
