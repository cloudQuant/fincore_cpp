# P118 R1：消除滚动窗口临时 DataFrame

2026-10-25，现有 dev，起点 `39d5897f1cceb41e9c302f33efc8a33b0c6cd3a4`。

`rolling_engine` 直接用原列 span/subspan 读取窗口，移除每窗口索引、值、
DataFrame 及标量函数中间数组的复制。Beta 保留原始完整索引对齐一次，移除
已对齐窗口的重复对齐。没有引入增量求和；NaN/Inf 筛选、运算顺序、重复右键
取首项、左侧顺序和窗口末端索引不变，公开结果仍为 owning DataFrame。

Root Release 构建成功，6 项 RollingEngine 单元测试通过，包括长窗口、稀疏
benchmark、乱序/重复索引及 NaN/±Inf 样本对标原标量函数。独立静态审阅未发现
阻塞，审阅者未运行测试。本轮不涉及 alpha003，未重复其已接受的完整输出 QA。

Root 在 CPU 0（实际 mask=1）执行冻结 P112 generic reference 与候选的交替
对照：2 对预热、5 对测量，每次进程内计算 7 次、无内层预热；50,000 行、
benchmark 49,485 行、窗口 252、五项指标。14 次运行全部完整输出核验通过，
不只比较 digest；源文件、diff 与二进制前后不变。候选身份是上述 base 加
JSON 中固定的两文件 overlay，不能把 base 本身误称为候选已提交代码。

| 指标 | 旧 C++ | 新 C++ |
|---|---:|---:|
| compute 中位数（ms，本项计分口径） | 1856.1374 | 176.333 |
| compute IQR/中位数 | 5.14% | 2.98% |
| 完整进程 wall 中位数（s） | 17.152758 | 5.179937 |
| 完整进程 CPU 中位数（s） | 15.109375 | 4.578125 |
| 峰值工作集最大值（bytes） | 64917504 | 64172032 |

计算中位数约快 **10.53×**，进程 wall 约快 **3.31×**；这是新旧 C++ 比较，
不是 C++/Python 比值，也不计入 alpha003 的 30×主门槛。状态为
`DEVELOPMENT_ONLY`；至少十对的正式回归和完整八类路径仍留待 P120。

命令：构建 `cmake --build build-win --target fincore_tests fincore_factor_benchmark -j 4`；
测试 `build-win/tests/fincore_tests.exe --gtest_filter=RollingEngine.*`；
配对入口 `C:/Python314/python.exe build-win/run_p118_r1.py`，复用已提交的
`tools/run_paired_benchmarks.py`。完整输入/命令/输出在
`build-win/p118-r1-paired/campaign.json`，精简身份和结果见 [JSON](p118-round1.json)。

本轮仅提交两项源/测试修改及本记录、JSON、验收表。下一轮回到主流程标签构造，
尝试避免先生成全价格网格的 forward 矩阵再映射回选中 panel。
