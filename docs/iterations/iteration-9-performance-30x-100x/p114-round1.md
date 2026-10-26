# P114 R1：类型化 CSV 追加和日期缓存

2026-10-25；fincore_cpp 仓库现有 dev，父提交 `4f36494d73b40cc6aa3f69c56ed781e0dba890eb`。
本记录与两文件代码同次提交，`git log -- p114-round1.md` 可定位。

仅修改 detailed exporter 和相关单测。数值通过栈上 to_chars 直接追加，文本即时转义，
连续相同的完整纳秒时间戳复用上一次文本。缓存视图不逃逸；schema、行序、UTF-8、LF、
NaN/Inf/负零、shortest-general 浮点、1 MiB缓冲、manifest和失败清理契约不变。
独立静态审阅未发现生产阻塞；审阅指出的新测试期望问题按下述证据修正。

`FactorResearchDetailedExport.*` 首次8通过/1失败：新负epoch测试把非零1.0的期望写成了1.0，
旧版与新版契约均输出1。仅修正6行测试期望，再运行该失败测试，1/1通过；生产实现未改。
既有7项测试及两项新测试均有通过证据，包括日期负值/ns/重复/乱序、整数极值、
浮点最大值/次正规数、零、UTF-8与失败清理。
实际 `tools/check_research_detailed_oracle.py --probe build-win/fincore_research_detailed_oracle.exe`
三组Python参考全通过；冻结P112探针与候选探针的三组33个CSV/manifest文件全部字节相同。

测量仍CPU0、两对预热+五对AB/BA，命令与P113相同，仅结果目录为
`benchmarks/alpha003_a_stocks/results-i9-p114-500-r1`。全部完整输出通过，最大差4.0246e-16。
Python/C++墙钟中位数10.2960/0.67766秒，15.19×；C++ IQR/median18.40%，性能INCONCLUSIVE。
C++导出中位数0.08224秒（P113开发批次0.27259秒），研究0.47268秒；不同批次仅诊断比较。

一次全量运行全部11表、manifest和workload与冻结P112 full/run1逐字节相同，输入/源/二进制前后不变。
墙钟8.44282秒，CPU7.09375秒，峰值1,090,465,792字节；研究6.50791、导出0.83324、
加载0.62806、因子0.32362秒。P113单次全量墙钟10.5426、导出2.8824秒；
P112全量基线中位数22.9365秒。单次全量仍不构成正式加速或30×验收。

功能及完整输出保持验收通过。接下来P115替换通用horizon中间行及重复分桶，
研究阶段现在约占该次全量内部墙钟78%，优先处理分配和中间容器。
原始证据路径/散列、源及二进制身份和详细统计见 [JSON](p114-round1.json)。
