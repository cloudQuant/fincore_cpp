# 迭代 10：Google 风格注释与中英文在线文档

**版本：** R1，2026-10-25（Asia/Shanghai）。

**状态：** IN_PROGRESS — P126–P132 在本机通过，P131 为 PARTIAL，P133 为
PLATFORM_PENDING，P134 的本机收尾项已完成（验收记录第 7 节），但因门禁要求四种
线上组合，仍记为 PARTIAL。实施证据见
[验收记录](../../acceptance/2026-10-26-iteration-10-comments-bilingual-docs.md)
与[审阅豁免清单](exemptions.md)。

**基线：** 现有 `dev`，`8a1174fe5a08fd524f6728536fc0b34c55d59c27`。

本迭代计划为 fincore_cpp 建立可维护的代码说明和中英文文档站：按 Google
风格解释接口契约和非显然的实现原因，从公共头文件生成 API 参考，以同一份
文档源支持 Read the Docs 和 GitHub Pages。两种语言应覆盖相同的用户任务，
包括安装、基本使用、功能边界、Python fincore 迁移和性能结果解读。

实施轮只包含注释、文档站配置、译文与发布配置；未修改任何签名、默认值、
表达式、常量、异常路径、包含顺序、宏或 ABI，改动范围由
`docs/site/tools/check_comment_only.py` 与全量 CTest 验证。行为缺陷不在注释轮
内修复：先登记，经审阅后另行安排修复轮次。

## 阅读入口

| 文档 | 内容 |
|---|---|
| [需求与注释规范](requirements.md) | 范围、Google 风格、禁止混入的改动、双语要求 |
| [文档架构与发布设计](design.md) | Doxygen/Sphinx 方案、翻译、导航、两个平台和版本管理 |
| [验收与实施流程](acceptance.md) | 每轮检查、覆盖分母、双语门槛、发布与交接 |
| [待审阅问题](review-findings.md) | 只读发现的文档问题及后续缺陷登记规则 |
| [审阅豁免清单](exemptions.md) | 标识符列表、无法渲染的常量、`@param` 按需项、精确 `nitpick_ignore` |
| [验收记录](../../acceptance/2026-10-26-iteration-10-comments-bilingual-docs.md) | 本轮实际命令、计数、结果与未执行项 |

## 当前起点

计划阶段的只读盘点：公共目录 117 个 `.hpp`，`src` 106 个 C++ 文件，
`tests` 138 个 C++ 文件，`tools` 31 个 Python 文件；当时 58 个头文件出现注释
标记、39 个出现 Doxygen 可识别的特殊注释标记。这只是文件级扫描，不是 API
文档覆盖率。

P131 的范围进度：`src/` 全部 106 个文件已完成审阅并各有一条说明，`tools/` 另有
16 个脚本因模块 docstring 已覆盖用途与调用方式而记为“审阅后无需改动”；其余 153 个
条目位于 `tests/`、`tools/`、`examples/`、`benchmarks/`，尚未逐项确认。

实施后的事实基线（2026-10-26）：`docs/site/api-ledger.tsv` 记录 2484 个公共
符号（含重载逐项），全部有说明；117 个头文件中 97 个在本轮修改并通过
comment-only 校验；文档站由 Doxygen 1.9.7 + Breathe 5.1.0 + Sphinx 9.2.0
构建，中英双语严格构建均通过，1092 个 C++ 测试全部通过；收尾度量已由
`docs/site/tools/check_built_site.py` 固化为可复跑命令。GitHub Pages 已在本日
开启（`build_type: workflow`），`docs` 工作流的 `build`、`pages` 两步通过，但
`deploy` 被自动创建的 `github-pages` 环境的分支策略拒绝（允许列表为空），线上
地址仍返回 404；Read the Docs 尚无项目。因此 P133 保持 PLATFORM_PENDING，
P134 只在本机门禁范围内完成。

