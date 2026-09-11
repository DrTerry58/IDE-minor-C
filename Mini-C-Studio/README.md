# Mini-C Studio · C 模块（GUI 界面 + 交互控制）

> 角色：C　｜　环境：Visual Studio 2026（内置 EasyX）· C++14 · Windows
> 依据：PPT 45–59 页（题目二）、组长的 `uml解释.docx`、A 的 GitHub 代码、B 的《三方对比.doc》
> 最后更新：2026-09-11 —— **B（EditorBuffer）、D（Compiler/Runtime）、E（FileManager）已全部真实接入，占位实现全部退役**
>
> 📁 **目录结构（组长 2026-09-10 整改）**：`Mini-C-Studio/src/`（所有 `.cpp/.h`）、`tests/`（测试）、`docs/`（文档）、`res/`（图片）、`README.md`（根目录）。
> 源码同处 `src/` 一个目录，各文件 `#include "Xxx.h"` **无需修改**；但 `tests/` 里的测试文件必须写 `#include "../src/Xxx.h"`。

---

## ⚠️ 编码与编译（务必先读）

本模块所有 `.cpp/.h` 源文件均为 **UTF-8 带 BOM** 编码。中文 Windows 上的 MSVC 默认把「无 BOM 的 UTF-8」当成系统代码页 936（GBK）误读，会爆出 `C4819` 警告并连带产生大量 `C2001/C2447/C2065` 报错（误把中文多字节里的 `\` 当成转义符）。

- **2026-09-11 全量统一 BOM**：`src/` 与 `tests/` 下的 **每一个** `.h / .cpp` 都已加上 UTF-8 BOM（含 B 的 `EditorBuffer.*`、E 的 `FileManager.*`、D 的 `Compiler.* / Runtime.*`）。BOM 只加在文件最开头 3 字节，**代码内容一字未改**，因此覆盖提交不会影响任何人的实现。
- **为什么不用 `/utf-8`**：本工程是 **MBCS（多字节）工程**，加 `/utf-8` 会把执行字符集也改成 UTF-8，与 `outtextxy` 按 GBK 解码中文的逻辑冲突（界面中文会变乱码）。正确做法是 **MBCS + 源文件带 BOM**，让编译器按 BOM 识别源码编码、执行字符集仍走 GBK。故 `.vcxproj` **不要** 加 `/utf-8`。
- **用 VS 打开后若仍报错**：先「生成 → 重新生成解决方案」（Clean + Rebuild），确保旧的中间文件被清掉。
- **不要**用记事本另存为「ANSI / GBK」，也不要用「UTF-8 无 BOM」重新保存这些文件，否则会重新触发 `C4819`。
- 新增源文件时请一并带 BOM；判别方法：用 VS「文件 → 另存为 → 编码保存」，选 **Unicode (UTF-8 带签名) - 代码页 65001**（带签名 = 带 BOM）。

## 另外两个已修的编译问题

1. **`error C2589` “(”:“::”右边的非法标记”（MainWindow.cpp:630）**
   原因：EasyX 间接包含 `<windows.h>`，其中定义了 `max` 宏，把 `std::max(a,b)` 劫持成 `std::(a,b)`。
   修复：`std::max` 写成 `(std::max)(a,b)`，括号阻止宏匹配，逻辑不变。该文件只此一处用到。
   （注：B 的 `EditorBuffer.cpp` 也用了 `std::max/min`，但它**不包含 windows 头**，不受影响，无需改动。）

2. **链接错误 `multiple definition of 'main'`**
   原因：`EditorBuffer_test.cpp`（B 的缓冲区单元测试）自带一个 `main`，与本模块的 `main.cpp` 冲突；原 vcxproj 把它也编进了主程序。
   修复：在 vcxproj 中将 `EditorBuffer_test.cpp` 设为「不参与生成」（它是 B 的测试，不属于 IDE 主程序）。
   若想单独跑 B 的缓冲区测试，请用：`g++ EditorBuffer_test.cpp EditorBuffer.cpp -o buf_test` 单独编译。

## 当前状态（合并后）

当前 4 个对接开关都在 `src/CoreApi.cpp` 顶部：

```cpp
#define USE_B_REAL_BUFFER    1    // B：EditorBuffer     —— ✅ 已接入并编译验证通过
#define USE_E_REAL_FILEMGR   1    // E：FileManager      —— ✅ 已接入并编译验证通过
#define USE_D_REAL_COMPILER  1    // D：Compiler         —— ✅ 2026-09-11 已按契约改造为类成员函数，已接入
#define USE_D_REAL_RUNTIME   1    // D：Runtime          —— ✅ 2026-09-11 新交付（异步轮询），已接入
```

- **B 同学** ✅：`EditorBuffer.h / .cpp` 已在 `src/` 并参与编译，程序跑的就是 B 的真实文本缓冲区。
  B 缺的 6 个 GUI 契约方法由 C 的 `EditorExt` 适配层用 B 的公开方法组合桥接，**B 的代码一个字符都没改**；坐标语义 `(列,行)` 由 `EditorExt.h` 的 `MINIC_SETCURSOR_IS_ROW_COL = 0` 统一。
- **E 同学** ✅：2026-09-10 交付，`FileManager.h / .cpp` 已接入（`src/`），`FileManager_test.cpp` 放 `tests/`。
  C 侧补了 3 处：① `CoreApi` 增加 `m_lastError / lastError()` 错误原因通道；② 三个文件函数成功后补 `setDirty(false)`；③ 打开/保存失败弹窗改为显示 E 返回的具体原因（如"文件不存在""权限不足"）。E 的代码本身未改。
- **D 同学** ✅（2026-09-11）：`Compiler` 已由自由函数改造为**类成员函数**（`available() / compile(src) / exePathOf(src)`，与 `MiniStub.h` 的 `MiniCompiler` 契约一致）；`Runtime` 类为新交付，用 `CreateProcess` + `CreatePipe` + `PeekNamedPipe` 实现异步轮询，绕过 `cmd /S /c` 引号坑并修复了退出码被覆盖为 -1 的问题。
  C 侧改动：两个宏置 `1`；为保留"无 gcc"测试能力，在门面层 `CoreApi::compilerAvailable()` 保留了 `MINIC_FORCE_NO_COMPILER` 钩子（D 的代码未改）。
  实测（D 自带的 `test_main.cpp`）：gcc 检测、`exePathOf`、编译成功并生成 exe、运行输出与退出码 0、错误诊断解析 —— **5 项全 PASS**；全量编译链接零错误。
  ⚠️ 联调待确认：1 个真实语法错误目前会返回 4 条诊断（含 gcc 上下文行与源码回显行，其 `line/column` 为 0），已在《B_D模块对接对齐清单》中反馈给 D。
- **A 组长**的 `MainWindow` 骨架由 C 在此基础上【只追加、只填函数体】实现完整 GUI；`src/MainWindow.*` 是 C 的完整版，**用于覆盖仓库里 A 的骨架**。

> 详见《B同学答复_采纳说明.md》：B 的 14 个答复里，C 侧只在「适配层 + 写盘换行转换」消化，B 的代码一个字符都没改。

---

## 文件夹内容

| 文件 | 负责人 | 说明 |
|---|---|---|
| `main.cpp` | A(+C 2 行) | 入口，加了启动欢迎界面 |
| `MainWindow.h` | A(+C 追加) | 主窗口类（C 完整实现） |
| `MainWindow.cpp` | A(+C 填函数体) | 全部界面绘制与交互（覆盖 A 骨架） |
| `EditorBuffer.h / .cpp / _test.cpp` | **B** | ★ B 的真实文本缓冲区（已合并，勿改） |
| `EditorExt.h / .cpp` | **C** | C 的缓冲适配层（光标语义、选区、查找替换） |
| `MiniCConfig.h` | **C** | 布局常量 / 主题配色 / 绘图与字符串工具 |
| `CoreTypes.h` | C + D | 复用 D 的 `Diagnostic` / `CompileResult` 等结构 |
| `CoreApi.h / .cpp` | **C** | GUI 访问 E / D 模块的唯一入口（对接开关在这） |
| `FileManager.h / .cpp` | **E** | ★ E 的真实文件管理（2026-09-10 已接入，勿改） |
| `MiniStub.h` | C（临时） | 通用工具 + **已退役的占位实现**（B/D/E 全部接入后不再参与运行，保留仅作回退与对照） |
| `UiDialog.h / .cpp` | **C** | 自绘弹窗 |
| `SplashScreen.h / .cpp` | **C** | 启动欢迎界面（可选读 `res\logo.png`，缺失时自动画文字 LOGO） |
| `Compiler.h / .cpp` | **D** | ★ D 的真实编译调度（2026-09-11 已改造为类成员函数并接入，勿改） |
| `Runtime.h / .cpp` | **D** | ★ D 的真实运行时托管（2026-09-11 新交付并接入，`CreateProcess`+管道异步轮询，勿改） |
| `tests/EditorBuffer_test.cpp` | **B** | B 的缓冲区单元测试（自带 `main`，**务必排除出主工程**） |
| `tests/FileManager_test.cpp` | **E** | E 的文件管理单元测试（自带 `main`，同上） |
| **接口使用说明.md** | C | ★ 给全组的接口契约与对接方法 |
| **三方对比核查结论.md** | C | ★ 对 B 那份文档的逐条核查结果 |
| **A文件改动对照.md** | C | ★ A 的文件改了什么、原始代码在哪 |
| **B同学答复_采纳说明.md** | C | ★ 对《B同学答复清单》的逐条采纳记录 |

---

## ★ 需要提交到 GitHub 的文件清单

> 下面 9 个文件是 MD5 级比对的最终结果：除 BOM 外内容与仓库不一致的**只有 3 个**（`CoreApi.cpp`、`FileManager_test.cpp`、`README.md`），
> 另外 6 个（`Compiler.*`、`Runtime.*`、`FileManager.*`）**代码与仓库完全一致，只是补了 BOM**，目的是彻底消除 MSVC `C4819`。

### ① 内容有实质改动（必传）

| 本地文件 | 上传到仓库路径 | 改了什么 |
|---|---|---|
| `CoreApi.cpp` | `Mini-C-Studio/src/CoreApi.cpp` | `USE_D_REAL_COMPILER` / `USE_D_REAL_RUNTIME` 置 `1`；`MINIC_FORCE_NO_COMPILER` 测试钩子移至 `CoreApi::compilerAvailable()` |
| `FileManager_test.cpp` | `Mini-C-Studio/tests/FileManager_test.cpp` | include 改 `../src/FileManager.h`（适配 `tests/` 目录）；补 BOM |
| `README.md` | `Mini-C-Studio/README.md` | 状态、编码约定、提交清单更新 |

### ② 仅补 BOM，代码一字未改（为消除 C4819，建议一并覆盖）

| 本地文件 | 上传到仓库路径 | 原作者 |
|---|---|---|
| `Compiler.h` | `Mini-C-Studio/src/Compiler.h` | D |
| `Compiler.cpp` | `Mini-C-Studio/src/Compiler.cpp` | D |
| `Runtime.h` | `Mini-C-Studio/src/Runtime.h` | D |
| `Runtime.cpp` | `Mini-C-Studio/src/Runtime.cpp` | D |
| `FileManager.h` | `Mini-C-Studio/src/FileManager.h` | E |
| `FileManager.cpp` | `Mini-C-Studio/src/FileManager.cpp` | E |

> 这 6 个文件已用「去 BOM + 归一化换行」后逐字节比对，与仓库现有版本**完全相同**，只是文件头多 3 字节 `EF BB BF`。

### ③ 已经一致、不用再传

`main.cpp`、`MainWindow.*`、`MiniCConfig.h`、`CoreTypes.h`、`CoreApi.h`、`MiniStub.h`、`EditorExt.*`、`SplashScreen.*`、`UiDialog.*`、`EditorBuffer.*`、`tests/EditorBuffer_test.cpp`（均已带 BOM 且内容与仓库一致）。

### ④ 工程配置提醒（给组长）

- `vcxproj` 的 `ClCompile` 需包含 `Compiler.cpp`、`Runtime.cpp`（D 新交付），`FileManager.cpp`、`EditorBuffer.cpp`、`EditorExt.cpp` 已在。
- `tests/` 下的 3 个测试文件（`EditorBuffer_test.cpp`、`FileManager_test.cpp`、`test_main.cpp`）均自带 `main`，**必须从主程序生成中排除**。
- `vcxproj` **不要**加 `/utf-8`（MBCS 工程，详见上方「编码与编译」）。

---

## 属于 C 的功能（PPT 52 页）

- 菜单栏（6 组 24 命令）+ 工具栏（20 按钮）
- 编辑区渲染：行号栏、语法高亮、当前行高亮、选区高亮、光标闪烁
- 鼠标：点击定位、拖拽选区、滚轮滚动、滚动条拖动
- 键盘：可打印字符、中文输入、方向键（含跳行）、Home/End、翻页、Ctrl 组合
- 查找 / 替换条（C 用 B 的只读接口自带实现，不依赖 B 补 findNext）
- 底部面板：诊断（点击跳转源码）+ 控制台（输出回显 + 输入下发）
- 状态栏：文件名、脏标记、行列、编译状态、主题、版本、临时提示
- 异常弹窗：文件读写失败、未找到编译器、源码为空、exe 不存在、未保存确认
- 加分项：语法高亮、撤销重做、亮暗主题切换、启动欢迎界面

## 不属于 C、只留接口

- 文本缓冲区的数据结构与增删算法 → **B**（✅ 真实代码已接入）
- 文件读写、路径、权限 → **E**（✅ 真实代码已接入）
- 调 gcc、解析报错 → **D**（✅ 真实代码已接入）
- 运行 exe、IO 重定向、超时判定 → **D**（✅ 真实代码已接入；`Runtime` 为管道异步轮询，**目前 `compile()` 未设超时**，如需编译超时检测由 D 补充 `CS_TIMEOUT`）

---

## 快捷键

| 键 | 功能 | 键 | 功能 |
|---|---|---|---|
| `Ctrl+N` | 新建 | `Ctrl+F` | 查找 |
| `Ctrl+O` | 打开 | `Ctrl+H` | 替换 |
| `Ctrl+S` | 保存 | `F3` / `Shift+F3` | 下一个 / 上一个 |
| `Ctrl+Shift+S` | 另存为 | `F9` | 编译 |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 | `F5` | 运行 |
| `Ctrl+X/C/V` | 剪切 / 复制 / 粘贴 | `Ctrl+F5` | 编译并运行 |
| `Ctrl+A` | 全选 | `F1` | 关于 |
| `Esc` | 关闭查找条 / 取消选区 | | |

---

## 四条纪律（2026-09-11 全部模块接入后更新）

1. **不删改 A 骨架的公开接口** —— `MainWindow` 的公开函数签名保持与 A 一致，只在内部填实现。
2. **不碰 B / D / E 的交付文件** —— `EditorBuffer.*`、`Compiler.*`、`FileManager.*` 一律不改；差异全部由 C 的 `EditorExt` 适配层或 `CoreApi` 门面消化。
3. **`MiniStub.h` 的占位实现已全部退役** —— B、D、E 都已接真实实现（4 个宏全为 `1`）。占位类保留在文件里仅作回退与契约对照，运行时不会被调用。
4. **换行约定** —— C 侧**不做** `\n` → `\r\n` 转换，E 的 `FileManager` 内部已转；两边都转会变成 `\r\r\n`。
5. **编码约定** —— `src/`、`tests/` 下所有 `.h / .cpp` 统一 **UTF-8 带 BOM**（2026-09-11 已全量处理）；`.vcxproj` 保持 MBCS、**不加** `/utf-8`。
