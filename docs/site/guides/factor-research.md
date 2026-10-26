# Factor research

The native factor workflow separates preparation, evaluation and reporting:

1. `prepare_factor_data` aligns a factor panel with forward returns and assigns
   quantiles on the documented cross-section.
2. `analyze_factor` builds an owning model for tears, IC, turnover and group
   views.
3. `evaluate_factor_research` evaluates a factor panel against a price grid
   with explicit targets, periods, labels and preprocessing.
4. Report/chart builders consume the frozen result rather than recomputing
   backward dependencies in renderers.

Factor alignment, missing labels, quantile ties and label horizons are
contractual inputs. A high IC or a benchmark speedup is not evidence that a
factor is profitable. See the repository's dated benchmark records before
quoting performance numbers.
