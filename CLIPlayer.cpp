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

#define MA_ENABLE_MP3
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// 指令类型枚举
enum class CommandType {
    PRINT_TEXT, NEWLINE, NEWLINE_NO_PROMPT, CLEAR_SCREEN, MOVE_CURSOR, SPACE,
    STYLE_BOLD, STYLE_ITALIC, STYLE_UNDERLINE, STYLE_STRIKETHROUGH, STYLE_RESET, COLOR_RGB, BACKGROUND_RGB
};

// 播放指令的数据结构
struct PlaybackAction {
    int sourceLineNumber; std::chrono::milliseconds timestamp; CommandType type;
    std::string text_payload; int cursor_row; int cursor_col; int r = 0, g = 0, b = 0, a = 0;
};

// 字符串替换辅助函数
void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

// 终端控制函数
void enableAnsiSupport() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE); if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0; if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; SetConsoleMode(hOut, dwMode);
#endif
}

struct AudioPlayer {
    ma_engine engine;
    bool initialized = false;

    AudioPlayer(const std::string& music_path) {
        ma_result result = ma_engine_init(NULL, &engine);
        if(result != MA_SUCCESS) {
            std::cerr << "警告: 初始化音频引擎失败, code: " << result << std::endl;
            return;
        }

        result = ma_engine_play_sound(&engine, music_path.c_str(), NULL);
        if(result != MA_SUCCESS) {
            std::cerr << "警告: 播放音频文件 '" << music_path << "' 失败, code: " << result << std::endl;
            ma_engine_uninit(&engine); // 初始化成功但播放失败，需要清理
            return;
        }

        initialized = true;
    }

    ~AudioPlayer() {
        if (initialized) {
            ma_engine_uninit(&engine);
            std::cout << "音频引擎已关闭。" << std::endl;
        }
    }
};

void clearScreen() { std::cout << "\033[2J\033[H" << std::flush; }
void moveCursor(int row, int col) { std::cout << "\033[" << row << ";" << col << "H" << std::flush; }

