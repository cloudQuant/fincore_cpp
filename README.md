# fincore_cpp

[![Docs](https://github.com/cloudQuant/fincore_cpp/actions/workflows/docs.yml/badge.svg?branch=master)](https://cloudquant.github.io/fincore_cpp/en/latest/)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](#requirements)

Native C++23 analytics for portfolios, factors, performance and risk — one typed
API over a local `DataFrame`, with no Python dependency at run time.

用于投资组合、因子、业绩与风险分析的原生 C++23 库。同一份文档提供两种语言：
[English](#english) · [简体中文](#简体中文)。

## English

`fincore_cpp` is a native C++23 library for quantitative portfolio, factor,
performance and risk analytics. It ships its own typed API over a local
`DataFrame` type and does not depend on Python, pandas or NumPy at runtime — nor
does it promise pandas object compatibility.

```cpp
#include <fincore/dataframe.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>

const auto returns = fincore::make_series(
    {fincore::utc_timestamp(2025, 1, 2), fincore::utc_timestamp(2025, 1, 3)},
    "returns", {0.01, -0.02});

const double sharpe = fincore::sharpe_ratio(returns, "returns");
const double drawdown = fincore::max_drawdown(returns, "returns");
```

### What it covers

| Area | What you get |
|---|---|
| Core data | `fincore::dataframe` (index + typed columns), UTC nanosecond timestamps, calendar and frequency helpers |
| Metrics | Returns, ratios, drawdowns, rolling statistics, tables, stability and consecutive-run statistics |
| Risk | Historical and parametric VaR/ES, GARCH/EGARCH, EVT (GPD/GEV), Bayesian cone, backtests and walk-forward validation |
| Portfolio | Positions, transactions, round trips, turnover, capacity, cash-flow-aware performance |
| Factor research | Analysis pipeline, quantile/IC studies, tears, artifacts, point-in-time joins, export contracts |
| Attribution | Brinson (BHB/Fachler), factor and style regression, exposures, prediction, performance attribution |
| Optimization | Mean-variance and risk-parity style solvers, projected gradient, box/sector constraints |
| Reporting | One report model rendered to HTML, XLSX, PDF, Plotly, Bokeh, Matplotlib-style axes and ImPlot |
| Runtime | Operation catalog and dispatch, analysis context caching, extension snapshots, serialization |
| Simulation | GBM, bootstrap, Latin hypercube, antithetic variates, correlation breakdowns |

### Requirements

- CMake 3.25+ and a C++23 compiler (MSVC 19.4+, Clang 17+, GCC 13+).
- Pinned git submodules (`third_party/DataFrame`, `third_party/implot`).

Python is used only for tooling, benchmarks and documentation, never by the
library.

### Build and test

```bash
git clone https://github.com/cloudQuant/fincore_cpp.git
cd fincore_cpp
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Install and consume from CMake:

```bash
cmake --install build --prefix /path/to/prefix
```

```cmake
find_package(fincore CONFIG REQUIRED COMPONENTS fincore)
target_link_libraries(my_target PRIVATE fincore::fincore)
```

Build options: `FINCORE_BUILD_TESTING` (default ON), `FINCORE_BUILD_IMPLOT`
(default OFF, GUI adapter), `FINCORE_BUILD_IMPLOT_DEMO` (default OFF),
`FINCORE_ENABLE_ASAN` and `FINCORE_ENABLE_UBSAN` (default OFF).

### Documentation

One bilingual documentation site, with a language switch in the top-right corner
of every page and your choice remembered between visits.

- Online: <https://cloudquant.github.io/fincore_cpp/en/latest/> ·
  <https://cloudquant.github.io/fincore_cpp/zh-cn/latest/>
- New to the library: the [tutorial](docs/site/tutorial.md) goes from prices to a
  rendered report in six steps; the [cookbook](docs/site/guides/cookbook.md)
  answers single tasks.
- Start here: installation, quickstart, conventions
- Guides: metrics, portfolio and risk, factor research, reports and runtime
- Reference: one page per public module, generated from the headers
- Repository sources: [`docs/site/`](docs/site/), build steps in
  [`docs/site/README.md`](docs/site/README.md)

Public coverage is tracked by `docs/site/api-ledger.tsv`: every public symbol is
documented, and the strict Sphinx builds (`-W -n --keep-going`) run for both
languages on every change.

### Performance

The repository includes a paired benchmark harness that compares the native
implementation against the pinned Python reference on identical inputs. On the
Alpha003 A-share research workload the 500-file development batch reached
**41.20x** over the reference runner; separate 1,000/2,000-file diagnostic pairs
reached **34.46x / 35.63x**. These are measurements of that workload, not a
guarantee for every API. See the
[iteration 9 closeout](docs/iterations/iteration-9-performance-30x-100x/closeout.md)
for methodology, memory results and the evidence levels behind each number.

### Repository layout

```
include/fincore/     public headers, one directory per module
src/                 implementation
tests/               unit, consumer and oracle tests (1092 cases)
examples/            render and dashboard demos (excluded from the default build)
benchmarks/          alpha003 research benchmark and layout experiments
docs/site/           bilingual documentation sources and build tooling
docs/iterations/     iteration records and acceptance evidence
tools/               Python tooling, oracles and replay harnesses
third_party/         pinned submodules
```

### Status and support

- 1092 C++ tests, plus strict bilingual documentation builds, are part of the
  routine gate.
- Supported compilers: MSVC, Clang and GCC on their C++23 settings; the CI matrix
  builds Windows and macOS, with a known GCC 13 constexpr-limit failure tracked
  in the iteration record.
- The documentation is live on GitHub Pages (English and Chinese) and on Read the
  Docs (one project, English: <https://fincore-cpp.readthedocs.io/en/latest/>);
  the language switch links the two hosts at the same page. The current state is
  always recorded in [the publication page](docs/site/publication.md).

### Contributing

Issues and pull requests are welcome. Please read
[`CONTRIBUTING`](docs/site/contributing.md) for the build, test and documentation
rules, and keep changes comment-only when they claim to be. Historical planning
and acceptance records live under [`docs/iterations/`](docs/iterations/) and
[`docs/acceptance/`](docs/acceptance/).

### License

MIT — see [`LICENSE`](LICENSE).

---

## 简体中文

`fincore_cpp` 是一个原生 C++23 库，用于量化投资组合、因子、业绩与风险分析。它在
本地 `DataFrame` 类型之上提供自己的类型化 API，运行时不依赖 Python、pandas 或
NumPy，也不承诺与 pandas 对象兼容。

```cpp
#include <fincore/dataframe.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>

const auto returns = fincore::make_series(
    {fincore::utc_timestamp(2025, 1, 2), fincore::utc_timestamp(2025, 1, 3)},
    "returns", {0.01, -0.02});

const double sharpe = fincore::sharpe_ratio(returns, "returns");
const double drawdown = fincore::max_drawdown(returns, "returns");
```

### 覆盖范围

| 领域 | 提供的能力 |
|---|---|
| 核心数据 | `fincore::dataframe`（索引 + 类型化列）、UTC 纳秒时间戳、日历与频率工具 |
| 指标 | 收益、比率、回撤、滚动统计、表格、稳定性与连续段统计 |
| 风险 | 历史法与参数法 VaR/ES、GARCH/EGARCH、EVT（GPD/GEV）、贝叶斯锥、回测与滚动前推验证 |
| 组合 | 持仓、交易、往返交易、换手、容量、含现金流的业绩计算 |
| 因子研究 | 分析流水线、分位/IC 研究、tears、artifacts、point-in-time 连接、导出契约 |
| 归因 | Brinson（BHB/Fachler）、因子与风格回归、暴露、预测、业绩归因 |
| 优化 | 均值方差与风险平价类求解器、投影梯度、箱式与行业约束 |
| 报表 | 单一报表模型渲染到 HTML、XLSX、PDF、Plotly、Bokeh、Matplotlib 风格坐标轴与 ImPlot |
| 运行时 | 操作目录与分发、分析上下文缓存、扩展快照、序列化 |
| 模拟 | GBM、bootstrap、拉丁超立方、对偶变量、相关性分解 |

### 环境要求

- CMake 3.25+ 与支持 C++23 的编译器（MSVC 19.4+、Clang 17+、GCC 13+）。
- 固定在 `third_party/DataFrame`、`third_party/implot` 下的 git 子模块。

Python 只用于工具、基准与文档，库本身不需要它。

### 构建与测试

```bash
git clone https://github.com/cloudQuant/fincore_cpp.git
cd fincore_cpp
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

安装并在 CMake 中消费：

```bash
cmake --install build --prefix /path/to/prefix
```

```cmake
find_package(fincore CONFIG REQUIRED COMPONENTS fincore)
target_link_libraries(my_target PRIVATE fincore::fincore)
```

构建选项：`FINCORE_BUILD_TESTING`（默认 ON）、`FINCORE_BUILD_IMPLOT`
（默认 OFF，图形界面适配器）、`FINCORE_BUILD_IMPLOT_DEMO`（默认 OFF）、
`FINCORE_ENABLE_ASAN` 与 `FINCORE_ENABLE_UBSAN`（默认 OFF）。

### 文档

同一套双语文档站点，每页右上角都有语言切换按钮，并会记住你的选择。

- 在线站点：<https://cloudquant.github.io/fincore_cpp/en/latest/> ·
  <https://cloudquant.github.io/fincore_cpp/zh-cn/latest/>
- 第一次使用：[教程](docs/site/tutorial.md) 用六步从价格走到渲染好的报表；
  [配方集](docs/site/guides/cookbook.md) 回答单个具体任务。
- 入门：安装、快速开始、数据与数值约定
- 指南：指标、组合与风险、因子研究、报表与运行时
- 参考：每个公共模块一页，直接由头文件生成
- 仓库内源码：[`docs/site/`](docs/site/)，构建步骤见
  [`docs/site/README.md`](docs/site/README.md)

公共覆盖由 `docs/site/api-ledger.tsv` 跟踪：每个公共符号都有说明，且两种语言的
严格 Sphinx 构建（`-W -n --keep-going`）在每次改动上都会执行。

### 性能

仓库内有一套成对基准，用相同输入比较原生实现与固定的 Python 参考实现。在 Alpha003
A 股研究负载上，500 文件的开发批达到 **41.20x**；另外的 1,000/2,000 文件诊断对达到
**34.46x / 35.63x**。这些是该负载的测量结果，不构成对所有 API 的保证。方法与内存
结果见[迭代 9 收尾记录](docs/iterations/iteration-9-performance-30x-100x/closeout.md)。

### 仓库结构

```
include/fincore/     公共头文件，每个模块一个目录
src/                 实现
tests/               单元、消费者与 oracle 测试（1092 个用例）
examples/            报表与仪表盘示例（默认构建不包含）
benchmarks/          alpha003 研究基准与内存布局实验
docs/site/           双语文档源码与构建工具
docs/iterations/     迭代记录与验收证据
tools/               Python 工具、oracle 与回放脚本
third_party/         固定子模块
```

### 状态与支持

- 1092 个 C++ 测试与双语严格文档构建都属于例行门禁。
- 支持的编译器：MSVC、Clang、GCC 的 C++23 配置；CI 覆盖 Windows 与 macOS，
  GCC 13 的 constexpr 限制导致的 `ubuntu-latest` 失败已在迭代记录中登记。
- 文档已上线 GitHub Pages（中英文）与 Read the Docs（单一项目，英文：
  <https://fincore-cpp.readthedocs.io/en/latest/>）；语言切换会在同一页面上连接
  两个宿主。当前状态始终记录在[发布说明](docs/site/publication.md)。

### 参与贡献

欢迎提交 issue 与 pull request。构建、测试与文档规则见
[`CONTRIBUTING`](docs/site/contributing.md)；历史计划与验收记录位于
[`docs/iterations/`](docs/iterations/) 与 [`docs/acceptance/`](docs/acceptance/)。

### 许可证

MIT，详见 [`LICENSE`](LICENSE)。
