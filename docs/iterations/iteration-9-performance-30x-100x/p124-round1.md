# P124 R1：组合研究与导出，省去大表 owning 行

2026-10-25，现有 dev，基点 `b6c9cb49f74993523a036c9ce45e956f27708e27`。
本记录和七个源/测试路径同次提交；提交身份见 Git 历史。

新增可安装库 API `evaluate_and_export_factor_research_detailed`，返回拥有其
status、manifest、diagnostics、行数和文件清单的独立 outcome。原 evaluator
仍返回完整 owning 行，原 standalone exporter 仍可独立使用。

组合路径共享原分析核心，signals 使用同步列视图，labels 按原 horizon/行顺序
写入同一个 CSV writer。虚调用边界按表/horizon，不按行；没有借用视图逸出。
分析需要的 panel/label 数组及全部计算保持，只省去数百万个中间结果行。
共享序列化、manifest 和清理逻辑，保留 NaN 行、精度、顺序、既有目录保护。
新接口遇到首个计算或写出错误就回滚自身文件；旧接口错误语义保持。

alpha runner 调用此库 API，明确记录 `research_and_table_export_seconds`
（含输出目录准备）与 `runner_export_seconds`。完整计时仍包含全部 13 个
约定文件、关闭与退出；不再把含 I/O 的组合阶段标成纯 research。

独立静态审阅 GO。Root Release 构建、74 项相关测试及 3 个 Python oracle
通过。新增测试覆盖 owning/组合路径 11 个详细文件逐字节一致、稀疏/NaN 标签、
非顺序 horizon、ties、默认/自定义分组、负零/无穷值、UTF-8/CSV 转义、返回元数据
拥有权、空统计表、晚期失败清理与目录保护。把当前测试过的 Release 库和依赖
安装到新目录并迁移后，独立 headless consumer 通过，包含新 API 的实际调用。

CPU0、单线程，两对预热加五对全量 native AB/BA：

| 指标 | 原 owning 路径 | 组合路径 |
|---|---:|---:|
| 进程 wall 中位数 | 3.390082 s | 2.941250 s |
| wall IQR/median | 4.854% | 2.668% |
| 进程 CPU 中位数 | 2.812500 s | 2.375000 s |
| CPU IQR/median | 4.444% | 4.605% |
| 峰值工作集中位数 | 748,244,992 B | 238,960,640 B |

完整 wall 减少约 13.24%，5/5 对新版更快，CPU 减少约 15.56%。14 次运行的
13 个完整文件均与 P112 字节相同，源/二进制/输入前后不变；所有样本保留。
组合路径最大观测峰值 239,321,088 B，低于既定 1,358,388,838 B 预算。
新组合阶段中位数 1.824580 s，runner 尾部导出 0.105098 s；不能将前者直接
与旧纯研究时间比较，按次相加后的阶段数据见 JSON。

500 资产两对预热及五对 Python/C++ 比较全部通过，最大数值差
`4.0245584642661925e-16`。wall 中位数 11.870896/0.288142 s，41.19809×，
5/5 对达到 30×；双方噪声 1.683%/4.482% 有效。

本轮接受实现、完整正确性与开发性能/资源证据。五对开发结果不能代替最终
两批各十对；全量对 Python 的正式门槛、完整 Release、sanitizer 与代表性库回归
仍待验收。下一步先从已提交候选检查全量正式差距，再安排最终其余门禁。
精确样本、环境、命令、源和原始记录身份见 [JSON](p124-round1.json)。
