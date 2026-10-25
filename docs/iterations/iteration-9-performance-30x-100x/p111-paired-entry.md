# P111：通用配对入口验收

2026-10-25，现有 `dev`，起点 `eb71df208c86103bbf5d1a62a95c955a30854713`。
本轮交付 `tools/run_paired_benchmarks.py` 及其测试，承接已提交的
[Windows native 与 rolling 参考](p111-generic-pairs.md)。

入口严格验证每个 case 的阶段集合、完整输出和非诊断业务字段；按 AB/BA
顺序在同一逻辑 CPU 创建新进程，比较在进程计时结束后完成。正式子集只可能
得到 `CASE_SUBSET_PASS`，不能冒充完整八类通过。分配追踪必须关闭。
rolling-engine 独立项使用 compute 中位数；其余八类使用进程 wall。
全部使用同一 5% 配对 bootstrap 与 10% 噪声门禁。细节及修订理由见上文。

验证：

- 18 项 mock/synthetic Python 单元测试通过，命令
  `C:/Python314/python.exe -m unittest tools.tests.test_paired_benchmarks`。
  初版语法错误及 A/B 标签直接索引 reference/candidate 的失败已修复；
  后者由完整调度测试发现，原始失败日志保留在 `build-win/p111-paired-unit-r2.log`。
- Root 实际同二进制 scalar-sharpe 开发检查：两对预热加五对测量，14 个
  子进程均通过，全部 affinity mask=1 且验证为 true，业务结果一致，二进制
  原件和归档 SHA-256 前后相同。结果
  `build-win/p111-paired-live-r1/campaign.json`；状态 `DEVELOPMENT_ONLY`，
  `generic_eight_status=NOT_RUN_COMPLETE_SET`。不宣称性能提升或正式回归通过。
- 已有 rolling-engine 50,000 行及 scalar 实际 JSON 均通过新入口的 schema
  校验；未为交付重复运行它们。

两类 C++ 参考已保存在 `build-win/i9-p112-reference/`，该目录不是 worktree。
alpha003 参考 SHA-256 为
`d80e8b604eca5906c35a0fda3e2a245edc35497b294a725656949997b2751e35`；
generic 参考为
`1e5668766ba76821bb132e849bb8833872ba1aed8e0463e0bf3fdc55a5f84f64`。
两者库实现仍为 P112 `3a2fe69`；generic 测量外壳为 `eb71df2`。

下一动作是固定 CPU 0 的 alpha003 500/full 十对完整输出基线，不再扩展基准框架。
