# 待审阅问题

**盘点日期：** 2026-10-25。

**源码基线：** `dev`，`8a1174fe5a08fd524f6728536fc0b34c55d59c27`。

**处理状态：** F10-D01–D04 与 F10-G01 已在 2026-10-26 实施轮处理（见下方新增
条目与[验收记录](../../acceptance/2026-10-26-iteration-10-comments-bilingual-docs.md)）；
本轮新登记的 F10-D05–D09 已随注释轮修正并记录依据。审阅前不自动给疑点定性
为缺陷，记录不等于修复授权。

本次只读查看接口、现有文档和构建入口，没有运行业务程序或性能基准。
已核实的问题集中于文档现状，不构成发现了运行时 bug，也不能据有限抽查
声称项目没有 bug。下面行号对应上述基线，不作为永久定位；路径与原文内容
共同用于后续复核。

## F10-D01 当前指南中的运行时目录数量过时

- **分类：** 已核实的文档不一致，建议优先级高。
- **位置：** [native-usage.md](../../guides/native-usage.md) 第 82 行，
  仍写 `builtin_catalog()` 含 48 个 source operation IDs。
- **证据：** [迭代 8 注册记录](../../evidence/iteration-8/p105/operation-registration.txt)
  记录 270 个注册、270 个唯一 ID；
  [ID 清单](../../evidence/iteration-8/p105/registered-operation-ids.txt)有 270 个唯一条目；
  [验收记录](../../acceptance/2026-10-24-iteration-8-native-parity-acceptance.md)
  第 59–61 行同样记为 270。
- **影响：** 当前迁移指南会低估已注册范围，并与现行能力介绍冲突。
- **建议：** 审阅后按同提交的现行注册清单更新用户指南，未来生成/校验数量。
  根 README 中 Phase 93 的 48 是历史阶段信息，保留其日期含义，不全局替换数字。
- **待决事项：** 选择现行能力表的唯一来源；本次未执行 `builtin_catalog()`，
  不能把文档/清单核对表述成一次新的运行时验收。

## F10-D02 用户入口引用不存在的文档

- **分类：** 已核实的本地链接缺失，建议优先级高。
- **位置：** [native-usage.md](../../guides/native-usage.md) 第 4–5、121 行；
  [根 README](../../../README.md) 第 3496、3499 行附近。
- **缺失目标：** `2026-10-23-revised-plan-candidate-acceptance.md`、
  `2026-10-23-revised-plan-scope-decisions.md` 和 `docs/iterations/capability-ledger.tsv`。
  此处故意以代码文本记下失效名称，不新增指向不存在文件的链接。
- **影响：** 读者无法通过安装/迁移入口找到文中所称的验收和能力依据。
- **建议：** 审阅后逐项选择真实存在且语义匹配的现行记录，不机械重定向到
  内容不同的历史文件。优先修新站和当前用户入口；缺失历史档案如无法恢复，
  用额外说明记录缺失状态，不虚构原内容。

## F10-D03 历史链接包含机器路径和可再生产物

- **分类：** 已核实的发布适配问题，建议优先级中。
- **范围证据：** 对根 README 和 `docs/` Markdown 的本地行内链接静态扫描，
  在 417 处检查目标中发现 64 处目标缺失：44 处文档目标、18 处机器本地
  `/tmp` 日志、2 处被忽略的 benchmark 生成物。计数按链接出现位置统计，
  不含完整的锚点、参考式链接和外网可达性检查，不能视为全站链接审计。
- **具体入口：** [迭代 9 README](../iteration-9-performance-30x-100x/README.md)
  的证据起点仍链接 `results/comparison.json` 与 `results-full/comparison.json`；
  文中也说明这些结果目录被忽略，干净克隆不保证存在。
- **影响：** 如果直接把整个旧 `docs/` 发布，新站会包含读者不可访问的引用。
- **建议：** 新站使用已核验的摘要和复现入口，只白名单发布产品内容。
  历史冻结记录保持原样，必要时在档案入口补充说明；不重新提交大型生成结果
  来“修复”链接，也不删除历史事实。P126 执行完整链接清单并逐项分派。

## F10-D04 ImGui 依赖版本说明与当前 gitlink 不同

- **分类：** 已核实的版本引用差异，兼容性影响待审阅，建议优先级中。
- **位置：** [native-usage.md](../../guides/native-usage.md) 第 36–37 行。
- **证据：** 指南写 ImGui pin 为 `46d39d56febc2a00bdd2270dc88c8a13f2a0441a`；
  本基线 `git ls-files -s -- third_party/imgui` 的 gitlink 为
  `4e99c0ab1a28c2b3d1f250ec0266dbf1e455ef58`。
  [CMakeLists.txt](../../../CMakeLists.txt) 默认使用 `third_party/imgui`。
- **影响：** 按指南单独准备依赖与递归克隆仓库会得到不同版本；本次未构建，
  不据此判断哪个版本不兼容，也不宣称安装失败已复现。
- **建议：** 审阅当前可选组件验收对应的版本后更新现行安装说明，明确历史
  版本的适用范围；本轮不改子模块 gitlink，不升级或降级依赖。

