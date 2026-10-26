# 审阅豁免清单（迭代 10）

**日期：** 2026-10-26（Asia/Shanghai）。
**范围：** 只登记"未写完整标签但没有被伪装成已文档化"的项目；每项含范围、理由与复查条件。
验收记录：[2026-10-26-iteration-10-comments-bilingual-docs.md](../../acceptance/2026-10-26-iteration-10-comments-bilingual-docs.md)。

统计口径来自 [`api-ledger-summary.json`](../../site/api-ledger-summary.json)
与 `docs/site/api-ledger.tsv`，公共符号 2484 条，说明覆盖 2484 条（缺失 0）。
下列豁免不计入"已完整说明"，只说明为何在某一项标签上保持现状。

## E-01 自动生成的继承列表（3 条，中文保持英文）

Breathe 生成的 `Subclassed by …` 段落内是类型引用节点；gettext 译文无法携带
引用节点，Sphinx 会以 `i18n.inconsistent_references` 报错，而译文里写
`:cpp:class:` 角色不会被解析（会原样显示）。因此这三条保留英文：

- `core.po`: `Subclassed by fincore::batch_fetch_error, fincore::research_error`
- `factor.po`: `Subclassed by fincore::enhanced_non_matching_timezone_error, fincore::factor_loss_exceeded`
- `factor.po`: `Subclassed by fincore::research_config_error, fincore::research_data_quality_error, fincore::research_dataset_error, fincore::research_dependency_error, fincore::research_factor_computation_error`

内容为纯标识符列表，属于"标识符等非翻译项"。复查条件：Sphinx 若在译文段落
中支持引用节点保留，即可翻译。

## E-02 无法渲染为声明的字符串常量（2 条）

`fincore::DEFAULT_HTML_CSS`（`report/renderers.hpp`）与
`fincore::basel_reference_disclosure`（`risk/report.hpp`）是
`inline constexpr std::string_view`，初值是跨多行的字符串字面量。Sphinx C++
域解析不了这种声明，`doxygenvariable` 会报 `Invalid C++ declaration`。
生成器 `docs/site/tools/generate_api_pages.py` 的 `UNRENDERABLE` 集合跳过这
两条，接口面仍由头文件注释与符号清单记录。复查条件：若 Breathe/Sphinx 支持
带长初值的变量声明，则从该集合移除。

## E-03 未使用 `@param` 列表的函数（296 条）

`requirements.md` 规定 `@param` 按需使用。以下函数的参数单位、对齐与失败条件
在概述句中已经写明（或由 `api/index.md` 的通用约定覆盖），因此本轮不追加参数
列表；数值约束、所有权与副作用相关的说明并未省略。按文件统计：

| 模块 | 文件 | 条数 |
|---|---|---:|
| factor | `factor/calendar.hpp` | 2 |
| factor | `factor/data.hpp` | 1 |
| factor | `factor/serialization.hpp` | 9 |
| metrics | `metrics/alpha_beta.hpp` | 8 |
| metrics | `metrics/basic.hpp` | 2 |
| metrics | `metrics/bayesian.hpp` | 9 |
| metrics | `metrics/consecutive.hpp` | 18 |
| metrics | `metrics/drawdown.hpp` | 21 |
| metrics | `metrics/frequencies.hpp` | 3 |
| metrics | `metrics/helpers.hpp` | 13 |
| metrics | `metrics/perf_attrib.hpp` | 7 |
| metrics | `metrics/perf_stats.hpp` | 5 |
| metrics | `metrics/ratios.hpp` | 23 |
| metrics | `metrics/returns.hpp` | 11 |
| metrics | `metrics/risk.hpp` | 11 |
| metrics | `metrics/rolling.hpp` | 15 |
| metrics | `metrics/stats.hpp` | 13 |
| metrics | `metrics/timing.hpp` | 5 |
| metrics | `metrics/yearly.hpp` | 7 |
| runtime | `runtime/runtime.hpp` | 26 |
| runtime | `runtime/serialization.hpp` | 7 |
| runtime | `runtime/utilities.hpp` | 13 |
| viz | `viz/bokeh_document.hpp` | 11 |
| viz | `viz/chart_data.hpp` | 9 |
| viz | `viz/html_builder.hpp` | 14 |
| viz | `viz/implot_backend.hpp` | 16 |
| viz | `viz/plotly_document.hpp` | 13 |
| viz | `viz/render_model.hpp` | 4 |

抽样依据（同一轮内核对实现与测试后判定）：
`metrics/ratios.hpp` 的 `adjusted_sharpe_ratio` 说明了年化口径、偏度峰度调整、
样本量下限与 `column` 含 NUL 时的异常；`metrics/basic.hpp` 的
`adjust_returns` 说明了列必须存在、时间戳保留、NaN 保持；`viz/chart_data.hpp`
的 `build_monthly_heatmap` 说明了索引必须升序、`column` 必须是 double 列以及
缺失月份保持 NaN。三者的参数含义均已在正文中给出，`@param` 只重复同一信息。

复查条件：任何后续修改若让某个参数的单位、默认值或失败条件不再能从概述句
读出，必须在同一轮补 `@param`；`api-ledger.tsv` 的 `param_docs` 列用于跟踪。

## E-04 精确的 `nitpick_ignore` 条目（17 条）

`docs/site/conf.py` 只列出**具体标识符**，没有使用通配或整域忽略：

- 由第三方 DataFrame 子模块提供的类型：`dataframe`、`hmdf`、
  `hmdf::StdDataFrame<timestamp>`、`timestamp`。
- Doxygen 在签名与正文中引用、但 Breathe 不作为独立 C++ 域目标输出的名字：
  `fincore`、`extensions`、`fincore::extensions`、
  `storage_type::const_iterator`、`storage_type::value_type`、
  `daily_annualization`、`returns_column`。
- 常量与报告结构成员：`report_document_schema_version`、`report_metric`、
  `report_table`、`report_series`、`report_section`、
  `basel_reference_disclosure`（见 E-02）。

复查条件：新增条目必须对应一个具体的未解析标识符，并说明其归属；如果某标识
符后来成为公共接口面的一部分，应在 API 页面渲染它并删除对应忽略项。

## E-05 生成期约束

- `docs/site/Doxyfile` 中 `AUTOLINK_SUPPORT = NO`：Doxygen 自动生成的引用节点
  无法进入 gettext 译文（见 E-01 同类问题），因此关闭自动链接，叙述页面改用
  显式的 `{cpp:class}` 角色。
- `docs/site/tools/normalize_doxygen_xml.py`：Doxygen 会把 C++14 数字分隔符
  原样写入 `<argsstring>`，Sphinx C++ 域解析失败。该脚本只重写生成 XML 中的
  参数字符串，不改变任何被文档化的取值；已接入
  `.readthedocs.yaml` 与 `.github/workflows/docs.yml`。生成物
  `docs/_build/` 不进入 Git。

## E-06 平台相关未验证项

Linux（Ubuntu）上的 Doxygen/Sphinx 构建、Read the Docs 两个语言项目、GitHub
Pages 两个语言路径均在本机未验证；详见验收记录第 5 节。这些不是文档内容豁
免，而是发布状态 `PLATFORM_PENDING`。