// 文件解析函数
bool parseFile(const std::string& filename, std::vector<PlaybackAction>& actions, std::string& username) {
    std::ifstream file(filename);
    if (!file.is_open()) { std::cerr << "错误: 无法打开文件 '" << filename << "'" << std::endl; return false; }

    std::string line;
    int lineNumber = 0;
    std::chrono::milliseconds lastTimestamp(0);

    if (std::getline(file, line)) {
        lineNumber++;
        if (line.rfind("[username]", 0) == 0) username = line.substr(10);
        else { std::cerr << "错误: 文件第一行必须以 '[username]' 开头。" << std::endl; return false; }
    }

    const std::string ESC_OPEN_BRACKET_PLACEHOLDER = "\x01\x01";
    const std::string ESC_CLOSE_BRACKET_PLACEHOLDER = "\x02\x02";
    const std::string WHITESPACE = " \t\n\r\v\f";

    while (std::getline(file, line)) {
        lineNumber++;

        if(!line.empty() && line.back() =='\r') {
            line.pop_back();
        }
        if (line.empty() || line.rfind("//", 0) == 0) continue;
        
        replaceAll(line, "&[", ESC_OPEN_BRACKET_PLACEHOLDER);
        replaceAll(line, "&]", ESC_CLOSE_BRACKET_PLACEHOLDER);

        size_t first_bracket = line.find('[');
        size_t first_closing_bracket = line.find(']');
        if (first_bracket != 0 || first_closing_bracket == std::string::npos) {
            std::cerr << "错误: 第 " << lineNumber << " 行时间戳格式错误。" << std::endl;
            continue;
        }

        std::string timestamp_str = line.substr(first_bracket + 1, first_closing_bracket - 1);
        // int minutes, seconds, milliseconds;
        // // [修正] 使用 sscanf 并匹配正确的 `.` 分隔符格式
        // if (sscanf(timestamp_str.c_str(), "%d.%d.%d", &minutes, &seconds, &milliseconds) != 3) {
        //     std::cerr << "错误: 第 " << lineNumber << " 行时间戳解析失败。应为 [mm.ss.zzz] 格式。" << std::endl;
        std::istringstream ts_iss(timestamp_str);
        int minutes = 0, seconds = 0, milliseconds = 0;
        char dot1 = 0, dot2 = 0;

        ts_iss >> minutes >> dot1 >> seconds >> dot2 >> milliseconds;
        if(ts_iss.fail() || dot1!='.' || dot2!='.'){
            std::cerr << "错误: 第 " << lineNumber << " 行时间戳解析失败。应为 [mm.ss.zzz] 格式，实际为 '[" << timestamp_str << "]'。" << std::endl;
            continue;
        }

        auto currentTimestamp = std::chrono::minutes(minutes) + std::chrono::seconds(seconds) + std::chrono::milliseconds(milliseconds);
        if (currentTimestamp < lastTimestamp) { std::cerr << "错误: 【防乱轴】..." << std::endl; return false; }
        lastTimestamp = currentTimestamp;

        std::string remaining = line.substr(first_closing_bracket + 1);
        
        size_t textStart = 0;
        while(textStart < remaining.length()) {
            size_t commandStart = remaining.find('[', textStart);
            if (commandStart == std::string::npos) {
                std::string text = remaining.substr(textStart);
                if (text.find_first_not_of(WHITESPACE) != std::string::npos) {
                    replaceAll(text, ESC_OPEN_BRACKET_PLACEHOLDER, "[");
                    replaceAll(text, ESC_CLOSE_BRACKET_PLACEHOLDER, "]");
                    actions.push_back({lineNumber, currentTimestamp, CommandType::PRINT_TEXT, text});
                }
                break;
            }
            if (commandStart > textStart) {
                std::string text = remaining.substr(textStart, commandStart - textStart);
                if (text.find_first_not_of(WHITESPACE) != std::string::npos) {
                    replaceAll(text, ESC_OPEN_BRACKET_PLACEHOLDER, "[");
                    replaceAll(text, ESC_CLOSE_BRACKET_PLACEHOLDER, "]");
                    actions.push_back({lineNumber, currentTimestamp, CommandType::PRINT_TEXT, text});
                }
            }

            size_t commandEnd = remaining.find(']', commandStart);
            if (commandEnd == std::string::npos) { std::cerr << "错误: 第 " << lineNumber << " 行指令格式错误..." << std::endl; return false; }
            std::string command = remaining.substr(commandStart + 1, commandEnd - commandStart - 1);
            
            if (command == "newline") actions.push_back({lineNumber, currentTimestamp, CommandType::NEWLINE});
            else if (command == "newlinenp") actions.push_back({lineNumber, currentTimestamp, CommandType::NEWLINE_NO_PROMPT});
            else if (command == "clear") actions.push_back({lineNumber, currentTimestamp, CommandType::CLEAR_SCREEN});
            else if (command == "space") actions.push_back({lineNumber, currentTimestamp, CommandType::SPACE});
            else if (command.rfind("mv ", 0) == 0) {
                std::string token; int r, c;
                std::istringstream cmd_iss(command);
                if (cmd_iss >> token >> r >> c) actions.push_back({lineNumber, currentTimestamp, CommandType::MOVE_CURSOR, "", r, c});
                else std::cerr << "警告: 第 " << lineNumber << " 行: [mv] 指令参数格式错误，将被忽略。" << std::endl;
            }
            else if (command == "bold") actions.push_back({lineNumber, currentTimestamp, CommandType::STYLE_BOLD});
            else if (command == "italic") actions.push_back({lineNumber, currentTimestamp, CommandType::STYLE_ITALIC});
            else if (command == "underline") actions.push_back({lineNumber, currentTimestamp, CommandType::STYLE_UNDERLINE});
            else if (command == "strikethrough") actions.push_back({lineNumber, currentTimestamp, CommandType::STYLE_STRIKETHROUGH});
            else if (command.rfind("color ", 0) == 0) {
                 std::string payload = command.substr(6);
                 if (payload == "default") actions.push_back({lineNumber, currentTimestamp, CommandType::STYLE_RESET});
                 else {
                     if (payload.length() != 6 || payload.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) { std::cerr << "错误: 第 " << lineNumber << " 行颜色代码 '" << payload << "' 格式错误..." << std::endl; return false; }
                     try { int r = std::stoi(payload.substr(0, 2), nullptr, 16); int g = std::stoi(payload.substr(2, 2), nullptr, 16); int b = std::stoi(payload.substr(4, 2), nullptr, 16); actions.push_back({lineNumber, currentTimestamp, CommandType::COLOR_RGB, "", 0, 0, r, g, b}); } catch (const std::exception&) { std::cerr << "错误: 第 " << lineNumber << " 行颜色代码 '" << payload << "' 转换失败。" << std::endl; return false; }
                 }
            } else if (command.rfind("background ",0) == 0){
                std::string payload = command.substr(11);
                if(payload.length() != 8 || payload.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                    std::cerr << "错误: 第 " << lineNumber << " 行背景颜色代码 '" << payload << "' 格式错误。应为8位十六进制 rrggbbaa。" << std::endl;
                    return false;
                }
                try {
                    int r = std::stoi(payload.substr(0,2),nullptr,16);
                    int g = std::stoi(payload.substr(2,2),nullptr,16);
                    int b = std::stoi(payload.substr(4,2),nullptr,16);
                    int a = std::stoi(payload.substr(6,2),nullptr,16);
                    actions.push_back({lineNumber, currentTimestamp, CommandType::BACKGROUND_RGB, "", 0,0,r,g,b,a});
                } catch (const std::exception&) {
                    std::cerr << "错误: 第 " << lineNumber << " 行背景颜色代码 '" << payload << "' 转换失败。" << std::endl;
                    return false;
                }
            }
            else if (command.rfind("size ", 0) == 0) std::cerr << "警告: 第 " << lineNumber << " 行：[size] 指令不被支持，将被忽略。" << std::endl;
            textStart = commandEnd + 1;
        }
    }
    return true;
}

