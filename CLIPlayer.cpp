#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <sstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

// 指令类型枚举
enum class CommandType {
    PRINT_TEXT,
    NEWLINE,
    NEWLINE_NO_PROMPT, // 新增：无提示符换行
    CLEAR_SCREEN,
    MOVE_CURSOR,
    SPACE,
    STYLE_BOLD,
    STYLE_ITALIC,
    STYLE_UNDERLINE,
    STYLE_STRIKETHROUGH,
    STYLE_RESET,
    COLOR_RGB
};

// 存储单条播放指令的数据结构
struct PlaybackAction {
    int sourceLineNumber;
    std::chrono::milliseconds timestamp;
    CommandType type;
    std::string text_payload;
    int cursor_row;
    int cursor_col;
    int r = 0, g = 0, b = 0;
};

// ========= 辅助函数 =========
// 字符串替换辅助函数
void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

// ========= 终端控制函数 =========
void enableAnsiSupport() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

void clearScreen() { std::cout << "\033[2J\033[H" << std::flush; }
void moveCursor(int row, int col) { std::cout << "\033[" << row << ";" << col << "H" << std::flush; }

// ========= 文件解析与执行 =========
bool parseFile(const std::string& filename, std::vector<PlaybackAction>& actions, std::string& username) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "错误: 无法打开文件 '" << filename << "'" << std::endl;
        return false;
    }

    std::string line;
    int lineNumber = 0;
    std::chrono::milliseconds lastTimestamp(0);

    if (std::getline(file, line)) {
        lineNumber++;
        if (line.rfind("[username]", 0) == 0) {
            username = line.substr(10);
        } else {
            std::cerr << "错误: 文件第一行必须以 '[username]' 开头。" << std::endl;
            return false;
        }
    }

    // [新增] 定义独特的占位符来处理转义
    const std::string ESC_OPEN_BRACKET_PLACEHOLDER = "\x01\x01";
    const std::string ESC_CLOSE_BRACKET_PLACEHOLDER = "\x02\x02";

    while (std::getline(file, line)) {
        lineNumber++;
        if (line.empty() || line.rfind("//", 0) == 0) continue;

        // [更新] 预处理转义字符
        replaceAll(line, "&[", ESC_OPEN_BRACKET_PLACEHOLDER);
        replaceAll(line, "&]", ESC_CLOSE_BRACKET_PLACEHOLDER);

        std::istringstream iss(line);
        char bracket;
        int minutes, seconds, milliseconds;
        char colon, dot;
        iss >> bracket;
        if (bracket != '[') { /* ... 错误处理 ... */ continue; }
        iss >> minutes >> colon >> seconds >> dot >> milliseconds >> bracket;
        if (iss.fail() || bracket != ']') { /* ... 错误处理 ... */ continue; }
        auto currentTimestamp = std::chrono::minutes(minutes) + std::chrono::seconds(seconds) + std::chrono::milliseconds(milliseconds);
        if (currentTimestamp < lastTimestamp) { /* ... 防乱轴错误处理 ... */ return false; }
        lastTimestamp = currentTimestamp;

        size_t currentPos = iss.tellg();
        std::string remaining = line.substr(currentPos);
        
        size_t textStart = 0;
        while(textStart < remaining.length()) {
            size_t commandStart = remaining.find('[', textStart);
            if (commandStart == std::string::npos) {
                std::string text = remaining.substr(textStart);
                replaceAll(text, ESC_OPEN_BRACKET_PLACEHOLDER, "[");
                replaceAll(text, ESC_CLOSE_BRACKET_PLACEHOLDER, "]");
                if (!text.empty()) actions.push_back({lineNumber, currentTimestamp, CommandType::PRINT_TEXT, text});
                break;
            }
            if (commandStart > textStart) {
                std::string text = remaining.substr(textStart, commandStart - textStart);
                replaceAll(text, ESC_OPEN_BRACKET_PLACEHOLDER, "[");
                replaceAll(text, ESC_CLOSE_BRACKET_PLACEHOLDER, "]");
                if (!text.empty()) actions.push_back({lineNumber, currentTimestamp, CommandType::PRINT_TEXT, text});
            }

            size_t commandEnd = remaining.find(']', commandStart);
            if (commandEnd == std::string::npos) { /* ... 错误处理 ... */ return false; }
            std::string command = remaining.substr(commandStart + 1, commandEnd - commandStart - 1);
            
            if (command == "newline") {
                actions.push_back({lineNumber, currentTimestamp, CommandType::NEWLINE});
            } else if (command == "newlinenp") { // 新增
                actions.push_back({lineNumber, currentTimestamp, CommandType::NEWLINE_NO_PROMPT});
            } else if (command == "clear") {
                actions.push_back({lineNumber, currentTimestamp, CommandType::CLEAR_SCREEN});
            } // ... (其他指令的解析保持不变)
            else if (command == "space") { actions.push_back({lineNumber, currentTimestamp, CommandType::SPACE}); }
            else if (command.rfind("mv ", 0) == 0) { int r, c; sscanf(command.c_str(), "mv %d %d", &r, &c); actions.push_back({lineNumber, currentTimestamp, CommandType::MOVE_CURSOR, "", r, c}); }
            else if (command == "bold") { actions.push_back({lineNumber, currentTimestamp, CommandType::STYLE_BOLD}); }
            else if (command == "italic") { actions.push_back({lineNumber, currentTimestamp, CommandType::STYLE_ITALIC}); }
            else if (command == "underline") { actions.push_back({lineNumber, currentTimestamp, CommandType::STYLE_UNDERLINE}); }
            else if (command == "strikethrough") { actions.push_back({lineNumber, currentTimestamp, CommandType::STYLE_STRIKETHROUGH}); }
            else if (command.rfind("color ", 0) == 0) {
                 std::string payload = command.substr(6);
                 if (payload == "default") { actions.push_back({lineNumber, currentTimestamp, CommandType::STYLE_RESET}); }
                 else {
                     if (payload.length() != 6 || payload.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) { /* ... 错误处理 ... */ return false; }
                     try { int r = std::stoi(payload.substr(0, 2), nullptr, 16); int g = std::stoi(payload.substr(2, 2), nullptr, 16); int b = std::stoi(payload.substr(4, 2), nullptr, 16); actions.push_back({lineNumber, currentTimestamp, CommandType::COLOR_RGB, "", 0, 0, r, g, b}); } catch (const std::exception&) { /* ... 错误处理 ... */ return false; }
                 }
            } else if (command.rfind("size ", 0) == 0) { std::cerr << "警告: 第 " << lineNumber << " 行：[size] 指令不被支持，将被忽略。" << std::endl; }
            textStart = commandEnd + 1;
        }
    }
    return true;
}

