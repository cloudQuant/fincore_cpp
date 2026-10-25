# 迭代 9：研究结论、架构与实施顺序

**版本：** R1，2026-10-25。以下性能收益均为待验证假设。
关联：[需求](requirements.md)、[验收](acceptance.md)。

## 1. 理论判断与历史数据

C++/Python 没有统一理论加速上限。Python 调用的 NumPy 内核本身使用编译代码，
不能按解释器循环估算所有数值运算。总收益取决于实际算法、指令、内存流量、
分配、序列化及 I/O；需要测量瓶颈，而非预先把 11–13 倍称为物理极限。
[NumPy 机制](https://numpy.org/doc/stable/user/whatisnumpy.html)。

历史全量三次分项中位数（秒）：

| 项目 | Python | C++ | 比值 |
|---|---:|---:|---:|
| load | 14.785343 | 0.665487 | 22.22× |
| factor | 1.094579 | 0.295668 | 3.70× |
| research | 23.086077 | 3.665139 | 6.30× |
| export | 28.157083 | 2.349715 | 11.98× |
| runner total | 66.484657 | 7.859718 | 8.46× |
| process wall | 67.398449 | 7.973192 | 8.45× |

分项中位数不能直接相加，且两个 runner 的 total 边界不同。全量 C++ run3
单次四项和为 6.9831332 s、total 为 7.8597177 s，未单列 0.8765845 s。
三次 C++ total 为 6.804/11.557/7.860 s；Python 为 66.485/65.724/92.959 s，
现有波动不支持精确预测小幅优化。历史报告只比较 run1 数值，不能证明每次输出。

C++ 使用 MSVC `std::clock()` 写 cpu_seconds，它实际返回墙钟时间；用
`GetProcessTimes` 的 user+kernel 替代，并通过 affinity/线程限制验证资源。
[微软 clock 说明](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/clock?view=msvc-170)。
固定先 Python 后 C++ 会造成文件缓存顺序偏差。两端还存在 Pearson IC、
多周期输出、NaN 标签、日期文本等工作量差异；full run3 CSV 总量约 398/362 MB。
这些是必须先修复的基线问题，不是可以计作算法提速的优化。

按历史 run3，factor 仅约占 total 的 3.85%，即使将其耗时降至零，整体也只再快
约 4%。research/export 各快 2 倍时粗估总计仍约 4.85 s、相对历史 Python 约
13.7 倍，达不到新最低目标。30 倍需要同时压缩研究、导出、加载和准备成本；
100 倍需要进一步架构收益，不能将各局部加速比相乘当作整体验证。

输出约 350 MB 或当前 load+factor 约 0.96 s 不能证明 50/100 倍“物理不可能”。
同样，没有实测也不能保证可达。用同机纯格式化、缓冲写、解析和内存遍历实验
估计可实现的资源下界/当前瓶颈，记录距离 30/100 倍还差多少。

## 2. 实际路径和候选改进

本 benchmark 的 `next_open_to_open` 实际路径是：

`research_panel -> next_open_table -> evaluate_with_table -> assemble_research -> build_signals`

`prepare_factor_data_by_horizon` 的 close-to-close 分支和完整 `analyze_factor_impl`
不是本 benchmark 的执行路径，不能把改它们的收益计入 alpha003。

| 源码入口 | 当前事实 | 改进与语义边界 |
|---|---|---|
| [research.cpp](../../../src/factor/research.cpp)，`assemble_research` | labels 填充后才 reserve | 提前统计/预留；只接受实测收益，不预估倍数 |
| 同文件，`build_signals` | 日期/资产 map 按约 125 万行 reserve；scatter 到 dense 网格再读取 | 按唯一轴数分配；传递显式 source-row index，保留 raw/processed、direction、universe 顺序 |
| 同文件，`evaluate_with_table` | 完整笛卡尔表上逐单元 hash 检查唯一键；每 horizon 复制 labeled_panel | 轴级唯一性验证、一次建立 row mapping；索引视图替代整表复制；不取消输入校验 |
| [data.hpp](../../../include/fincore/factor/data.hpp)，`factor_row` | 每个单 horizon 行仍含字符串和单元素 vector | 内部 SoA/整数 ID/连续收益矩阵，公开 API 适配成本仍纳入完整计时 |
| [performance.cpp](../../../src/factor/performance.cpp)，`average_ranks`/IC | 多次构建分桶、排序与 rank 缓冲 | 共享日期 offsets、排序工作区和 factor 顺序；按每周期有效掩码重新编号 |
| 同文件，`*_many` | 稳定性多个 lag、turnover 多分位已复用部分准备 | 保留已有复用，避免再实现一套同样的缓存 |
| research exporter | 已有 1 MiB buffer、日期缓存、to_chars；仍逐行 vector<string>/临时 string | typed append-row、string_view、复用缓冲，保持转义、缺失字段、浮点格式和错误清理 |
| [alpha003 runner](../../../benchmarks/alpha003_a_stocks/cpp/alpha003_research.cpp) | 已整文件解析、复用排序空间；保存两张完整 rank 面板 | 测解析/文件打开开销；rank 环形缓冲或分块融合；保留近常量稳健相关算法 |
| [rolling_engine.cpp](../../../src/rolling_engine.cpp) | 每窗口构建 dataframe，多指标重复遍历 | span 内核、一次对齐、共享滚动矩；适用指标 O(NW)→O(N)，回撤另用专门算法 |

所有行目前只有源码依据；还没有这些修改的独立采样、硬件计数器或收益证据。
短字符串可能使用 SSO，不能把每个 string 都算成一次堆分配；单元素 vector、
重复拷贝和容器工作集仍应实测。

## 3. 目标架构

内部研究上下文一次建立：日期/资产字典、source-row index、日期 offsets、
有效掩码、各 horizon 收益数组以及可复用排序缓冲。共享 immutable 输入，
统计内核消费 span/索引视图；按 horizon 缺失集合形成各自的秩和分位结果。
稀疏输入用显式行索引，不强制构造所有 date×asset 单元。

将热路径数值计算与公开结果/文本表示分开：公开接口保持拥有数据的结果；
若引入列式结果或迭代导出入口，应是正式可复用 API，并承担相同的计算和输出
义务。旧 API 也要回归；完整成绩不得将适配/必要物化移到计时之后。

导出器直接将类型化列追加到输出块，数值只格式化一次、日期按唯一值缓存。
无需逐行生成 vector<string>；保持文件关闭、失败清理和完整输出行为。
构造参考 adapter 做差分验证后再决定保留何种公共入口，不能让 benchmark
独占未经库验收的特例。

## 4. P111–P120 任务与检查点

每一 phase 可拆成数个小提交；表中状态全部为 `NOT_RUN`。

| Phase | 范围、产物 | 验收/下一步 |
|---|---|---|
| P111 | 修复 CPU/墙钟计时；补 preparation/materialization/cleanup 分项；固定线程/affinity；交替顺序；环境/数据/二进制记录；采样热点与分配；实现版本化回归判定 | 有真实 process CPU 和对称实验记录；阶段边界闭合；通用benchmark的5%配对区间门禁按acceptance实现并用已知样本验证；不做性能 PASS 声明 |
| P112 | 对齐完整表义务和比较器；补 Python已有/C++缺失的指标；每轮完整比较；冻结 Python、C++ correctness reference 与 schema | 数值/键/缺失/错误语义通过；冻结正式基线及新时间预算；这轮必要功能成本不冒充优化回归 |
| P113 | labels reserve 前移、按唯一数预留、source-row mapping、轴唯一性验证；消除重复 hash/拷贝 | 针对性回归、500/全量配对结果、内存/分配变化；验收并提交后继续 |
| P114 | labels/signals 优先的直接缓冲序列化，再推广其他表；分析格式化与系统写入占比 | 完整导出 parity、字节/行数、失败路径；完整流程与 exporter 分项收益均报告 |
| P115 | 内部紧凑 SoA、整数 ID、horizon 收益连续存储；消除单元素 vector/重复字符串和 labeled_panel 深拷贝 | owning/生命周期/稀疏回归、峰值内存和真实流水线收益；避免 API-only 快路径 |
| P116 | 日期/有效掩码/秩顺序复用、IC/分位统计融合、稳定性和 turnover 准备共享 | ties、horizon 缺失、退化截面、方向与分位边界；核对排序/分配次数下降 |
| P117 | 加载、布局转换与 alpha003 融合；按profile选择解析、环形缓存、批处理或SIMD；同时检查启动/销毁 | 同原始CSV的完整计时；近常量相关与坏输入回归；不把预转换二进制替代原数据成绩 |
| P118 | 代表性库路径，优先 rolling_engine/rolling metrics 的重复切片与滑动状态 | 长窗口、多指标、NaN和漂移回归；八类通用 benchmark 无显著回归，单独报告收益 |
| P119 | 在热点稳定后 A/B 验证 LTO/PGO/向量化和分配策略；训练集与最终验收数据分离 | 相同语义和单核预算；保留通用CPU构建与fallback；只合入可重复收益 |
| P120 | 独立最终配对测量、完整回归和安装消费者；检查 30×门槛与100×差距；必要时增补下一轮结构优化 | 两主场景各自判定；未达30×则修订路线继续，不能以计划phase用尽为完成 |
| P121（R4修订） | 依当前粗阶段热点，对大截面使用稳定binary64基数排序；先合并直方图扫描，保留小截面比较排序和原有秩/数值运算 | 极值、正负零、ties、阈值及scratch复用回归；完整字节、500/full新旧native配对；后续优先标签索引和结果物化 |

默认依赖 P111→P112→P113→P114→P115→P116→P117→P119→P120。
P118 在基线冻结后可与不共享源文件的工作并行；最终仍纳入 P120。
同一源文件保持一个实现编辑者；独立审阅可并行。某项已由现有代码满足时，
记录证据并跳过重复实现，而不是重复执行已接受 QA。

## 5. 预算、可达性和改进路线

正式预算以 P112 的固定 Python 基线计算：500/full 分别 `T_py/30` 和 `T_py/100`。
每轮同时报告当前 C++ 对上一提交、对 correctness reference、对 Python 的比值。
先按 profile 给各阶段分配秒数预算，再实现，不预承诺“排序减少30%”等无测量数字。

100 倍探索必须覆盖完整路径：更少中间物化、跨指标融合、零逐行分配导出、
解析/格式化微基准、启动与析构开销。以现有全量约0.674 s目标看，当前load
就接近整个预算，不能只优化research。用真实吞吐和剩余工作计算差距，
将有证据的瓶颈转成下一phase；不要直接用“C++应更快”解释不达标。

若连续两轮收益落在噪声内或累计收益不足以覆盖剩余预算，暂停同类微调，
复核热点、工作量和数据布局，修订设计再开发。除非用户另行改变目标，
减少表、仅摘要/hash、GPU或给C++更多核均不能替代当前主验收；它们只能
作为有独立名称、同预算比较的补充实验。

## 6. 已知风险与验证

- 排序复用：相同因子值在不同有效集合中的秩不同，用缺失/ties回归防止重犯。
- 滑动统计：简单平方和可能发生灾难性消减；用稳定更新、必要的周期重算和
  oracle验证，不直接用快速但不稳健的公式替换当前相关系数。
- 布局：避免破坏公开拥有权、生命周期和稀疏规模；兼容适配成本计入。
- 100倍：小规模启动和大规模文本输出可能主导，但在测得下界前不写物理结论。
- 编译：现有 `/O2` 已启用；新选项单独 A/B，训练数据不能作为唯一验收数据。
  [MSVC PGO 文档](https://learn.microsoft.com/en-us/cpp/build/profile-guided-optimizations?view=msvc-170)。
