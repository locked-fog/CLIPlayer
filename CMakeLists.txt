# CMake 最低版本要求。3.12 是一个比较安全的选择，它支持现代 C++ 标准设置。
cmake_minimum_required(VERSION 3.12)

# 定义项目名称、版本号和使用的语言 (CXX 代表 C++)
project(CLIPlayer VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 标准为 C++17。这是必须的，因为我们的代码使用了 C++17 的特性。
# CMAKE_CXX_STANDARD_REQUIRED ON 确保如果编译器不支持 C++17，配置会失败。
# CMAKE_CXX_EXTENSIONS OFF 建议关闭编译器特定的扩展（如 GNU 扩展），以获得更好的跨平台兼容性。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加可执行文件目标。
# 第一个参数 "CLIPlayer" 是生成的可执行文件的名称。
# 后续参数是用于编译该目标的所有源文件。
add_executable(CLIPlayer CLIPlayer.cpp)

# --- 库链接 ---
# 对于这个项目，我们不需要显式链接任何第三方库。
# 1. 在 Windows 上，`CLIPlayer.cpp` 中包含的 `<windows.h>` 所需的库（如 Kernel32.lib）
#    会被 MSVC 或 MinGW 的链接器自动处理。
# 2. 在 Linux/macOS 上，`<thread>` 可能依赖的 pthread 库，
#    现代 CMake 和编译器通常也能自动处理。
#
# 如果在一些非常旧的 Linux 系统上遇到关于 "pthread" 的链接错误，
# 可以取消下面几行代码的注释：
#
# if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
#   target_link_libraries(CLIPlayer PRIVATE Threads::Threads)
# endif()

# --- 安装指令 (可选) ---
# 如果您希望能够 "install" 这个程序到系统中，可以添加安装规则。
# 这对于简单的本地运行不是必需的。
#
# install(TARGETS CLIPlayer DESTINATION bin)