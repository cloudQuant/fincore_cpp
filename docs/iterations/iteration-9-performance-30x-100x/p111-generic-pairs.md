# P111：Windows 通用配对基准与 rolling engine 参考

2026-10-25，现有 `dev`，起点 `c260003846920da75f64b766a5c9552d4ba1bd65`。
本轮验收 Windows native benchmark、rolling 参考和旧 runner 的分配计数兼容。
新的配对 Python 入口仍在独立验证，暂不纳入此提交。任何一项都不代表性能优化
或 30 倍验收。

## 修改范围

通用 C++ benchmark 支持 Windows 的峰值工作集、普通/对齐分配与编译器信息。
默认关闭全局分配计数；`--allocation-profile` 单独启用，计分运行不带此参数。
关闭时分配字段为 null，不把未测量写成零。旧 Python 比较入口识别该状态，
并拒绝把启用和关闭追踪的结果混合比较。

后续独立交付的 `tools/run_paired_benchmarks.py` 将复用 P111 的进程创建前单核绑定、
线程限制和真实 CPU/工作集测量。每 case 先 AB/BA 两对预热，再交替运行
reference/candidate；每次进程退出后验证完整结果，失败即停止。保留原始 JSON、
二进制副本和摘要、顺序与资源数据。五对是开发诊断；正式至少十对，使用
既定 10,000 次配对 bootstrap、seed 92026、5% 回归阈值和 10% 噪声阈值。
只运行子集不能获得完整 generic-eight 的 PASS。

新增独立 rolling-engine case：50,000 行、252 窗口、五项指标，包含 NaN 和
稀疏 benchmark。导出各指标完整时间索引与值，检查缺失掩码和参考计算。
第一版回撤 oracle 错用了另一 rolling API：该 API 对 NaN 的语义不同于
engine 调用的 scalar max_drawdown。保留失败记录；改用独立财富序列计算，
按既有 scalar 语义把 NaN 收益视为零，未改生产代码或容差。

## 测量范围修订

八类通用 case 继续使用外部完整进程墙钟时间。rolling-engine 是独立的计算
性能项，使用每个新进程中七次 `compute` 原始样本的中位数作配对样本，再按
相同回归规则判定；同时保留外部 wall/CPU/RSS 作诊断。原因是其独立 oracle
和完整序列序列化在 compute 计时外、进程退出前完成，把它们算入计算收益会
掩盖退步。alpha003 的 500/full 主门槛仍使用启动至退出的完整进程耗时。

## 验证记录

- MSVC Release 构建 `fincore_factor_benchmark`、`fincore_tests` 成功：
  `build-win/p111-generic-build-r2.log`。
- 新增长窗口/NaN/稀疏对齐测试 1/1 通过：
  `build-win/p111-rolling-oracle-tests-r2.json`。首个错误 oracle 的失败输出保留。
- scalar 实际 CLI 的开/关分配追踪输出业务结果一致；关闭为 null、开启记录
  真实计数，两次均有正的 Windows RSS：`build-win/p111-native-scalar-*.json`。
- rolling-engine 实际 CLI 检查全部通过：`build-win/p111-rolling-smoke-r1.json`。
  四个指标各 49,749 行，beta 49,234 行；所有索引、缺失掩码、数值及重复输出
  检查为 true。这是正确性检查，不是单核正式性能结果。
- 旧 runner 的 16 项 Python 测试通过；新配对入口的单元与实际启动检查尚未通过，
  不纳入本轮已接受范围。

下一步：冻结两类 C++ correctness reference，测量 500/full 完整输出正式基线，
据阶段耗时确定首轮生产优化预算。以上原始输出在 ignored build 目录保留；
不为交付重跑已通过的测试。
