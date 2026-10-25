# P113 R5：按选中 panel 直接生成 next-open 标签

2026-10-25，现有 dev，起点 `256e953c702e9f914dd53920d027fe4879dbcd6f`。

Detailed 的 next-open 分支只为选中 panel 生成每 horizon 的 owning 标签数组，
省去全资产×全日期的 forward 中间矩阵、日期/资产副本及再映射。仅保留按选中
资产的非拥有列指针表及 grid 日期索引；不增加逐 panel 行的 O(N) 指针缓存。
公开输出行序、公式、非有限值转 NaN、缺日期和尾部语义不变；私有 helper 仍
支持乱序 panel。早期缺列错误和 canonical period/grid 检查顺序不变，旧版
`next_open_table` 与 close-to-close 分支未改。

Root Release 构建成功；51 项相关单测和 3 个实际 Python oracle 场景通过。
新回归覆盖选中资产子集、额外价格列、乱序/稀疏 panel、缺 grid 日期、尾部、
多 horizon、Inf/NaN/零分母。独立静态复核未发现阻塞，审阅者未运行。

CPU 0 开发测量（2 对预热、5 对测量）：所有完整结果 PASS，最大数值差
`4.0245584642661925e-16`。Python/C++ 进程 wall 中位数为
11.1030852/0.3779515 秒，**29.377×**，仅 **1/5** 配对达到30；
IQR/中位数为2.787%/5.123%，均在噪声阈值内。此批未达到主门槛。

全量单次的13个合同输出与P112逐字节相同，输入前后不变：wall4.1255791秒，
CPU3.5秒，峰值739,074,048 bytes；research1.8840145、export0.9773182、
load0.686883、factor0.425292、materialization0.0492637秒。
该单次 wall 高于R4的3.506秒，多个未修改阶段也增加；不同批次不能分离变化
来源，不能据此宣称本轮提速或稳定回退。可确认的是移除了中间全网格计算与
所有权，功能和字节契约通过；最终正式性能仍待验。

源/测试仅 `research.cpp` 与 `factor_research_detailed_test.cpp` 两文件。
精确命令、输入身份、原始结果哈希见 [JSON](p113-round5.json)，原始日志和结果
位于 `build-win/p113-r5-*` 与 `benchmarks/alpha003_a_stocks/results-i9-p113-r5-500`。
下一轮移除已排序 panel 的树形日期分组和预处理临时复制，并优化价格列成员查询。
