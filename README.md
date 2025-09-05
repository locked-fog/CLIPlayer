# CLI Player - 命令行文字PV播放器

[![Build Status](https://github.com/locked-fog/CLIPlayer/actions/workflows/ci.yml/badge.svg)](https://github.com/locked-fog/CLIPlayer/actions/workflows/ci.yml)

一个用于创建和播放终端文本动画的工具，支持精确到毫秒的时间控制、丰富的文本样式和颜色，以及模拟命令行交互。

## ✨ 功能 (Features)

-   **精确时间控制**：基于 `[mm:ss.zzz]` 格式的时间戳。
-   **丰富的指令集**：支持文本、清屏、移动光标、样式、颜色和两种换行模式。
-   **字符转义**：支持输出字面上的 `[` 和 `]` 符号 (`&[`, `&]`)。
-   **模拟CLI**：`[newline]` 指令会自动添加 `用户名>` 提示符，增强沉浸感。
-   **跨平台支持**：使用 CMake 构建，可在 Windows, Linux 和 macOS 上编译和运行。
-   **错误检查机制**：内置“防乱轴”和“防超时”保护。
-   **UTF-8 支持**：原生支持多语言字符显示。

## 🔧 如何编译 (How to Compile)

本项目使用 **CMake** 作为官方构建系统。请确保您的系统上已安装 CMake (版本 3.12+) 和一个 C++17 编译器。

1.  **克隆仓库**
    ```bash
    git clone https://github.com/locked-fog/CLIPlayer.git
    cd CLIPlayer
    ```

2.  **创建构建目录**
    ```bash
    mkdir build
    cd build
    ```

3.  **配置并生成构建文件**
    ```bash
    cmake ..
    ```

4.  **编译**
    ```bash
    cmake --build . --config Release
    ```
    编译完成后，可执行文件 (`CLIPlayer` 或 `CLIPlayer.exe`) 会出现在 `build` 目录（或 `build/Release` 目录）中。

## 🚀 如何运行 (How to Run)

```bash
# 在 Linux/macOS 上
./CLIPlayer ../example.clip

# 在 Windows 上
.\Release\CLIPlayer.exe ..\example.clip
```

## 📝 .clip 文件格式指南

`.clip` 文件是驱动播放器的脚本。它由一系列带有时间戳的指令组成。

### 文件结构

1.  **用户名 (必须在第一行)**
    文件必须以 `[username]` 指令开头，它定义了在模拟命令行交互时显示的提示符。
    ```
    [username]YourName@YourPC
    ```

2.  **时间轴指令**
    从第二行开始，每一行都代表一个或多个在特定时间点触发的动作。
    ```
    [mm:ss.zzz]Action1[Action2]SomeText...
    ```

3.  **注释**
    以 `//` 开头的行将被程序忽略。
    ```
    // 这是一条注释
    ```

### 时间戳格式

时间戳格式为 `[mm:ss.zzz]`，其中：
- `mm`: 分钟 (两位数)
- `ss`: 秒 (两位数)
- `zzz`: 毫秒 (三位数)

**重要**：文件中的时间戳必须是严格递增的，否则会触发“防乱轴”保护。

### 字符转义 (Character Escaping)

为了能输出作为普通字符的方括号 `[` 和 `]`，你需要对它们进行转义：

-   `&[` 将被解析为单个字符 `[`。
-   `&]` 将被解析为单个字符 `]`。

**示例**: `[00:01.000]这是一个转义示例&[Hello&]` 将会输出 `这是一个转义示例[Hello]`。

### 指令集

指令被包裹在 `[]` 中。

| 指令 | 格式 | 描述 |
| :--- | :--- | :--- |
| **清屏** | `[clear]` | 清空整个终端屏幕。 |
| **换行(带提示符)** | `[newline]` | 换行，并在新行的开头显示 `用户名> ` 样式的提示符。 |
| **换行(无提示符)** | `[newlinenp]` | **(新增)** 仅换行，不显示任何额外内容。 |
| **空格** | `[space]` | 输出一个半角空格。 |
| **移动光标**| `[mv R C]` | 将光标移动到指定的行 `R` 和列 `C`。左上角为 `(1, 1)`。 |
| **粗体** | `[bold]` | 使后续的文本变为粗体。 |
| **斜体** | `[italic]` | 使后续的文本变为斜体。 |
| **下划线** | `[underline]` | 为后续的文本添加下划线。 |
| **删除线** | `[strikethrough]`| 为后续的文本添加删除线。 |
| **设置颜色** | `[color RRGGBB]` | 使用6位十六进制代码设置后续文本的颜色。例如 `[color ff5733]`。 |
| **重置样式** | `[color default]` | 重置所有样式（颜色、粗体等）为终端默认值。 |

### 输出纯文本

任何没有被 `[]` 包裹的字符（并且不是转义序列的一部分）都将被视为纯文本，在指定的时间点被原样输出到当前光标位置。

### 示例

```

// 在 1.5 秒时，清空屏幕并显示带颜色的粗体字
[00:01.500][clear][mv 5 10][bold][color 55aaff]Hello, CLI Player!

// 在 3.0 秒时，重置样式，并使用带提示符的换行
[00:03.000][color default][newline]这是一个带提示符的新行。

// 在 4.5 秒时，使用不带提示符的换行来创建文本块
[00:04.500][newlinenp][italic]这是一个文本块的第一行...
[00:05.000][newlinenp]...这是第二行，没有提示符。

// 在 6.0 秒时，演示转义字符
[00:06.000][color default][newline]转义演示: &[color default&]

```

## 🗺️ 未来计划 (Roadmap)



- **[ ] 字符画生成器集成**: 集成一个工具（可能基于 FIGlet 或 FreeType），允许在 `.clip` 文件中通过指令直接生成字符画，例如 `[char-art "Hello"]`。
- **[ ] 背景色支持**: 添加 `[bgcolor RRGGBB]` 指令来设置终端的背景颜色。
- **[ ] 音频支持 (初步)**: 探索在特定时间戳触发简单的蜂鸣声或提示音的可能性。
- **[ ] 循环与跳转**: 引入 `[label name]` 和 `[goto name count]` 指令，以实现动画片段的循环播放。



## 🤝 贡献 (Contributing)



欢迎提交 Issue 和 Pull Request！如果您有任何想法或建议，请随时开一个 Issue 进行讨论。



## 📄 许可 (License)



本项目采用 Apache 2.0 许可。