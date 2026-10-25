# fincore_cpp 用 C++ 做量化分析与因子研究

做量化研究，计算一个因子只是开始。接下来还要看它与未来收益的关系，比较不同分位的表现，检查换手、覆盖率和稳定性，再把结果整理成能交流的报告。策略进入组合阶段后，收益、回撤、风险和归因又会成为每天重复运行的工作。

Python 版 **fincore** 把这些分析能力组织在同一个项目里。**fincore_cpp** 则是它的 C++ 重构版本：使用 C++23 和原生 DataFrame，把量化分析能力提供给 C++ 程序，也为较大规模的批量研究提供一条更直接的计算路径。

这篇文章介绍它能做什么、如何安装和调用，以及最近一次与 Python fincore 的实际性能对比。功能和测试数据以 **2026 年 10 月 25 日的 dev 分支**为准。

## 从 Python fincore 到 C++ 原生库

Python fincore 面向收益与风险分析、因子研究、组合分析和报告生成，适合与 pandas 等工具一起使用。只想使用 Python 版本，可以在 Python 3.11 及以上环境中通过 PyPI 安装：

```bash
python -m pip install fincore
```

这条命令安装的是 Python 项目。它的功能模块与安装说明见 [fincore 的 PyPI 页面](https://pypi.org/project/fincore/)。

fincore_cpp 采用 CMake 构建，通过头文件和链接库接入 C++ 工程。它以 `DataFrame` 为数据基础，使用明确的数值列、时间索引和结果类型。收益分析、因子计算与报告生成可以在同一个原生程序中完成。

两者共享许多分析概念，但接入方式不同。**当前 fincore_cpp 没有 Python 绑定，也不能直接接收 pandas 对象。** 从 Python 迁移时，需要把输入数据、时间戳、缺失值和返回结果映射到 C++ API，而不是只替换一行 import。

## 项目能完成哪些分析

### 收益与风险绩效

输入策略或资产的收益序列，可以计算累计收益、年化收益、波动率、Sharpe、Sortino、Calmar、最大回撤、alpha、beta 等指标，也可以按年度或滚动窗口观察它们的变化。

这类分析适合回答两个问题：策略整体表现如何，以及这种表现是否随着时间发生了变化。对于带基准的分析，索引对齐和收益频率需要按相应 API 的约定设置。

### 因子研究

因子研究是这次重构中重点打磨的工作流。项目提供因子数据准备、收益标签构建、横截面 rank 或 z-score 预处理，以及后续的研究统计。

常用输出包括：

- Pearson IC 与 Rank IC，用于观察因子值和未来收益的关系。
- IC 汇总与 ICIR，用于了解相关性的平均水平和稳定程度。
- 分位收益与高低分位价差，用于观察因子的排序效果。
- 分位换手、滞后相关、覆盖率和诊断信息，用于检查信号变化与数据质量。

此外还有多因子比较、相关性、组合、分组与分段研究接口。使用者可以分别研究不同时间区间和资产集合，而不必为每一种分析重新拼装整条计算流程。

### 组合与交易记录分析

项目可以分析已有持仓和成交记录，包括多空暴露、杠杆、集中度、换手、滑点调整、容量，以及往返交易统计。

例如，同样的收益曲线，可能来自分散的小仓位，也可能来自少数资产的集中暴露。把收益统计和持仓、交易分析放在一起，才能更完整地理解结果。

这里处理的是已有数据的分析；行情接入、订单执行和策略撮合仍需由外部系统提供。

### 风险模型与组合分析

更深入的分析能力包括 GARCH 家族模型、极值风险估计、VaR 与 ES 检验、滚动风险预测，以及 Brinson 归因、风格和因子回归。这里的风险回测用于检查风险预测与实际观测的关系。

组合构建方面，项目提供均值方差、最大 Sharpe、有效前沿和风险平价等接口；模拟方面包括 Monte Carlo、bootstrap 和压力情景。模型参数、约束和数值求解条件仍需要由使用者根据自己的研究目标确定。

### 报告与可视化

计算结果可以组织为报告，输出 HTML、XLSX、原生 PDF 等格式。HTML 支持内联 SVG 或嵌入 ECharts 资源；桌面图表另有可选的 ImPlot 组件。核心计算库可以独立构建，不要求启用 GUI。

最近还简化了 Alpha003 benchmark 的输出：Python 和 C++ 各自只生成一个离线 `research.html`，图表、样式和八张研究统计表嵌入同一个文件。文件可以直接分享和打开，不需要同时携带一批 CSV 和 JSON。

这些能力按 C++ 原生接口提供。Python 对象语义、PyMC 后验对象，以及依赖外部行情 SDK 的在线调用不属于当前原生兼容范围。具体范围可查阅[原生能力验收记录](https://github.com/cloudQuant/fincore_cpp/blob/dev/docs/acceptance/2026-10-24-iteration-8-native-parity-acceptance.md)。

## 安装 fincore_cpp

下面以 Windows 上的核心计算库为例。需要准备 Git、CMake 3.25 或更高版本、Ninja，以及支持项目所需 C++23 特性的编译器。本次测试使用 Visual Studio 2022 的 MSVC 19.42。

请在 **Visual Studio Developer PowerShell** 中运行命令，并确认 `cmake`、`ninja` 和 MSVC 编译器可用。先递归克隆 `dev` 分支，让 Git 同时取得固定版本的依赖：

```powershell
git clone --branch dev --recurse-submodules https://github.com/cloudQuant/fincore_cpp.git
cd fincore_cpp
```

已有仓库则可以补齐子模块：

```powershell
git submodule update --init --recursive
```

配置 Release 构建，关闭测试和可选 GUI，先编译核心库：

```powershell
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=OFF
cmake --build build-win --target fincore --parallel 4
```

`FINCORE_BUILD_TESTING` 默认开启；这里关闭它是为了让初次构建只关注库本身。需要运行项目测试时再开启，此时还需要 GoogleTest 依赖。

接下来把 DataFrame 和 fincore 安装到同一个目录。以下使用构建目录内的 `stage`，可以改成自己选择的绝对路径：

```powershell
$prefix = Join-Path $PWD.Path 'build-win/stage'
cmake --install build-win/_deps/dataframe-build --prefix "$prefix" --config Release
cmake --install build-win --prefix "$prefix" --component fincore --config Release
```

这里有两条安装命令，是因为 fincore 的 CMake 包会查找 `DataFrame::DataFrame` 依赖。只安装 `fincore` 组件，并不会把 DataFrame 一起安装。使用同一构建中的依赖，也能减少编译器和运行库配置不一致的问题。

macOS、Linux 同样采用 CMake 构建，但应使用满足项目要求的工具链，并按目标平台验证。这里展示的命令和接入示例使用 Windows 路径及 PowerShell 语法。

## 写一个可以运行的 C++ 例子

在仓库根目录新建 `demo` 目录，放入下面两个文件。

`demo/CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.25)
project(fincore_demo LANGUAGES CXX)

find_package(fincore 0.1 CONFIG REQUIRED COMPONENTS core)
add_executable(fincore_demo main.cpp)
target_link_libraries(fincore_demo PRIVATE fincore::fincore)
```

`demo/main.cpp`：

```cpp
#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>
#include <fincore/metrics.hpp>

#include <iostream>

int main() {
    const auto returns = fincore::make_series(
        {fincore::utc_timestamp(2025, 1, 6),
         fincore::utc_timestamp(2025, 1, 7),
         fincore::utc_timestamp(2025, 1, 8),
         fincore::utc_timestamp(2025, 1, 9),
         fincore::utc_timestamp(2025, 1, 10)},
        fincore::returns_column,
        {0.01, -0.005, 0.02, -0.01, 0.008});

    std::cout << "cumulative_return="
              << fincore::cumulative_return(returns) << '\n';
    std::cout << "max_drawdown="
              << fincore::max_drawdown(returns) << '\n';
    std::cout << "sharpe="
              << fincore::sharpe_ratio(returns) << '\n';
}
```

这里的 `0.01` 表示 1% 的单期收益，时间索引由 `utc_timestamp` 构造。Sharpe 使用默认的 252 年化因子和零无风险收益；五个观测只是接口示例，不适合据此评价策略。

继续在前面的 PowerShell 会话中编译并运行，`$prefix` 保持为刚才的安装目录：

```powershell
cmake -S demo -B build-demo -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$prefix"
cmake --build build-demo --parallel 4
./build-demo/fincore_demo.exe
```

这组示例数据的输出为：

```text
cumulative_return=0.0229169
max_drawdown=-0.01
sharpe=6.04755
```

这个程序通过安装后的 CMake 包寻找头文件和库，C++23 要求会从链接目标传递给调用方。实际接入时，再把示例序列换成自己的数据，并保持收益单位、频率和缺失值处理方式一致。

## 与 Python fincore 的性能对比

比较使用同一份 A 股日线数据，分别由 Python 和 C++ 独立读取原始 CSV，计算 Alpha003，再完成因子研究并写出 HTML。

Alpha003 的表达式为：

```text
alpha003 = -correlation(rank(open), rank(volume), 10)
```

其中 rank 是横截面排序，correlation 使用 10 期窗口。研究使用 1D、5D、20D 三个预测周期和五分位分组，标签为 next-open-to-open。数据采用未复权价格，统计结果未扣除交易费用。

这次比较在 Windows 11、Intel Core i9-13900HX 上进行，两个引擎固定在同一个逻辑 CPU，计算库相关线程数环境变量设为 1，动态线程设置为关闭。C++ 使用 MSVC Release；Python 使用 3.14.2、pandas 2.3.3、NumPy 2.4.1，以及固定提交的 fincore 参考实现。

每个规模先运行两对预热，再以交替顺序运行五对测量。计时覆盖进程启动、读取、计算、HTML 写入和进程退出。结果如下：

| 输入规模 | Python 中位耗时 | C++ 中位耗时 | 观测加速比 |
| --- | ---: | ---: | ---: |
| 500 个文件 | 6.734 秒 | 0.220 秒 | 30.58 倍 |
| 全量 5,471 个文件 | 46.566 秒 | 2.418 秒 | 19.26 倍 |

**这些数字是本次运行的观测值，稳定的加速幅度仍需进一步确认。** 两组 C++ 样本的 IQR/中位数分别为 12.48% 和 76.10%，均超过预设的 10% 波动阈值，因此性能结论记为 `INCONCLUSIVE`。不能据此保证所有规模都稳定快 30 倍。

数值一致性则全部通过：每对运行都检查研究参数和八张研究表的列、行顺序、字符串、整数、缺失值掩码与浮点值。500 文件和全量的最大绝对差分别约为 `3.89e-16` 和 `1.05e-15`，低于 `1e-6` 的比较阈值。

这也说明，性能比较需要写清楚具体任务。这里测的是 Alpha003 研究流程，包含输入输出与进程开销；它不代表每个指标函数都有同样的倍率。此前导出完整 CSV 明细的测试属于另一种输出工作量，不能与这组单文件 HTML 结果直接混用。完整方法和原始耗时见[本次基准记录](https://github.com/cloudQuant/fincore_cpp/blob/dev/docs/acceptance/2026-10-25-alpha003-html-benchmark.md)。

## 生成自己的研究报告

Alpha003 示例位于 `benchmarks/alpha003_a_stocks/`。准备好数据后，可以只运行 C++ 端：

```powershell
cmake --build build-win --target fincore_alpha003_research --parallel 4
./build-win/fincore_alpha003_research.exe --data-dir benchmarks/alpha003_a_stocks/data/a_stocks --max-symbols 500 --out benchmarks/alpha003_a_stocks/results-my-research --report-template benchmarks/alpha003_a_stocks/report_template.html
```

运行成功后，输出目录中只有 `research.html`。打开它可以查看 IC、分位收益、价差、换手、稳定性和覆盖信息，切换不同预测周期。完整计算仍然执行，大型 signals、labels 和因子面板明细不再写入报告。

输出目录需要不存在或为空。重新运行时请换一个目录，或自行处理已有结果，程序不会覆盖旧报告。

**示例原始行情数据没有放进 Git 仓库，克隆后需要自行准备。** 数据目录由每个证券一个 CSV 和共享交易日历组成，字段与布局见[数据说明](https://github.com/cloudQuant/fincore_cpp/blob/dev/benchmarks/alpha003_a_stocks/data/README.md)。复现两端性能比较还需要固定版本的 Python fincore；具体入口、环境和命令见 [benchmark 使用说明](https://github.com/cloudQuant/fincore_cpp/blob/dev/benchmarks/alpha003_a_stocks/README.md)。

统一对比脚本最终保留两份研究 HTML 和一份 `comparison.html`，预热与重复样本的临时输出会自动清理。全量批次三个文件总计约 2.16 MiB，便于本地阅读和分享；生成结果默认被 Git 忽略。

## 如何选择和参与

如果现有工作流主要在 Python 中进行，Python fincore 仍然是直接的入口。如果希望把分析能力嵌入 C++ 程序，或者正在建设需要反复计算的原生研究流程，可以从 fincore_cpp 的一个小模块开始，先核对数据约定和结果，再测量自己业务中的耗时。

项目采用 MIT 许可证，依赖组件保留各自的许可证与声明。欢迎通过 Issue 提供最小复现、性能样本或功能需求，也欢迎提交改进。我们希望后续优化继续围绕具体工作量展开：减少重复计算与不必要的分配，让结果保持可解释，并让每一次加速都有对应的正确性检查。

- [fincore_cpp 项目与 dev 分支](https://github.com/cloudQuant/fincore_cpp/tree/dev)
- [Python fincore 项目](https://github.com/cloudQuant/fincore)
- [fincore_cpp 问题反馈](https://github.com/cloudQuant/fincore_cpp/issues)
- [fincore_cpp 许可证](https://github.com/cloudQuant/fincore_cpp/blob/dev/LICENSE)
