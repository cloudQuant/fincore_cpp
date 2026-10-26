# P125 R1：修复 MSVC ASan 构建配置

2026-10-25，既有 `D:/source_code/fincore_cpp` / `dev`；基点
`e4fed743aba20b2c0ebadecc0252f0f6d4df29c7`。这是收尾构建修复，不是新的性能优化。

原 ASan 构建在链接时出现两个问题：CMake 把编译器的 `/fsanitize=address`
直接传给 `link.exe`，被 LNK4044 忽略；只有 fincore 库插桩，测试、DataFrame、
GoogleTest 对象的 `annotate_string` / `annotate_vector` 与库不一致，触发 LNK2038。
同时有 34 个 ASan 符号未解析。安装的运行库存在；没有据此声称默认推断为何失效。

修复限于三个构建文件：

- MSVC 静态库通过 PUBLIC 编译选项传播 `/fsanitize=address`；最终链接使用
  有效的 `/INFERASANLIBS`。非 MSVC 原编译/链接分支保留。
- 本地源码构建的 DataFrame、`gtest`、`gtest_main` 和 `fincore_tests` 使用同一
  sanitizer helper，保持最终链接闭包的容器注解一致。外部 imported DataFrame
  不被修改，其预构建 archive 必须自身兼容；不兼容仍由链接器拒绝。
- 增量构建随后在 GoogleTest 的 `/WX` 下遇到 C5072（ASan 无调试信息）。增加
  `/Z7` 与 `/DEBUG`，提供调试信息，不禁用警告或容器注解。

以上符合微软的[容器注解一致性要求](https://learn.microsoft.com/en-us/cpp/sanitizers/error-container-overflow?view=msvc-170)、
[ASan 链接选项](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-building?view=msvc-170#linker)
及 [C5072 指引](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-c5072?view=msvc-170)。
独立静态审阅未发现阻塞问题。

## 验证

在已配置 VS 2022 x64 Developer PowerShell 中，沿用原 ASan build 目录增量修复，
保留首次链接失败和后续 C5072 失败日志；未重复创建开发 worktree。

```powershell
cmake -S . -B build-win-p120-asan-e4fed74
cmake --build build-win-p120-asan-e4fed74 --target fincore_tests -j4
ctest --test-dir build-win-p120-asan-e4fed74 --output-on-failure --parallel 1 --no-tests=error
```

配置为 MSVC 19.42、Release `/O2`、ASan ON、UBSan OFF；依赖来自既有本地源码。
本次覆盖 fincore 实现、测试源、DataFrame、GoogleTest；不是旧的仅库插桩配置。
MSVC 不支持的 UBSan 不标为通过。ASan 构建只用于正确性，未用于性能计分。

最终结果及二进制/日志摘要记录于 [p125-round1.json](p125-round1.json)。
ASan 完整 CTest **1,092/1,092**，退出 0，179.30 s，无 ASan 内存错误报告；
239 条编译命令全部含 `/fsanitize=address /Z7`，最终链接包含 `/INFERASANLIBS`。
构建有一个原有 `runtime_test.cpp:295` 的 C4858（丢弃 `make_shared` 返回值），
未抑制或顺带修改。修复后的默认 Release 完整 CTest **1,092/1,092**，退出 0，20.08 s。
原始日志为 `build-win/p125-r1-asan-debug-{configure,build,ctest}.log`。
默认 Release 的 sanitizer 选项仍关闭，单独重新构建并检查；生成的 ECharts
资源对象因 CMake 重新生成而重编、相关可执行文件重链接，故补测该最终 Release。

本轮不追加 Python/C++ 性能 campaign；用户已要求按当前性能收尾。
