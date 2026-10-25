# P111 R1：可信测量组件

2026-10-25，`D:/source_code/fincore_cpp`，现有 `dev`，起点
`6a7e77db96bcc023f88b33622eac28ffc3b864ac`。交付提交为本记录首次引入的提交。
本轮只验收测量修复；P111整体为PARTIAL，30×/100×均未验收。

修复Windows C++错误的CPU计时，使用GetProcessTimes kernel+user；外层从
CreateProcess前到wait返回计完整wall，退出后读进程CPU与peak working set。
内部单独记录启动准备、加载、因子、研究输入物化、研究、导出和other，不能代替完整进程用时。
父进程在创建子进程前临时限定单逻辑核，创建后恢复自身并核验子进程继承结果；
双方显式设置同一组单线程环境。完整受控执行目前支持Windows，其他平台明确拒绝。

双方各预热两次（AB、BA），随后逐对AB/BA交替；每对输出比较结束后才启动下一对。
已有结果目录一律拒绝覆盖，recheck另写新文件。报告保留全部样本、IQR、CPU、
内存和实际affinity；Python固定为clean `3e9ef205`，记录实际导入位置、包源hash、
数据顺序/字节身份、二进制、编译配置、CPU型号、系统与电源方案。

新增固定协议的统计组件：IQR/median>10%判INCONCLUSIVE；至少10对、seed92026、
10,000次配对索引bootstrap，按重采样后中位数之比及95%区间判定5%回归。
八类benchmark的实际配对campaign尚未接入，旧tools入口的10%规则仍不是本门禁。
profile和该集成安排在P112完整结果契约后，避免在不等工作量上冻结基线。

验证：

- `C:/Python314/python.exe -m unittest tools.tests.test_measurement tools.tests.test_alpha003_benchmark -v`：16项通过；覆盖非等比例bootstrap oracle、超时/失败清理、父affinity恢复、末对失败保留和禁止覆盖历史结果。
- Python runner `py_compile`通过；MSVC 14.42 Release `/O2 /Ob2 /DNDEBUG`构建
  `fincore_alpha003_research`成功。初次cmd引号导致开发环境未加载，编译因找不到
  `stdexcept`停止；改用VS的`Launch-VsDevShell.ps1`后成功，候选源码未因此改动。
- 真实50文件诊断命令与14个进程资源/阶段记录见
  [小型原始测量记录](p111-round1-measurements.json)。5对加两对预热，mask均为1；
  内部阶段和总时间闭合、CPU/peak可读、全部输入前后相同。
- 现有部分数值比较每对通过，最大主误差`7.771561172376096e-16`。
  此比较仍遗漏Python部分指标，报告明确`NOT_ELIGIBLE_PENDING_FULL_PARITY`。
- 最终收口把因子计算之后的输入筛选/dataframe构造从other拆成
  `materialization_seconds`，以便后续定位布局转换成本。重建后用50文件、1对加
  两对预热验证这项计时变化：6个进程阶段闭合且全部旧子集检查通过，见
  [最终计时字段smoke](p111-round1-materialization-smoke.json)。原5对记录保留，
  该1对只验新字段，不能提供性能统计结论。

诊断进程wall中位数为Python 4.562514 s、C++ 0.093403 s。
Python IQR/median 11.18%，判INCONCLUSIVE；C++为4.82%。这是50文件测量链路测试，
既不是500/full正式样本，也不是速度提升或30倍达标证据，不据此调整性能目标。
全量CSV保留在git忽略的`results-i9-p111-smoke-r1`，历史results/results-full未覆盖。

独立静态审阅未发现本轮阻塞；指出的源身份和环境记录缺口已修复。
下一轮P112增加可复用的版本化详细研究结果，保留旧API/导出兼容，补齐Pearson、
每日分位/价差、逐期换手率、NaN标签及同集合稳定性，再冻结正式基线。