[迭代 8 验收记录](../../acceptance/2026-10-24-iteration-8-native-parity-acceptance.md)
和[最新 HTML 基准记录](../../acceptance/2026-10-25-alpha003-html-benchmark.md)
可作为现状依据；[迭代 9 收尾记录](../iteration-9-performance-30x-100x/closeout.md)
保留性能工作的实际完成边界。本迭代不重启延期的性能优化，不追认历史未通过项。

## 分轮任务

编号接续 P125。状态列为 2026-10-26 的实施结果，具体命令与计数见
[验收记录](../../acceptance/2026-10-26-iteration-10-comments-bilingual-docs.md)。

| 阶段 | 任务与交付 | 前置条件 | 主要验收 | 状态 |
|---|---|---|---|---|
| P126 | 冻结自有代码与公共符号清单；确定规范、术语表、站点页面清单；审阅问题登记 | 启动实施 | 每个目录、符号和页面有归属；排除项有理由；未混入 bug 修复 | PASS |
| P127 | 文档工具链试点、最小双语站、API 翻译验证、早期文档 CI | P126 | 模板/重载等代表接口可提取；中文 API 正文可翻译；严格双语构建通过 | PASS |
| P128 | 基础类型、数据、时间、metrics/performance/rolling 接口与必要实现注释 | P127 | 对照实现及测试审阅单位、缺失值、对齐和年化规则；同轮更新中文 | PASS |
| P129 | portfolio、risk、optimization、simulation 及相关模型接口 | P127；涉及基础契约时依赖 P128 | 所有权、约束、随机性、拟合/求解失败和返回状态说明有依据 | PASS |
| P130 | factor、研究流程、report、runtime、viz 与可选适配接口 | P127；相关基础契约已确认 | 数值边界、缓存、序列化、输出文件和可选依赖契约完整 | PASS |
| P131 | 内部算法注释补审；examples、tests、benchmarks、tools 使用说明 | 对应模块轮次 | 解释关键不变量、测试依据和计时范围；Python docstring 单独审阅 | PARTIAL |
| P132 | 完成双语教程、操作指南、解释文档和迁移说明，整合 API 导航 | P128–P131；内容可提前随轮交付 | 页面清单完整、示例有效、翻译同步、现行事实和链接核验通过 | PASS |
| P133 | 接入 Read the Docs 双语言项目与 GitHub Pages 发布；线上检查 | P132 | 同一提交的四种站点/语言组合通过发布检查，版本和地址可追溯 | PLATFORM_PENDING |
| P134 | 独立终审、例外清单、维护规则与迭代收尾 | P133 | 所有必需项有证据；未解决项明确列出，不用本地成功替代线上成功 | PARTIAL |

执行顺序为 P126 → P127 → 各模块小轮 → P132 → P133 → P134。
P128–P130 可按不重叠文件并行；翻译、API 页面和检查随每轮同步推进。
P131 中不依赖模块改动的工具说明可提前进行。P127 的 API 翻译和 C++23
解析试点未通过前，不批量补注释，以免在工具链选择错误后重复劳动。

## 工作方式与交付边界

后续继续使用现有 `dev` checkout，不新建开发 worktree。每轮完成选定范围、
审阅和对应验收后，只提交该轮明确文件，再开始下一轮；保留无关工作区修改。
建议每个模块子轮控制在约 10–20 个文件以内，复杂 API 应进一步拆小；这是
审阅粒度建议，不是工时承诺，也不用于降低覆盖要求。

源码注释、文档工具配置、翻译/指南、部署配置分别组织提交。每轮记录范围、
验收结果、仓库/分支和实际提交哈希，不新建大批临时证据或复制可再生产物。
HTML、Doxygen XML、翻译中间文件和站点构建缓存不进入 Git 历史。

计划完成不等于迭代实施完成。未来只有源代码说明、双语内容、构建检查和两个
平台的实际发布均达到[验收要求](acceptance.md)，才能宣布整个迭代完成。
若平台账号或仓库设置尚未就绪，可交付已验证的可部署文档，但必须单列
“平台发布待完成”，不能标记 P133/P134 通过。
