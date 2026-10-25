# Alpha003：Python / C++ 单文件 HTML 研究报告

两端独立读取同一份 A 股原始 CSV，计算 Alpha101 alpha003，并调用各自的 fincore 研究 API。
每次运行只生成 **`research.html`**：CSS、JavaScript、SVG 图表与研究数据均嵌入文件，可离线打开，不依赖 CDN。

报告包含研究参数、Pearson / Rank IC、IC 汇总、分位收益、top-minus-bottom 价差、换手率、稳定性、覆盖与诊断。
支持切换 1D / 5D / 20D。不再输出 CSV、manifest、timings 或 run_info 侧车文件，也不把百万行因子、signals、labels 明细装进 HTML。
完整因子和研究计算仍然执行；计时与规模信息直接嵌入 HTML。

## 数据与计算

- `alpha003 = -correlation(rank(open), rank(volume), 10)`；横截面采用平均并列秩 / 有效资产数。
- 保持十个完整 rank pair 窗口、anchor-centered / two-stage-scaled Pearson 内核和常量窗口 NaN 语义。
- `data/a_stocks/` 含 5,471 个证券 CSV，2025 年 243 个观测交易日，以及共享日历。
- 默认均匀抽取 500 个文件，`--max-symbols 0` 使用全量；末尾预留 22 行，研究区间截止 2025-12-01。
- 标签为 next-open-to-open，周期 1/5/20，横截面 rank 预处理，5 分位；使用未复权价格，不包含交易成本。
- 输入数据保持不变且已被 Git 忽略，来源与复制命令见 [data/README.md](data/README.md)。

## 构建

在 Visual Studio Developer PowerShell 中，从仓库根目录运行：

```powershell
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DFINCORE_BUILD_TESTING=OFF
cmake --build build-win --target fincore_alpha003_research --parallel
```

Python 使用本地干净的 `fincore` checkout；比较脚本固定引用提交
`3e9ef205ef2023f92e89fddd1052d46b16429c64`。默认路径是 `D:/source_code/fincore`，
可用 `--fincore-root` 或 `FINCORE_ROOT` 指定。

## 单独生成报告

```powershell
python benchmarks/alpha003_a_stocks/python/run_research.py --max-symbols 500 `
  --fincore-root D:/source_code/fincore --out benchmarks/alpha003_a_stocks/results-python

build-win/fincore_alpha003_research.exe --data-dir benchmarks/alpha003_a_stocks/data/a_stocks `
  --max-symbols 500 --out benchmarks/alpha003_a_stocks/results-cpp `
  --report-template benchmarks/alpha003_a_stocks/report_template.html
```

两条命令分别只写 `<out>/research.html`。输出目录必须不存在或为空，已有内容不会被覆盖。
直接运行不强制 CPU 亲和性；性能比较请使用下方统一入口。

## 重新比较性能与结果

```powershell
python benchmarks/alpha003_a_stocks/run_benchmark.py --max-symbols 500 --repeats 5 --cpu 0 `
  --results-dir benchmarks/alpha003_a_stocks/results-html-500

python benchmarks/alpha003_a_stocks/run_benchmark.py --max-symbols 0 --repeats 5 --cpu 0 `
  --results-dir benchmarks/alpha003_a_stocks/results-html-full
```

每个批次最终只保留三个文件：

```text
results-html-500/
  python/research.html
  cpp/research.html
  comparison.html
```

`comparison.html` 包含对比结论、逐对时间、进程 CPU/峰值内存/亲和性、环境与输入摘要和每一对的数值检查。
Python/C++ 两份报告来自最后完成的一对。预热和重复测量的输出、日志在临时目录中处理，结束后自动清理，不累积 run1…N 文件。
所有 `results*/` 都被 `.gitignore` 排除，生成报告与原始数据均不上传 GitHub；只提交源码、测试、模板和简短交付记录。

比较规则：

1. 每端两个新进程预热，然后 AB/BA 交替测量。两个引擎从进程创建起使用同一个逻辑 CPU，计算库线程环境为 1。
2. 外部 process wall 包括解释器启动/导入、CSV 读取、因子和研究计算、HTML 写入关闭、进程退出。
3. 每对完成后、下对启动前，核对 HTML 内的 workload 和 8 张研究表：schema、行顺序、字符串/整数、缺失值掩码一致，有限浮点绝对误差不超过 `1e-6`。无穷值拒绝，NaN 编为 null。
4. 报告中位数、全部原始样本及 IQR/median 噪声状态；超过 10% 则标记 `INCONCLUSIVE`。数值一致不代表时延稳定。
5. 任一进程或比较失败即停止后续样本，失败原因嵌入 `comparison.html`，退出非零。

`--skip-run --results-dir ...` 仅复核保留的最后一对 HTML 与原记录摘要，不重新运行或声称重查已清理的中间输出。
新批次必须使用未存在的结果目录。

## 与旧基准的关系

当前契约是 **`alpha003-html-v1`**。旧 `alpha003-complete-v2` 要求导出并比较全部 13 个文件，包含大型因子、signals 和 labels 明细。
这次按用户要求改变了输出工作负载，新的加速比单独记录，不能当作原 CSV 完整输出门禁通过。
库原有 CSV/export API 没有改动；历史完整比较工具 `contract.py` 仍保留用于回归测试。

旧 `results`、`results-full` 和迭代 9 本地证据压缩包已按本次指令从项目目录移入回收站；需要时可重新生成。
历史开发记录仍在 Git 中，原始大文件不再要求保留。当前交付与实测结果见
[HTML benchmark 交付记录](../../docs/acceptance/2026-10-25-alpha003-html-benchmark.md)。
