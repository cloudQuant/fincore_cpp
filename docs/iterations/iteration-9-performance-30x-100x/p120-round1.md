# P120 R1：P124 全量正式批次未通过性能门槛

2026-10-25，`D:/source_code/fincore_cpp`、`dev`，固定 commit
`6417974c4c4e690739d1d5e92c279363854d9197`，Python 固定
`3e9ef205ef2023f92e89fddd1052d46b16429c64`。这是完整保留的首次最终全量批次，
结论为 **INCONCLUSIVE**；最终验收未完成。

```powershell
C:/Python314/python.exe benchmarks/alpha003_a_stocks/run_benchmark.py --max-symbols 0 --repeats 10 --cpu 0 --fincore-root D:/source_code/fincore --cpp-exe build-win/fincore_alpha003_research.exe --results-dir benchmarks/alpha003_a_stocks/results-i9-p120-6417974-full-b1
```

双方从进程创建起绑定 CPU 0，计算线程环境为 1；两对对称预热后保留全部十对
AB/BA 样本，每对完整比较均在下一次计时前结束。2 个预热对及 10 个计分对的
13 文件结果契约全部通过，最大绝对误差 `9.992007221626409e-16`，输入前后不变。

| 指标 | Python | C++ |
|---|---:|---:|
| 进程 wall 中位数 | 88.51045485 s | 3.90655065 s |
| IQR/中位数 | 4.8942% | 57.7604% |
| wall 最小/最大 | 86.2829 / 95.3687 s | 2.8388 / 7.8551 s |

中位数之比 **22.65693262×**；仅 **2/10** 对达到 30×。C++ 全部预热及计分
运行的峰值工作集最大值为 **239,022,080 B**，低于 1,358,388,838 B 预算。
内存与结果通过不抵消墙钟噪声或配对门槛不足。原始 `comparison.json` 的
`DEVELOPMENT_CAMPAIGN_ONLY` 字段未被重写为最终通过。

慢样本的主要变化位于加载：例如 run4/run5 的 load 为 2.731/2.665 s，前面
约为 0.56–0.66 s；它们的进程 CPU 仅为 2.766/2.688 s，wall 却为 5.303/4.789 s。
这定位了大量额外等待，但不能确定是存储、系统缓存、调度或其他因素。

Root 按进程命令行确认控制器 PID 15344，并观察到控制器峰值工作集约 9.925 GB。
源码比较器已按 100,000 行分块，返回值不保存整表。批次结束后，针对已有输出的
一次分表诊断和另一个连续三次比较的独立进程均全部通过，峰值约 1 GB，三次间
没有持续增长；显式 GC 也没有复现或解释原峰值。因此没有据此改动测量协议，
也没有把控制器内存直接认定为历史等待根因。

同期 D 盘可用约 8.71 GB / 666.71 GB（1.31%）；C/D 同属物理磁盘 0。该事实是
待排查的 I/O 条件，不是因果证明。已完成输出使用 NTFS 压缩释放空间，并逐文件
核对压缩前后路径、长度和 SHA-256；原始输入和未来输出目录不参与压缩。

原始证据位于 `benchmarks/alpha003_a_stocks/results-i9-p120-6417974-full-b1/`。
附加诊断位于 `build-win/p120-full-b1-{summary,controller-memory-observation,compare-memory-diagnostic,repeated-compare-memory}.json`。
下一动作是从已提交的小轮优化继续，并定位加载等待，随后完成有效的两批主场景、
Release/ASan、通用回归及规模诊断。不得删慢样本或重复挑选达标批次。