void executeAction(const PlaybackAction& action, const std::string& username) {
    switch (action.type) {
        case CommandType::PRINT_TEXT:
            std::cout << action.text_payload << std::flush;
            break;
        case CommandType::NEWLINE: // [更新] 输出提示符
            std::cout << '\n' << username << "> " << std::flush;
            break;
        case CommandType::NEWLINE_NO_PROMPT: // [新增]
            std::cout << '\n';
            break;
        case CommandType::CLEAR_SCREEN: clearScreen(); break;
        case CommandType::MOVE_CURSOR: moveCursor(action.cursor_row, action.cursor_col); break;
        case CommandType::SPACE: std::cout << ' ' << std::flush; break;
        case CommandType::STYLE_BOLD: std::cout << "\033[1m"; break;
        case CommandType::STYLE_ITALIC: std::cout << "\033[3m"; break;
        case CommandType::STYLE_UNDERLINE: std::cout << "\033[4m"; break;
        case CommandType::STYLE_STRIKETHROUGH: std::cout << "\033[9m"; break;
        case CommandType::STYLE_RESET: std::cout << "\033[0m"; break;
        case CommandType::COLOR_RGB: std::cout << "\033[38;2;" << action.r << ";" << action.g << ";" << action.b << "m"; break;
    }
}

void play(const std::vector<PlaybackAction>& actions, const std::string& username) {
    auto startTime = std::chrono::steady_clock::now();
    for (size_t i = 0; i < actions.size(); ++i) {
        const auto& currentAction = actions[i];
        auto targetTime = startTime + currentAction.timestamp;
        std::this_thread::sleep_until(targetTime);

        auto beforeExecute = std::chrono::steady_clock::now();
        executeAction(currentAction, username); // [更新] 传递 username
        auto afterExecute = std::chrono::steady_clock::now();
        auto executionDuration = afterExecute - beforeExecute;

        if (i + 1 < actions.size()) {
            // ... (防超时逻辑保持不变)
        }
    }
}

void configureWindowsConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

int main(int argc, char* argv[]) {
    configureWindowsConsole();
    if (argc != 2) { std::cerr << "使用方法: " << argv[0] << " <文件名.clip>" << std::endl; return 1; }
    enableAnsiSupport();

    std::vector<PlaybackAction> actions;
    std::string username = "user@cliplayer";
    std::string filename = argv[1];

    if (!parseFile(filename, actions, username)) return 1;
    
    clearScreen();
    std::cout << "\033[0m" << username << "> " << std::flush; // [更新] 初始显示一次提示符
    play(actions, username); // [更新] 传递 username
    std::cout << "\033[0m";

    std::cout << std::endl << "播放结束。" << std::endl;
    return 0;
}