// 指令执行函数
void executeAction(const PlaybackAction& action, const std::string& username) {
    switch (action.type) {
        case CommandType::PRINT_TEXT: std::cout << action.text_payload << std::flush; break;
        case CommandType::NEWLINE: std::cout << '\n' << username << "> " << std::flush; break;
        case CommandType::NEWLINE_NO_PROMPT: std::cout << '\n'; break;
        case CommandType::CLEAR_SCREEN: clearScreen(); break;
        case CommandType::MOVE_CURSOR: moveCursor(action.cursor_row, action.cursor_col); break;
        case CommandType::SPACE: std::cout << ' ' << std::flush; break;
        case CommandType::STYLE_BOLD: std::cout << "\033[1m"; break;
        case CommandType::STYLE_ITALIC: std::cout << "\033[3m"; break;
        case CommandType::STYLE_UNDERLINE: std::cout << "\033[4m"; break;
        case CommandType::STYLE_STRIKETHROUGH: std::cout << "\033[9m"; break;
        case CommandType::STYLE_RESET: std::cout << "\033[0m"; break;
        case CommandType::COLOR_RGB: std::cout << "\033[38;2;" << action.r << ";" << action.g << ";" << action.b << "m"; break;
        case CommandType::BACKGROUND_RGB: std::cout << "\033[48;2;" << action.r << ";" << action.g << ";" << action.b << "m"; break;
    }
}

// 播放主函数
void play(const std::vector<PlaybackAction>& actions, const std::string& username) {
    auto startTime = std::chrono::steady_clock::now();
    for (size_t i = 0; i < actions.size(); ++i) {
        const auto& currentAction = actions[i];
        auto targetTime = startTime + currentAction.timestamp;
        std::this_thread::sleep_until(targetTime);

        auto beforeExecute = std::chrono::steady_clock::now();
        executeAction(currentAction, username);
        auto afterExecute = std::chrono::steady_clock::now();
        auto executionDuration = afterExecute - beforeExecute;

        if (i + 1 < actions.size()) {
            const auto& nextAction = actions[i + 1];
            auto timeUntilNext = nextAction.timestamp - currentAction.timestamp;
            if (timeUntilNext.count() > 0 && executionDuration > timeUntilNext) {
                auto overTime = std::chrono::duration_cast<std::chrono::microseconds>(executionDuration - timeUntilNext);
                std::cerr << "\n错误: 【防超时】在第 " << currentAction.sourceLineNumber
                          << " 行的动作执行超时！\n"
                          << "详情: 动作耗时 " << std::chrono::duration_cast<std::chrono::microseconds>(executionDuration).count() << " us, "
                          << "但距离下一个动作仅有 " << std::chrono::duration_cast<std::chrono::microseconds>(timeUntilNext).count() << " us。\n"
                          << "超时了 " << overTime.count() << " us。" << std::endl;
                return;
            }
        }
    }
}

// Windows 控制台配置函数
void configureWindowsConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);
#endif
}

// Main 函数
int main(int argc, char* argv[]) {
    // configureWindowsConsole();
    // if (argc != 2) { std::cerr << "使用方法: " << argv[0] << " <文件名.clip>" << std::endl; return 1; }
    // enableAnsiSupport();

    // std::vector<PlaybackAction> actions;
    // std::string username = "user@cliplayer";
    // std::string filename = argv[1];

    // if (!parseFile(filename, actions, username)) return 1;
    configureWindowsConsole();
    if (argc < 2) {std::cerr << "使用方法: " << argv[0] << " <文件名.clip> [--music <音频文件.mp3>]" << std::endl; return 1;}
    enableAnsiSupport();

    std::string filename = argv[1];
    std::string music_path;
    for(int i = 2;i<argc;++i) {
        if (std::string(argv[i]) == "--music" && i+1<argc) {
            music_path = argv[i+1];
            break;
        }
    }

    std::unique_ptr<AudioPlayer> player;
    if (!music_path.empty()) {
        player = std::make_unique<AudioPlayer>(music_path);
    }

    std::vector<PlaybackAction> actions;
    std::string username = "user@cliplayer";

    if(!parseFile(filename, actions, username)) return 1;

    clearScreen();
    std::cout << "\033[0m" << username << "> " << std::flush;
    play(actions, username);
    std::cout << "\033[0m";

    // std::cout << std::endl << "播放结束。" << std::endl;
    return 0;
}