## F10-G01 文档与注释设施缺口

这是规划需求，不是软件 bug。公共注释形式不统一；部分丰富说明使用普通
`//`，不会自动成为 API 参考。仓库内尚未发现 Doxygen/Sphinx/RTD/Pages
配置，当前 [.github/workflows/ci.yml](../../../.github/workflows/ci.yml)仅提供
业务构建和测试；现有中英文材料也未形成同范围的两种语言站点。
P126/P127 处理清单和试点，不以批量添加空注释或生成英文菜单来宣称补齐。

## 后续疑似 bug 的登记与审阅

新增项使用 `F10-Bxx` 标识行为疑点，`F10-Dxx` 标识文档问题；状态依次记录
“待审阅 → 已确认/非缺陷/需更多证据 → 独立修复已安排 → 已验证关闭”。
审阅前不自动给疑点定性为缺陷，记录不等于修复授权。

每项至少包含：定位文件和符号/行号、基线提交、实际观察、预期行为的依据、
最小复现说明、影响范围、证据等级、建议处理、审阅人/结论及后续任务。
证据等级区分源码推断、已有测试/记录、当前实际复现；不能相互替代。
本次不为复现疑点写测试代码或修改被审查实现，后续复现也须按单独授权范围执行。

审阅确认只是文档错误时，在文档轮中更正并引用审阅结论；确认是业务缺陷时，
创建独立修复任务，定义必要的失败复现与回归验收，再由后续负责人实现和提交。
注释作者不得顺手改变逻辑、放宽输入验证、改变数值容差或更新输出基准来消除失败。
在结论未定前，对外说明只写可证实行为，并明确限制；影响必需示例或核心
契约的疑点阻止对应页面验收。

## 实施轮发现的文档缺陷（2026-10-26）

以下均为文档/工具缺陷，不含运行时行为改动。证据等级统一为"当前实际复现"
（严格 Sphinx 构建告警或词法比对），处理均限定在注释与文档配置范围内。

### F10-D05 C++ 注释中残留 Python 交叉引用角色

- **位置：** `attribution/brinson.hpp`（`:func:brinson_results`）、
  `attribution/fama_french.hpp`（`:func:fetch_ff_factors`）、
  `attribution/fama_french_patch.hpp`（`:class:FamaFrenchModel`）、
  `risk/report.hpp`（`:struct:risk_validation_report`）。
- **观察：** 这些 Python/Sphinx 角色在 C++ API 页面上原样显示，读者看到的
  不是链接也不是代码。
- **处理：** 改为 Doxygen 代码跨度（双反引号），英文与中文页面同轮更新。

### F10-D06 Doxygen 数字分隔符导致签名无法解析

- **观察：** `portfolio/transactions.hpp` 默认参数 `1'000'000.0` 被 Doxygen
  写成 `1 '000 '000.0`，Sphinx C++ 域报
  `Expecting "," or ")" in parameters-and-qualifiers, got "'"`，
  `days_to_liquidate_positions()` 与
  `get_max_days_to_liquidate_by_ticker()` 两个接口无法出现在 API 参考中。
- **处理：** 新增 `docs/site/tools/normalize_doxygen_xml.py`，只重写生成 XML
  的 `<argsstring>`，接入 RTD `pre_build` 与 Pages 工作流；不改源码常量写法。

### F10-D07 行首 `100.` 被 Doxygen 解析为有序列表

- **位置：** `portfolio/transactions.hpp` 中
  `get_max_days_to_liquidate_by_ticker` 的详述。
- **观察：** 提取出的正文变成 `... multiplied bySymbols with no valid value`，
  `100.` 被当作列表项消失，读者无法看到"乘以 100 得百分比"这一单位说明。
- **处理：** 改为 `as a percentage (100 * fraction)` 的行内写法，语义不变。

### F10-D08 printf 说明被 MyST 当作链接

- **位置：** `viz/bokeh_document.hpp` 的 `printf_tick_formatter` 详述。
- **观察：** `` `%[flags][width][.precision](f|e|g|d|i)` `` 被解析成
  Markdown 链接，构建报
  `'myst' cross-reference target not found: 'f|e|g|d|i'`。
- **处理：** 改写为破折号加列举形式，避免方括号紧跟圆括号。

### F10-D09 符号清单与页面生成器的两处口径缺陷

- **位置：** `docs/site/tools/build_api_ledger.py`、
  `docs/site/tools/generate_api_pages.py`（本轮新增）。
- **观察：** 根 `fincore` 命名空间下的枚举与自由函数在清单里丢失
  `fincore::` 前缀，重载集合被折叠成一行；因此生成的 API 页面一度完全漏掉
  28 个枚举与全部自由函数（Sphinx 随之报 60+ 未解析标识符），
  `doxygenfunction` 也因缺少签名而无法解析重载。
- **处理：** 清单改为全限定名并逐签名记录重载（2408 → 2484 行），页面生成器
  为重载输出签名、模板函数按名称解析；`__LINE__` 之外的非注释 token 未变，
  由 comment-only 校验与 1092 项测试回归确认。
