# P115 R1：紧凑 horizon 与分位 reducer

2026-10-25；fincore_cpp 现有 dev，父提交 `4845d7dcaa3bee3963d4d9f12f882791956732d1`。
本记录与本轮两文件代码同次提交，`git log -- p115-round1.md` 可定位。

每周期中间数据改为原面板行索引、分位标签、连续日期区间和loss统计；
移除逐行通用factor_row及字符串/单元素forward vector复制。IC每日期只收集一次输入；
分位均值/计数直接以原行序累计，替代通用函数的多层桶、未消费标准误及重复lookup树。
公开owning结果、qcut、loss/异常、诊断、Pearson算术、分位输出顺序保持。
Turnover仅适配行索引，map/set算法及按q压缩日期轴仍保留；stability路径未改。

独立静态审查无生产阻塞。审查曾怀疑日期×分位工作区引入稀疏轴资源回归，
追到旧函数的同维度means/standard_errors和旧daily_lookup后撤回；该笛卡尔积维度是既存行为。

首次构建指出新测试六处date/time字段笔误，改正测试后成功。11项core测试首次10通过/1失败，
原因是新测试误断言q2所有日期均无turnover。实际运行冻结P112和候选探针对该稀疏fixture的11个文件
逐字节相同：q2压缩日期轴为start与start+3day，应输出后一日turnover0。
测试改为精确断言一行、该日期和值0后，重测该用例通过；生产代码无需修正。
`tools/check_research_detailed_oracle.py --probe build-win/fincore_research_detailed_oracle.exe`
实际三组Python参考全部通过，覆盖所有十张研究表。

五对500开发测量：CPU0、两对预热、AB/BA，完整11表与manifest每对通过；最大差4.0246e-16。
结果目录 `benchmarks/alpha003_a_stocks/results-i9-p115-r1-500`；命令沿用P113，repeats=5。
Python/C++进程墙钟中位数9.63432/0.53694秒，17.94×；IQR/median20.53%/12.38%，
性能INCONCLUSIVE。C++研究中位数0.28237秒（P114为0.47268秒），导出0.08611秒。

一次全量运行全部11表、manifest、workload与冻结P112 full/run1字节相同，数据/源/二进制前后不变。
墙钟6.06856秒、CPU5.46875秒、峰值1,031,823,360字节；研究4.18752秒、导出0.79191秒、
加载0.64136秒、因子0.29964秒。P114全量单次为8.44282秒/研究6.50791秒，
P112全量基线中位数22.93646秒。不同轮单次比较仅诊断，不是正式配对性能接受。

本轮功能及完整字节保持验收通过；30×仍未达标。下一轮去掉turnover的重复树/集合，
以及stability通用行物化和重复rank/dense grid，保持既有缺日/NaN/Inf/求和顺序。
精确命令、源/二进制身份、原始路径及散列见 [JSON](p115-round1.json)。
