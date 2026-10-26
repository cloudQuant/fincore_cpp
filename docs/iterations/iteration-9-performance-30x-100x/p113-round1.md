# P113 R1：原始因子行映射

2026-10-25；仓库 fincore_cpp，分支 dev，父提交 `6603d29878208a5c0364ef3a8d939934d712cc40`。
本记录随本轮代码提交；用 `git log -- p113-round1.md` 可定位提交。

删除对每一原始行线性查找目标资产再构建树映射的工作，改为按选择器的稳定子序列顺序单调匹配原始行。
仍使用原始值保留 NaN/Inf，执行原有全输入重复键、目标和窗口校验，最后按日期/资产排序。
仅修改 detailed 实现及其单测，公开 owning API 和数值计算顺序不变。
独立只读审查未发现阻塞问题。

验收：`build-win/tests/fincore_tests.exe --gtest_filter=FactorResearchDetailed.*` 10/10；
`C:/Python314/python.exe -W error tools/check_research_detailed_oracle.py --probe build-win/fincore_research_detailed_oracle.exe`
三组完整 Python oracle 全通过。新增乱序/筛选/方向/非有限值映射和窗外重复键回归。

开发测量命令：`C:/Python314/python.exe benchmarks/alpha003_a_stocks/run_benchmark.py --max-symbols 500 --repeats 5 --cpu 0 --cpp-exe build-win/fincore_alpha003_research.exe --results-dir benchmarks/alpha003_a_stocks/results-i9-p113-500-r1`。
两对预热、五对 AB/BA，全部完整输出比较通过，最大绝对误差 4.0246e-16，输入不变。
Python/C++ 进程墙钟中位数 10.0357 / 0.9674 秒，约 10.37×；IQR/median 11.11% / 13.91%，故性能 INCONCLUSIVE。
C++ 研究阶段 0.5463 秒，500规模尚无可声明的稳定收益。

单次全量 CPU0 检查使用相同完整数据、end-offset=22、cross_section_rank；精确命令、二进制与源身份见 JSON。
全部11张CSV、manifest和workload与 P112 full/run1 逐字节相同，前后数据/二进制/源身份未变。
该次墙钟 10.5426 秒、CPU 8.6094 秒、峰值 1,089,630,208 字节；研究 6.5685 秒、导出 2.8824 秒。
P112全量中位数墙钟22.9365秒、研究17.1753秒，峰值最大1,234,898,944字节。
单次新运行与历史中位数仅作诊断，不代表正式配对速度提升，也不接受30×目标。

本轮功能及完整输出保持验收通过；500性能有噪声，全量性能待最终正式批次。
下一轮 P114 优化CSV字段追加及日期格式缓存；保留输出字节和完整工作量。
原始证据路径、散列、完整开发统计及全量资源见 [JSON](p113-round1.json)。
