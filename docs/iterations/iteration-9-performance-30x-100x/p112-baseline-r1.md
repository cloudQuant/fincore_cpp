# P112：完整输出基线与预算

2026-10-25，`dev` 测量起点 `dfe2806`；库 correctness reference 为 `3a2fe69`。
使用已冻结 `build-win/i9-p112-reference/fincore_alpha003_research.exe`，未重新编译。
Python 固定为 `3e9ef205ef2023f92e89fddd1052d46b16429c64`。

实际命令共两次：

```powershell
C:/Python314/python.exe benchmarks/alpha003_a_stocks/run_benchmark.py --max-symbols 500 --repeats 10 --cpu 0 --cpp-exe build-win/i9-p112-reference/fincore_alpha003_research.exe --results-dir benchmarks/alpha003_a_stocks/results-i9-p112-baseline-500-b1
C:/Python314/python.exe benchmarks/alpha003_a_stocks/run_benchmark.py --max-symbols 0 --repeats 10 --cpu 0 --cpp-exe build-win/i9-p112-reference/fincore_alpha003_research.exe --results-dir benchmarks/alpha003_a_stocks/results-i9-p112-baseline-full-b1
```

每场景 AB/BA 两对预热、十对计分，所有新进程从创建起绑定 CPU 0，线程限制、
实际 affinity、输入和二进制摘要前后验证通过。期间没有并行构建、测试或其他
性能 campaign。11 张表和公共 manifest 每对均在计时外完整比较；24 对
（含预热）全部通过，最大绝对差分别为 `4.025e-16`、`9.992e-16`。

| 场景 | Python 进程中位数 | C++ 进程中位数 | 比值 | Python / C++ IQR÷median | 测量结论 |
|---|---:|---:|---:|---:|---|
| 500 文件 | 9.260855 s | 0.888805 s | 10.4194× | 6.42% / 11.33% | INCONCLUSIVE：C++ 噪声超 10% |
| 全量 5,471 文件 | 78.081857 s | 22.936458 s | 3.4043× | 6.60% / 7.43% | 有效基线，未达 30× |

筛选后分别为 498 / 5,449 个资产、114,509 / 1,255,737 个 factor cells，
两边相同。全量 signals 为 1,136,571 行，labels 为 3,409,713 行，完整保留
5,297 个缺失标签。当前是完整结果 v2 路径，不能把旧的部分输出跑分作为
相同工作量对照。runner 的 `DEVELOPMENT_CAMPAIGN_ONLY` 字段原样保留：
它不自动宣告最终两批次的 30×/100× 验收。

C++ 阶段中位数（各项中位数不保证相加等于整体中位数）：

| 场景 | load | factor | research | export |
|---|---:|---:|---:|---:|
| 500 | 0.067450 s | 0.021524 s | 0.540045 s | 0.232973 s |
| 全量 | 0.815673 s | 0.361682 s | 17.175320 s | 3.097600 s |

诊断预算：500 的 30×/100× 分别要求 C++ ≤0.308695/0.092609 s；因该批有
噪声，只作路线参考。全量预算分别为 ≤2.602729/0.780819 s。全量原生进程
最高峰值工作集为 1,234,898,944 bytes，后续 110% 资源上限为
1,358,388,838 bytes。500 原生峰值为 117,383,168 bytes。完整阶段、噪声、
资源及原始四类记录摘要见 [紧凑数据记录](p112-baseline-r1.json)。原始目录保留。

## 下一轮及计划调整

源码与规模曲线共同指向原始值恢复中的逐行 `targets` 线性搜索及有序树映射。
P113 先用源行单调游标消除它们，保留完整 selector 校验、原始 Inf/NaN、
direction 与输出排序。P114 随后处理详细导出的日期重复格式化和临时字符串。

500 噪声的单一原因尚未确定：慢样本既有 CPU 时间增加，也有额外等待。没有
删除样本，也不通过反复重跑挑最好成绩。P112 的稳定 500 基线项仍待补齐；
已接受的正确性参考不受此影响，允许先做有针对性的代码优化。最终仍需两个
独立有效批次达到原门槛，不能拿此不确定批次宣告通过。

小轮沿用固定至少五对的 500 开发比较，并用一次全量运行核对完整 native
输出；单次全量只作为正确性/诊断，不宣称稳定提速。全量正式跨语言大批次
主要用于基线及最终验收。每轮有针对性的验收后立即 scoped commit，不等待
整个计划结束；这一调整减少重复开销，不降低最终功能、资源或速度要求。
