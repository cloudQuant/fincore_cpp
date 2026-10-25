# P119 R2：独立合成训练的 PGO 实验未采用

2026-10-25，现有 dev，源码与普通 Release 均绑定
`d145354f1f16531bcd09e9074a0af66d9a91f9b8`。实验全部位于 ignored build 目录；
未修改默认 CMake、浮点选项、基准数据或 Python 实现。本轮提交仅记录实验结果。

MSVC 19.42.34435/toolset 14.42.34433 使用 `/GL /LTCG`、
`/GENPROFILE:EXACT,PGD=...` 与 `/USEPROFILE:PGD=...`。固定种子 20261025
生成独立的 5,471 资产、243 个工作日数据，含价格/成交量变化和缺失值。
同一插桩二进制依次训练 500 与全量；正式 P112 数据没有参与训练。
两个 PGC 显式合并，计数溢出损失均为 0.0%。优化链接前后 106 个对象散列不变。

两个规模分别 CPU0、两对预热加五对 AB/BA 测量；共 28 次运行的完整 13 文件
均与 P112 对应输出逐字节一致，源文件、输入和二进制身份前后不变。

| 规模 | 普通墙钟中位 | PGO墙钟中位 | 普通/PGO | 普通噪声 | PGO噪声 | PGO更快配对 |
|---|---:|---:|---:|---:|---:|---:|
| 500 | 0.308937s | 0.289738s | 1.06626 | 16.34% | 5.89% | 4/5 |
| full | 3.278636s | 3.496345s | 0.93773 | 8.01% | 14.66% | 2/5 |

噪声为 IQR/median。两规模均有一端超 10%，不能据此确认稳定提升或稳定回退。
500 的 CPU 中位为普通 0.296875s / PGO 0.265625s；full 为 2.953125s / 2.968750s。
PGO CPU 噪声亦超限。full 阶段中位分别为普通/PGO：加载 0.567611/0.527820s、
因子 0.355689/0.541605s、研究 1.224315/1.219626s、导出 0.828758/0.969519s。
阶段差只能帮助选择下一实验，不能证明编译器某项变换是原因。

因此不采用 PGO 构建，继续默认 Release 的源码优化。没有运行优化版全库单测、
oracle 或安装消费者，也不声称获得库级 PGO 发布验收。PGO 调用次数报告不是 CPU
采样 profile；最终 30×、两批十对、通用回归及资源门槛仍待验。

构建过程有三项可复现性说明。只有中文 MSVC 资源，`VSLANG=1033` 无效；
全量编译前用正常构建中已验证的中文 include 前缀修正新目录的 CMake 生成配置。
重配置会重写字节相同的 ECharts 生成源码，Root 因此直接执行 Ninja 生成的最终链接
命令，保留原始 `/GL` 对象。PGC 实际生成在可执行文件旁，原训练记录的 PGD 目录
列表为空；单独归档了两个 PGC 并显式合并，未重复训练。链接器又更新了 PGD，
已分别保存精确的合并输入和链接后数据库。早期 generator 改动期间生成的 training-r1
未使用；实际训练用固定最终 generator 生成并前后校验的 training-r2。

独立静态复核未发现构建或身份记录阻塞，未执行测试。精确命令、散列、逐样本统计、
profile 输入/输出身份和限制见 [JSON](p119-round2.json)；原始实验产物保留。
工具语义依据微软官方 [GENPROFILE](https://learn.microsoft.com/en-us/cpp/build/reference/genprofile-fastgenprofile-generate-profiling-instrumented-build?view=msvc-170)、
[USEPROFILE](https://learn.microsoft.com/en-us/cpp/build/reference/useprofile?view=msvc-170)
及 [PGC 合并](https://learn.microsoft.com/en-us/cpp/build/how-to-merge-multiple-pgo-profiles-into-a-single-profile?view=msvc-170) 文档。
