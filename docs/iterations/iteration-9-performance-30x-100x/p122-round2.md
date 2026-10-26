# P122 R2：复用相邻日期与价格列查询

2026-10-25，现有 `D:/source_code/fincore_cpp`、`dev`，base commit
`6417974c4c4e690739d1d5e92c279363854d9197`。接受范围为源代码、结果一致性和
开发批次 CPU 改善；端到端墙钟收益仍为 INCONCLUSIVE，不能算正式 30× 通过。

`append_detailed_next_open_horizons` 在 panel 行数多于 double 价格列数时直接
建立价格列查询表，避免对每一行重复插入资产名；宽表、少行输入继续只保存选中
资产。相邻相同日期复用查询结果，包括缺失日期。任意无序日期变化仍重新查询。
列名的 C-string 语义、验证顺序、NaN 填充、周期顺序和浮点表达式保持一致。

新增一个回归覆盖两种列映射分支、连续日期命中/缺失，以及离开后重新访问同一
日期。独立静态审查无阻塞；Root Release 构建、75 项相关测试和 3 个实际 Python
oracle 用例通过。未为本轮重复安装消费者或全量 CTest；最终门禁仍待执行。

全量 native 对照使用冻结的 P124 可执行文件与本轮候选，CPU 0、单线程环境、
两对预热和五对 AB/BA 计分。14 次运行的 13 个契约文件均与 P112 字节一致，
源文件、数据和二进制前后摘要一致。全部样本保留：

| 指标 | P124 参考 | 本轮候选 | 判定 |
|---|---:|---:|---|
| 进程 CPU 中位数 | 2.421875 s | 2.343750 s | 减少 3.23%；4 对降低、1 对相同 |
| CPU IQR/中位数 | 3.87% | 3.33% | 两端有效 |
| 进程 wall 中位数 | 2.910150 s | 2.833162 s | 参考噪声超限，INCONCLUSIVE |
| wall IQR/中位数 | 14.05% | 2.09% | 不宣称稳定 wall 改善 |
| 峰值工作集最大值（计分） | 239,046,656 B | 239,009,792 B | 无内存回退迹象 |

实际命令（先进入 VS 2022 x64 Developer PowerShell）：

```powershell
cmake --build build-win --target fincore_tests fincore_research_detailed_oracle fincore_alpha003_research -j4
build-win/tests/fincore_tests.exe --gtest_filter=FactorResearch.*:FactorResearchDetailed.*:FactorResearchDetailedExport.* --gtest_output=json:build-win/p122-r2-unit.json
C:/Python314/python.exe tools/check_research_detailed_oracle.py --probe build-win/fincore_research_detailed_oracle.exe
C:/Python314/python.exe build-win/compare_alpha_source_builds_p124_r1.py --reference-bin build-win/p122-r2-reference/fincore_alpha003_research.exe --candidate-bin build-win/fincore_alpha003_research.exe --max-symbols 0 --pairs 5 --cpu 0 --output-dir benchmarks/alpha003_a_stocks/results-i9-p122-r2-native-full-ab
```

本轮复用现有参数化对照脚本，没有再派生计时框架。精确源/产物摘要和样本见
[机器记录](p122-round2.json)。500 跨语言本轮未重跑；正式 500/full 两批与通用
回归仍待验收。上一个 P124 全量正式尝试及等待问题见 [P120 R1](p120-round1.md)。
