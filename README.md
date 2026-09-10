# Mini-C Studio · C 模块（GUI 界面 + 交互控制）

> 角色：C　｜　环境：Visual Studio 2026（内置 EasyX）· C++14 · Windows
> 依据：PPT 45–59 页（题目二）、组长的 `uml解释.docx`、A 的 GitHub 代码、B 的《三方对比.doc》
> 最后更新：2026-09-09 —— **已合并 B 同学（EditorBuffer）与 D 同学（Compiler）的真实代码**

---

## ⚠️ 编码与编译（务必先读）

本模块所有 `.cpp/.h` 源文件均为 **UTF-8 带 BOM** 编码。中文 Windows 上的 MSVC 默认把「无 BOM 的 UTF-8」当成系统代码页 936（GBK）误读，会爆出 `C4819` 警告并连带产生大量 `C2001/C2447/C2065` 报错（误把中文多字节里的 `\` 当成转义符）。

- **已修复**：本文件夹每个源文件都已加 UTF-8 BOM；`Mini-C-Studio.vcxproj` 也在所有配置里加了 `<AdditionalOptions>/utf-8</AdditionalOptions>`（双保险，有无 BOM 都能按 UTF-8 编译）。
- **用 VS 打开后若仍报错**：先「生成 → 重新生成解决方案」（Clean + Rebuild），确保旧的中间文件被清掉。
- **不要**用记事本另存为「ANSI / GBK」，也不要用「UTF-8 无 BOM」重新保存这些文件，否则会重新触发上述错误。
- 若把本模块文件并入小组共享工程，请让共享 `.vcxproj` 也加上 `/utf-8`，或保证提交的源文件带 BOM。

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
#define USE_D_REAL_COMPILER  0    // D：Compiler         —— ⚠️ 占位中（接口待对齐）
#define USE_D_REAL_RUNTIME   0    // D：Runtime          —— ⚠️ 占位中（D 尚未交付该实现）
```

- **B 同学** ✅：`EditorBuffer.h / .cpp` 已在 `src/` 并参与编译，程序跑的就是 B 的真实文本缓冲区。
  B 缺的 6 个 GUI 契约方法由 C 的 `EditorExt` 适配层用 B 的公开方法组合桥接，**B 的代码一个字符都没改**；坐标语义 `(列,行)` 由 `EditorExt.h` 的 `MINIC_SETCURSOR_IS_ROW_COL = 0` 统一。
- **E 同学** ✅：2026-09-10 交付，`FileManager.h / .cpp` 已接入（`src/`），`FileManager_test.cpp` 放 `tests/`。
  C 侧补了 3 处：① `CoreApi` 增加 `m_lastError / lastError()` 错误原因通道；② 三个文件函数成功后补 `setDirty(false)`；③ 打开/保存失败弹窗改为显示 E 返回的具体原因（如"文件不存在""权限不足"）。E 的代码本身未改。
- **D 同学** ⚠️：`Compiler.h / .cpp` 已在 `src/` 但**未接入**（D 是自由函数接口，与 `CoreApi` 成员契约不符，详见《B_D模块对接对齐清单》）；`Runtime` 实现**尚未交付**，目前运行功能由 `MiniStub.h` 的 `MiniRuntime` 占位（已修 `cmd /S /c` 引号问题）。
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
| `MiniStub.h` | C（临时） | D 的占位实现（`MiniCompiler` / `MiniRuntime`）+ 通用工具 |
| `UiDialog.h / .cpp` | **C** | 自绘弹窗 |
| `SplashScreen.h / .cpp` | **C** | 启动欢迎界面（可选读 `res\logo.png`，缺失时自动画文字 LOGO） |
| `Compiler.h / .cpp` | **D** | D 的编译调度（**已上传但未接入**，接口待对齐） |
| `tests/EditorBuffer_test.cpp` | **B** | B 的缓冲区单元测试（自带 `main`，**务必排除出主工程**） |
| `tests/FileManager_test.cpp` | **E** | E 的文件管理单元测试（自带 `main`，同上） |
| **接口使用说明.md** | C | ★ 给全组的接口契约与对接方法 |
| **三方对比核查结论.md** | C | ★ 对 B 那份文档的逐条核查结果 |
| **A文件改动对照.md** | C | ★ A 的文件改了什么、原始代码在哪 |
| **B同学答复_采纳说明.md** | C | ★ 对《B同学答复清单》的逐条采纳记录 |

---

## ★ 需要提交到 GitHub 的文件清单

> 仓库现有 `Mini-C-Studio/` 下已经有 B 的 `EditorBuffer.*` 和 D 的 `Compiler.*`。
> **你只需要把下面「C 负责的」文件推上去即可**；B/D 的文件已在仓库中，不必重复提交（即使提交了也是完全相同的内容）。

把本文件夹整体拷进仓库的 `Mini-C-Studio/` 目录后，**需要 `git add` 的 C 文件**：

### 新增（仓库里原本没有）
```
CoreApi.h            CoreApi.cpp
CoreTypes.h
MiniCConfig.h
MiniStub.h
EditorExt.h          EditorExt.cpp
SplashScreen.h       SplashScreen.cpp
UiDialog.h           UiDialog.cpp
```

### 覆盖更新（替换仓库里 A 的对应文件）
```
MainWindow.h         MainWindow.cpp      ← C 的完整 GUI，覆盖 A 的骨架
main.cpp             ← 仅比 A 多了 2 行（include SplashScreen + 显示欢迎界面）
```

### 可选更新
```
README.md            ← 用本文件夹这份（已写明合并状态与提交清单）
```

### 不必提交 / 不要动
```
EditorBuffer.h  EditorBuffer.cpp  EditorBuffer_test.cpp   ← B 的，已在仓库
Compiler.h     Compiler.cpp                            ← D 的，已在仓库
*.md（接口使用说明 / 三方对比核查结论 / A文件改动对照 / B同学答复_采纳说明）
                    ← 只是沟通与说明文档，可放可不放；放上去方便队友看，但不影响编译
```

**一句话**：`git add` 上面「新增」9 个文件 + 「覆盖更新」的 `MainWindow.h/.cpp`、`main.cpp`，再 `commit` 即可。B 和 D 的文件已经在仓库里，别重复 add。

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

- 文本缓冲区的数据结构与增删算法 → **B**（已合并真实代码）
- 文件读写、路径、权限 → **E**（占位中）
- 调 gcc、解析报错 → **D**（已合并真实代码）
- 运行 exe、IO 重定向、超时判定 → **D**（已合并真实代码）

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

## 三条纪律（合并后更新）

1. **不删改 A 骨架的公开接口** —— `MainWindow` 的公开函数签名保持与 A 一致，只在内部填实现。
2. **不碰 B / D / E 的交付文件** —— `EditorBuffer.*`、`Compiler.*`、`FileManager.*` 一律不改；差异全部由 C 的 `EditorExt` 适配层或 `CoreApi` 门面消化。
3. **`MiniStub.h` 现在只占位 D** —— B、E 都已接真实实现；D 的 `Compiler` 接口对齐 + `Runtime` 交付后，把 `CoreApi.cpp` 顶部两个宏改成 1 即可，GUI 层一行不改。
4. **换行约定** —— C 侧**不做** `\n` → `\r\n` 转换，E 的 `FileManager` 内部已转；两边都转会变成 `\r\r\n`。
