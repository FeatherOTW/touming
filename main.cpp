#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <string>
#include <vector>
#include <set>
#include <gdiplus.h>
#include <time.h>

// 如果定义了__RESOURCE_ICON_ICO__宏，则定义IDI_ICON1标识符
#ifdef __RESOURCE_ICON_ICO__
#define IDI_ICON1 101
#endif

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// Window class name
const wchar_t g_szClassName[] = L"TransparentWindowClass";

// Menu item IDs
#define IDM_EXIT 1001
#define IDM_SETTINGS 1002
#define IDM_ABOUT 1003
#define IDM_SHOW 1004
#define IDM_LOCK 1005
#define IDM_TOPMOST 1006

// Window size
const int WINDOW_WIDTH = 640;
const int WINDOW_HEIGHT = 480;

// Settings window control IDs
#define IDC_SLIDER_OPACITY 1100
#define IDC_SLIDER_TEXT_OPACITY 1125
#define IDC_CHECK_TOPMOST 1101
#define IDC_BUTTON_BG_COLOR 1102
#define IDC_BUTTON_TEXT_COLOR 1103
#define IDC_COMBO_FONT 1104
#define IDC_COMBO_FONT_SIZE 1105
#define IDC_EDIT_WIDTH 1106
#define IDC_EDIT_HEIGHT 1107
#define IDC_EDIT_DISPLAY_TEXT 1108
#define IDC_BUTTON_OK 1111
#define IDC_PANEL_LEFT 1112
#define IDC_PANEL_RIGHT 1113
#define IDC_BUTTON_GENERAL 1114
#define IDC_BUTTON_LOAD 1115
#define IDC_BUTTON_APPLY 1116
#define IDC_BUTTON_STANDBY_DISPLAY 1117
#define IDC_EDIT_STANDBY_TEXT 1118
#define IDC_RADIO_HORIZONTAL_SCROLL 1119
#define IDC_RADIO_VERTICAL_SCROLL 1120
#define IDC_CHECK_TRANSPARENT_MODE 1126
// 单词卡相关ID
#define IDC_BUTTON_WORD_CARDS 1127
#define IDC_BUTTON_LOAD_CSV 1128
#define IDC_EDIT_CSV_PATH 1129
#define IDC_RADIO_SEQUENTIAL 1130
#define IDC_RADIO_RANDOM 1131
#define IDC_EDIT_INTERVAL 1132
#define IDC_CHECK_WORD_CARDS_ENABLED 1133

// Taskbar icon message
#define WM_TRAYMESSAGE (WM_USER + 1)

// Function prototypes
void ApplySettings(HWND hDialogWnd); // 应用设置函数
bool LoadWordCardsFromCsv(const std::wstring& filePath);
void DrawWordCard(Graphics& graphics, HWND hWnd);
void SwitchToNextWordCard();

// Global variables
HMENU g_hPopupMenu = NULL;
NOTIFYICONDATAW g_notifyIconData;
HFONT g_hFont = NULL;
HWND hButtonOK = NULL; // 确定按钮句柄

// 滚动相关的全局变量
int scrollX = 0;
int scrollY = 0;
SIZE textSize = {0};
static bool needCenterInit = true; // 是否需要初始化居中位置

// 单词卡相关全局变量
std::vector<std::vector<std::wstring>> wordCards; // 单词卡列表，每行是一个文本块，每列是一个段落
int currentCardIndex = 0; // 当前显示的卡片索引
DWORD64 lastCardChangeTime = 0; // 上次切换卡片的时间
bool isWordCardsMode = false; // 是否处于单词卡模式
bool needReloadWordCards = false; // 是否需要重新加载单词卡
std::vector<int> shuffledIndices; // 洗牌后的索引列表
int shuffledIndex = 0; // 当前在洗牌列表中的位置


// Settings
// 待机显示滚动类型枚举
enum class StandbyScrollType {
    HorizontalScroll = 0,  // 横向滚动
    VerticalScroll = 1,    // 竖向滚动
    Static = 2,            // 静止显示
};

struct WindowSettings {
    BYTE opacity; // 0-255
    BYTE textOpacity; // 0-255 文本透明度
    bool isTopmost;
    COLORREF bgColor;
    COLORREF textColor;
    std::wstring fontName;
    int fontSize;
    bool isBold;      // 粗体
    bool isItalic;    // 斜体
    bool isUnderline; // 下划线
    int width;
    int height;
    int xPos; // Window X position
    int yPos; // Window Y position
    bool isLocked;
    
    // 透明模式控制
    bool isTransparentBackground; // true: 使用可调节透明度的背景, false: 窗口底色完全透明（仅显示文本）
    
    // 待机显示相关设置
    std::wstring standbyDisplayText; // 待机显示文本
    StandbyScrollType standbyScrollType; // 滚动类型
    int scrollSpeed; // 滚动速度 (1-10)
    
    // 单词卡相关设置
    std::wstring wordCardsCsvPath; // CSV文件路径
    bool isWordCardsEnabled; // 是否启用单词卡功能
    bool isRandomOrder; // 是否随机播放
    int playIntervalSeconds; // 播放间隔时间（秒）
};

WindowSettings g_settings = {
    200, // opacity
    255, // textOpacity - 默认完全不透明
    true, // isTopmost
    RGB(255, 255, 255), // bgColor
    RGB(0, 0, 0), // textColor
    L"KaiTi", // fontName - 楷体更加圆润
    14, // fontSize
    false,  // isBold
    false,  // isItalic
    false,  // isUnderline
    WINDOW_WIDTH, // width
    WINDOW_HEIGHT, // height
    CW_USEDEFAULT, // xPos (default position)
    CW_USEDEFAULT, // yPos (default position)
    false, // isLocked
    false, // isTransparentBackground - 默认窗口底色完全透明（仅显示文本）
    L"待机模式 - 透明窗口应用", // standbyDisplayText
    StandbyScrollType::Static, // standbyScrollType - 默认静止显示
    2, // scrollSpeed - 默认滚动速度
    L"", // wordCardsCsvPath - 初始为空
    false, // isWordCardsEnabled - 默认不启用单词卡功能
    false, // isRandomOrder - 默认顺序播放
    5, // playIntervalSeconds - 默认5秒
};

// Function declarations
void CreateAppPopupMenu();
void CreateTaskbarIcon(HWND hWnd);
void RemoveTaskbarIcon();
void ApplyWindowSettings(HWND hWnd);
HWND CreateSettingsWindow(HWND hWndParent);
void LoadSettingsFromIni();
void SaveSettingsToIni();

// 字体枚举参数结构体
struct EnumParams {
    HWND comboBox;
    const wchar_t* currentFont;
    int* selectedIndex;
    std::set<std::wstring>* fontNames; // 用于去重的字体名称集合
};

// 字体枚举回调函数
int CALLBACK EnumFontProc(const LOGFONTW* lplf, const TEXTMETRICW* lptm, DWORD dwType, LPARAM lParam) {
    EnumParams* params = reinterpret_cast<EnumParams*>(lParam);
    
    // 只添加非空字体名称
    if (lstrlenW(lplf->lfFaceName) > 0) {
        std::wstring fontName(lplf->lfFaceName);
        
        // 检查是否已存在该字体（去重）
        if (params->fontNames->find(fontName) == params->fontNames->end()) {
            // 添加到集合中
            params->fontNames->insert(fontName);
            
            // 添加字体到下拉框
            int index = SendMessageW(params->comboBox, CB_ADDSTRING, 0, (LPARAM)lplf->lfFaceName);
            
            // 检查是否为当前字体，如果是则记录索引
            if (*params->selectedIndex == -1 && params->currentFont && 
                _wcsicmp(lplf->lfFaceName, params->currentFont) == 0) {
                *params->selectedIndex = index;
            }
        }
    }
    
    return 1; // 继续枚举
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Subwindow procedure callback function
LRESULT CALLBACK PanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

// WinMain function - 使用正确的函数声明格式
INT WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
) {
    // 初始化GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    // Load settings from INI file on program start
    LoadSettingsFromIni();
    
    // 初始化随机数生成器，使用当前时间作为种子，确保每次运行程序时随机顺序不同
    srand(time(NULL));

    // Register window class
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    // 加载图标，如果存在自定义图标则使用，否则使用默认图标
#ifdef __RESOURCE_ICON_ICO__
    wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    if (!wc.hIcon) {
#endif
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
#ifdef __RESOURCE_ICON_ICO__
    }
#endif
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    // 使用空背景刷以避免默认边框效果
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = g_szClassName;
    // 加载小图标，如果存在自定义图标则使用，否则使用默认图标
#ifdef __RESOURCE_ICON_ICO__
    wc.hIconSm = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    if (!wc.hIconSm) {
#endif
        wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
#ifdef __RESOURCE_ICON_ICO__
    }
#endif

    if (!RegisterClassEx(&wc)) {
        MessageBoxW(NULL, L"Window class registration failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Create window without title bar using saved position or default
    // 添加WS_EX_TOOLWINDOW样式使窗口不出现在Alt+Tab切换列表中
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | (g_settings.isTopmost ? WS_EX_TOPMOST : 0),
        g_szClassName,
        L"",
        WS_POPUP | WS_VISIBLE,
        g_settings.xPos, g_settings.yPos, g_settings.width, g_settings.height,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        MessageBoxW(NULL, L"Window creation failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Set initial window properties
    ApplyWindowSettings(hwnd);
    CreateAppPopupMenu();
    CreateTaskbarIcon(hwnd);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Save settings to INI file on program exit
    SaveSettingsToIni();
    
    RemoveTaskbarIcon();
    if (g_hPopupMenu) DestroyMenu(g_hPopupMenu);
    if (g_hFont) DeleteObject(g_hFont);
    
    // 关闭GDI+
    GdiplusShutdown(gdiplusToken);

    return (int)msg.wParam;
}

// Create right-click menu
void CreateAppPopupMenu() {
    g_hPopupMenu = CreatePopupMenu();
    AppendMenuW(g_hPopupMenu, MF_STRING, IDM_SHOW, L"显示窗口");
    AppendMenuW(g_hPopupMenu, MF_STRING, IDM_SETTINGS, L"设置");
    AppendMenuW(g_hPopupMenu, MF_STRING, IDM_LOCK, L"锁定窗口");
    // 添加窗口置顶菜单项
    UINT topmostMenuFlags = MF_STRING | (g_settings.isTopmost ? MF_CHECKED : MF_UNCHECKED);
    AppendMenuW(g_hPopupMenu, topmostMenuFlags, IDM_TOPMOST, L"窗口置顶");
    AppendMenuW(g_hPopupMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(g_hPopupMenu, MF_STRING, IDM_ABOUT, L"关于");
    AppendMenuW(g_hPopupMenu, MF_STRING, IDM_EXIT, L"退出");
}

// Create taskbar icon
void CreateTaskbarIcon(HWND hWnd) {
    ZeroMemory(&g_notifyIconData, sizeof(NOTIFYICONDATAW));
    g_notifyIconData.cbSize = sizeof(NOTIFYICONDATAW);
    g_notifyIconData.hWnd = hWnd;
    g_notifyIconData.uID = 100;
    g_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_notifyIconData.uCallbackMessage = WM_TRAYMESSAGE;
    // 加载任务栏图标，如果存在自定义图标则使用，否则使用默认图标
#ifdef __RESOURCE_ICON_ICO__
    g_notifyIconData.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    if (!g_notifyIconData.hIcon) {
#endif
        g_notifyIconData.hIcon = LoadIcon(NULL, IDI_APPLICATION);
#ifdef __RESOURCE_ICON_ICO__
    }
#endif
    lstrcpyW(g_notifyIconData.szTip, L"Transparent Window App");
    Shell_NotifyIconW(NIM_ADD, &g_notifyIconData);
}

// Remove taskbar icon
void RemoveTaskbarIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_notifyIconData);
    if (g_notifyIconData.hIcon) DestroyIcon(g_notifyIconData.hIcon);
}

// Load settings from INI file
void LoadSettingsFromIni() {
    wchar_t iniPath[MAX_PATH];
    GetModuleFileNameW(NULL, iniPath, MAX_PATH);
    wcscpy_s(wcsrchr(iniPath, L'.'), MAX_PATH - wcslen(iniPath), L".ini");

    // Read settings from INI file
    g_settings.opacity = (BYTE)GetPrivateProfileIntW(L"Settings", L"Opacity", g_settings.opacity, iniPath);
    g_settings.textOpacity = (BYTE)GetPrivateProfileIntW(L"Settings", L"TextOpacity", g_settings.textOpacity, iniPath);
    g_settings.isTopmost = GetPrivateProfileIntW(L"Settings", L"IsTopmost", g_settings.isTopmost, iniPath) != 0;
    g_settings.bgColor = GetPrivateProfileIntW(L"Settings", L"BgColor", g_settings.bgColor, iniPath);
    g_settings.textColor = GetPrivateProfileIntW(L"Settings", L"TextColor", g_settings.textColor, iniPath);
    
    // Read font name
    wchar_t fontName[256];
    GetPrivateProfileStringW(L"Settings", L"FontName", g_settings.fontName.c_str(), fontName, 256, iniPath);
    g_settings.fontName = fontName;
    
    g_settings.fontSize = GetPrivateProfileIntW(L"Settings", L"FontSize", g_settings.fontSize, iniPath);
    g_settings.width = GetPrivateProfileIntW(L"Settings", L"Width", g_settings.width, iniPath);
    g_settings.height = GetPrivateProfileIntW(L"Settings", L"Height", g_settings.height, iniPath);
    
    // Read window position
    g_settings.xPos = GetPrivateProfileIntW(L"Settings", L"XPos", g_settings.xPos, iniPath);
    g_settings.yPos = GetPrivateProfileIntW(L"Settings", L"YPos", g_settings.yPos, iniPath);
    
    // Read font style settings
    g_settings.isBold = GetPrivateProfileIntW(L"Settings", L"IsBold", g_settings.isBold, iniPath) != 0;
    g_settings.isItalic = GetPrivateProfileIntW(L"Settings", L"IsItalic", g_settings.isItalic, iniPath) != 0;
    g_settings.isUnderline = GetPrivateProfileIntW(L"Settings", L"IsUnderline", g_settings.isUnderline, iniPath) != 0;
    
    g_settings.isLocked = GetPrivateProfileIntW(L"Settings", L"IsLocked", g_settings.isLocked, iniPath) != 0;
    
    // 读取透明模式设置
    g_settings.isTransparentBackground = GetPrivateProfileIntW(L"Settings", L"IsTransparentBackground", g_settings.isTransparentBackground, iniPath) != 0;
    
    // Load standby display settings - 使用直接文件读取方法以支持多行文本
    // 尝试使用GetPrivateProfileStringW读取（向后兼容）
    wchar_t standbyText[1024];
    GetPrivateProfileStringW(L"Settings", L"StandbyDisplayText", g_settings.standbyDisplayText.c_str(), standbyText, 1024, iniPath);
    
    // 检查是否包含转义的换行符，如果没有，尝试直接从文件读取
    std::wstring tempText = standbyText;
    if (tempText.find(L"\\n") != std::wstring::npos) {
        // 包含转义的换行符，进行替换
        size_t pos = 0;
        while ((pos = tempText.find(L"\\n", pos)) != std::wstring::npos) {
            tempText.replace(pos, 2, L"\n");
            pos += 1; // 跳过替换的字符
        }
        g_settings.standbyDisplayText = tempText;
    } else {
        // 尝试直接从文件读取整个INI内容，手动解析
        HANDLE hFile = CreateFileW(iniPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hFile, nullptr);
            std::vector<char> fileData(fileSize + 1);
            DWORD bytesRead;
            if (ReadFile(hFile, fileData.data(), fileSize, &bytesRead, nullptr)) {
                fileData[bytesRead] = '\0';
                
                // 转换为宽字符串
                int wideSize = MultiByteToWideChar(CP_ACP, 0, fileData.data(), -1, nullptr, 0);
                std::vector<wchar_t> wideBuffer(wideSize);
                MultiByteToWideChar(CP_ACP, 0, fileData.data(), -1, wideBuffer.data(), wideSize);
                
                std::wstring fileContent(wideBuffer.data());
                
                // 手动解析StandbyDisplayText部分
                std::wstring sectionStart = L"[Settings]";
                std::wstring keyStart = L"StandbyDisplayText=";
                
                size_t sectionPos = fileContent.find(sectionStart);
                if (sectionPos != std::wstring::npos) {
                    size_t keyPos = fileContent.find(keyStart, sectionPos);
                    if (keyPos != std::wstring::npos) {
                        keyPos += keyStart.length();
                        
                        // 查找值的结束位置（下一个节或文件结束）
                        size_t endPos = fileContent.find(L"\n", keyPos);
                        if (endPos == std::wstring::npos) {
                            endPos = fileContent.length();
                        }
                        
                        // 提取值并处理多行情况
                        std::wstring value = fileContent.substr(keyPos, endPos - keyPos);
                        
                        // 检查并移除开头和结尾的引号（如果存在）
                        if (value.length() >= 2 && value[0] == '"' && value[value.length() - 1] == '"') {
                            value = value.substr(1, value.length() - 2);
                        }
                        
                        // 替换转义的换行符
                        size_t pos = 0;
                        while ((pos = value.find(L"\\n", pos)) != std::wstring::npos) {
                            value.replace(pos, 2, L"\n");
                            pos += 1;
                        }
                        
                        g_settings.standbyDisplayText = value;
                    }
                }
            }
            CloseHandle(hFile);
        }
    }
    
    int scrollType = GetPrivateProfileIntW(L"Settings", L"StandbyScrollType", (int)g_settings.standbyScrollType, iniPath);
    // Validate scroll type value
    if (scrollType >= (int)StandbyScrollType::HorizontalScroll && scrollType <= (int)StandbyScrollType::Static) {
        g_settings.standbyScrollType = (StandbyScrollType)scrollType;
    }
    
    // Load scroll speed
    g_settings.scrollSpeed = GetPrivateProfileIntW(L"Settings", L"ScrollSpeed", g_settings.scrollSpeed, iniPath);
    // Validate speed value (1-10)
    if (g_settings.scrollSpeed < 1) g_settings.scrollSpeed = 1;
    if (g_settings.scrollSpeed > 10) g_settings.scrollSpeed = 10;
    
    // Load word cards settings
    wchar_t csvPath[1024];
    GetPrivateProfileStringW(L"Settings", L"WordCardsCsvPath", g_settings.wordCardsCsvPath.c_str(), csvPath, 1024, iniPath);
    g_settings.wordCardsCsvPath = csvPath;
    
    g_settings.isRandomOrder = GetPrivateProfileIntW(L"Settings", L"IsRandomOrder", g_settings.isRandomOrder, iniPath) != 0;
    
    // 从INI加载间隔设置
    int loadedInterval = GetPrivateProfileIntW(L"Settings", L"PlayIntervalSeconds", g_settings.playIntervalSeconds, iniPath);
    g_settings.playIntervalSeconds = loadedInterval;
    
    g_settings.isWordCardsEnabled = GetPrivateProfileIntW(L"Settings", L"IsWordCardsEnabled", g_settings.isWordCardsEnabled, iniPath) != 0;
    
    // Validate interval value (1-300 seconds)
    if (g_settings.playIntervalSeconds < 1) g_settings.playIntervalSeconds = 1;
    if (g_settings.playIntervalSeconds > 300) g_settings.playIntervalSeconds = 300;
    
    
    
}

// Save settings to INI file
void SaveSettingsToIni() {
    wchar_t iniPath[MAX_PATH];
    GetModuleFileNameW(NULL, iniPath, MAX_PATH);
    wcscpy_s(wcsrchr(iniPath, L'.'), MAX_PATH - wcslen(iniPath), L".ini");

    // Write settings to INI file
    wchar_t buffer[32];
    swprintf_s(buffer, 32, L"%d", g_settings.opacity);
    WritePrivateProfileStringW(L"Settings", L"Opacity", buffer, iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.textOpacity);
    WritePrivateProfileStringW(L"Settings", L"TextOpacity", buffer, iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.isTopmost ? 1 : 0);
    WritePrivateProfileStringW(L"Settings", L"IsTopmost", buffer, iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.bgColor);
    WritePrivateProfileStringW(L"Settings", L"BgColor", buffer, iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.textColor);
    WritePrivateProfileStringW(L"Settings", L"TextColor", buffer, iniPath);
    
    WritePrivateProfileStringW(L"Settings", L"FontName", g_settings.fontName.c_str(), iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.fontSize);
    WritePrivateProfileStringW(L"Settings", L"FontSize", buffer, iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.width);
    WritePrivateProfileStringW(L"Settings", L"Width", buffer, iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.height);
    WritePrivateProfileStringW(L"Settings", L"Height", buffer, iniPath);
    
    // Save word cards settings
    WritePrivateProfileStringW(L"Settings", L"WordCardsCsvPath", g_settings.wordCardsCsvPath.c_str(), iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.isRandomOrder ? 1 : 0);
    WritePrivateProfileStringW(L"Settings", L"IsRandomOrder", buffer, iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.playIntervalSeconds);
    WritePrivateProfileStringW(L"Settings", L"PlayIntervalSeconds", buffer, iniPath);
    swprintf_s(buffer, 32, L"%d", g_settings.isWordCardsEnabled ? 1 : 0);
    WritePrivateProfileStringW(L"Settings", L"IsWordCardsEnabled", buffer, iniPath);
    
    // Save window position
    swprintf_s(buffer, 32, L"%d", g_settings.xPos);
    WritePrivateProfileStringW(L"Settings", L"XPos", buffer, iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.yPos);
    WritePrivateProfileStringW(L"Settings", L"YPos", buffer, iniPath);
    
    // Save font style settings
    swprintf_s(buffer, 32, L"%d", g_settings.isBold ? 1 : 0);
    WritePrivateProfileStringW(L"Settings", L"IsBold", buffer, iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.isItalic ? 1 : 0);
    WritePrivateProfileStringW(L"Settings", L"IsItalic", buffer, iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.isUnderline ? 1 : 0);
    WritePrivateProfileStringW(L"Settings", L"IsUnderline", buffer, iniPath);
    
    swprintf_s(buffer, 32, L"%d", g_settings.isLocked ? 1 : 0);
    WritePrivateProfileStringW(L"Settings", L"IsLocked", buffer, iniPath);
    
    // 保存透明模式设置
    swprintf_s(buffer, 32, L"%d", g_settings.isTransparentBackground ? 1 : 0);
    WritePrivateProfileStringW(L"Settings", L"IsTransparentBackground", buffer, iniPath);
    
    // Save standby display settings - 使用更可靠的方式保存多行文本
    // 对换行符进行转义
    std::wstring escapedText = g_settings.standbyDisplayText;
    size_t pos = 0;
    while ((pos = escapedText.find(L"\n", pos)) != std::wstring::npos) {
        escapedText.replace(pos, 1, L"\\n");
        pos += 2; // 跳过替换的字符
    }
    
    // 使用WritePrivateProfileStringW写入（基本支持）
    WritePrivateProfileStringW(L"Settings", L"StandbyDisplayText", escapedText.c_str(), iniPath);
    
    // 为了确保多行文本正确保存，我们使用直接文件操作进行修复
    HANDLE hFile = CreateFileW(iniPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD fileSize = GetFileSize(hFile, nullptr);
        std::vector<char> fileData(fileSize + 1);
        DWORD bytesRead;
        if (ReadFile(hFile, fileData.data(), fileSize, &bytesRead, nullptr)) {
            fileData[bytesRead] = '\0';
            
            // 转换为宽字符串进行处理
            int wideSize = MultiByteToWideChar(CP_ACP, 0, fileData.data(), -1, nullptr, 0);
            std::vector<wchar_t> wideBuffer(wideSize);
            MultiByteToWideChar(CP_ACP, 0, fileData.data(), -1, wideBuffer.data(), wideSize);
            
            std::wstring fileContent(wideBuffer.data());
            
            // 查找并替换StandbyDisplayText行
            std::wstring sectionStart = L"[Settings]";
            std::wstring keyStart = L"StandbyDisplayText=";
            
            size_t sectionPos = fileContent.find(sectionStart);
            if (sectionPos != std::wstring::npos) {
                size_t keyPos = fileContent.find(keyStart, sectionPos);
                if (keyPos != std::wstring::npos) {
                    // 查找当前值的结束位置
                    size_t endPos = fileContent.find(L"\n", keyPos);
                    if (endPos == std::wstring::npos) {
                        endPos = fileContent.length();
                    }
                    
                    // 构建新的键值对行，用引号包裹值以支持特殊字符
                    std::wstring newValue = keyStart + L"\"" + escapedText + L"\"";
                    
                    // 替换旧的键值对
                    fileContent.replace(keyPos, endPos - keyPos, newValue);
                    
                    // 转换回多字节字符串并写回文件
                    int multiSize = WideCharToMultiByte(CP_ACP, 0, fileContent.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    std::vector<char> multiBuffer(multiSize);
                    WideCharToMultiByte(CP_ACP, 0, fileContent.c_str(), -1, multiBuffer.data(), multiSize, nullptr, nullptr);
                    
                    // 写回文件
                    SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
                    SetEndOfFile(hFile);
                    DWORD bytesWritten;
                    WriteFile(hFile, multiBuffer.data(), multiSize - 1, &bytesWritten, nullptr);
                }
            }
        }
        CloseHandle(hFile);
    }
    swprintf_s(buffer, 32, L"%d", (int)g_settings.standbyScrollType);
    WritePrivateProfileStringW(L"Settings", L"StandbyScrollType", buffer, iniPath);
    
    
    // Save scroll speed
    swprintf_s(buffer, 32, L"%d", g_settings.scrollSpeed);
    WritePrivateProfileStringW(L"Settings", L"ScrollSpeed", buffer, iniPath);
    
    // Save float speed
    // 移除漂移速度相关设置
}

// Clear all controls in the right panel
void ClearRightPanel(HWND hPanelRight) {
    EnumChildWindows(hPanelRight, [](HWND hwndChild, LPARAM) -> BOOL {
        // 不删除确定和应用按钮，但这些按钮现在是设置窗口的直接子窗口，而不是面板的子窗口
        // 因此这个检查可能不再必要，但保留以防万一
        DWORD_PTR id = GetWindowLongPtr(hwndChild, GWLP_ID);
        if (id != IDC_BUTTON_OK && id != IDC_BUTTON_APPLY) {
            DestroyWindow(hwndChild);
        }
        return TRUE;
    }, 0);
    
    // 刷新面板，确保旧控件被完全清除
    InvalidateRect(hPanelRight, NULL, TRUE);
    UpdateWindow(hPanelRight);
}

// Show general settings panel
void ShowGeneralSettingsPanel(HWND hWnd, HWND hPanelRight) {
    ClearRightPanel(hPanelRight);
    
    // 确保在添加面板内容前，应用和确定按钮总是可见的
    
    // Create SimSun font for all labels
    HFONT hSimSunFontEarly = CreateFontW(
        11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"SimSun");
    
    // Store the font handle in window properties for cleanup later
    SetPropW(hWnd, L"SimSunFontEarly", (HANDLE)hSimSunFontEarly);
    
    // Create width label and edit box - 调整位置避免超出面板范围
    HWND hLabelWidth = CreateWindowW(L"STATIC", L"窗口宽度:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 20, 80, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelWidth, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hEditWidth = CreateWindowW(L"EDIT", NULL, 
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 
        100, 20, 80, 20, hPanelRight, (HMENU)IDC_EDIT_WIDTH, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hEditWidth, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    // 设置初始宽度值
    wchar_t widthText[10];
    swprintf_s(widthText, 10, L"%d", g_settings.width);
    SetWindowTextW(hEditWidth, widthText);
    
    // Create height label and edit box - 调整位置避免超出面板范围
    HWND hLabelHeight = CreateWindowW(L"STATIC", L"窗口高度:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 190, 20, 70, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelHeight, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hEditHeight = CreateWindowW(L"EDIT", NULL, 
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 
        270, 20, 70, 20, hPanelRight, (HMENU)IDC_EDIT_HEIGHT, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hEditHeight, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    // 设置初始高度值
    wchar_t heightText[10];
    swprintf_s(heightText, 10, L"%d", g_settings.height);
    SetWindowTextW(hEditHeight, heightText);
    
    // Create font name label and combo box - 调整大小避免超出面板范围
    HWND hLabelFont = CreateWindowW(L"STATIC", L"字体名称:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 60, 80, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelFont, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hComboFont = CreateWindowW(L"COMBOBOX", NULL, 
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_BORDER | WS_VSCROLL, 
        100, 60, 150, 100, hPanelRight, (HMENU)IDC_COMBO_FONT, 
        GetModuleHandle(NULL), NULL);
    // 设置下拉列表的最大显示高度，允许显示更多选项
    SendMessageW(hComboFont, CB_SETMINVISIBLE, (WPARAM)15, 0);  // 设置最多可见15个选项
    SendMessageW(hComboFont, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // 创建枚举参数和字体名称集合（用于去重）
    int selectedIndex = -1;
    std::set<std::wstring> fontNames;
    EnumParams params;
    
    params.comboBox = hComboFont;
    params.currentFont = g_settings.fontName.c_str();
    params.selectedIndex = &selectedIndex;
    params.fontNames = &fontNames;
    
    // 设置LOGFONT结构以枚举所有字体
    LOGFONTW logFont;
    ZeroMemory(&logFont, sizeof(LOGFONTW));
    // 使用DEFAULT_CHARSET并设置lfPitchAndFamily为0以枚举所有字体
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfPitchAndFamily = 0;  // 不限制字体的间距和字体系列
    
    // 调用枚举函数，使用0标志确保枚举所有字体
    // 通过logFont.lfPitchAndFamily = 0确保枚举所有字体类型
    EnumFontFamiliesExW(GetDC(NULL), &logFont, EnumFontProc, (LPARAM)&params, 0);
    
    // 如果找到了匹配的字体，设置选中状态；否则默认选择第一个字体
    if (selectedIndex >= 0) {
        SendMessageW(hComboFont, CB_SETCURSEL, selectedIndex, 0);
    } else {
        // 检查下拉框中是否有项目，如果有则选择第一个
        int count = SendMessageW(hComboFont, CB_GETCOUNT, 0, 0);
        if (count > 0) {
            SendMessageW(hComboFont, CB_SETCURSEL, 0, 0);
        }
    }
    
    // Create font size label and combo box - 调整位置避免超出面板范围
    HWND hLabelFontSize = CreateWindowW(L"STATIC", L"字体大小:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 260, 60, 60, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelFontSize, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hComboFontSize = CreateWindowW(L"COMBOBOX", NULL, 
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_BORDER, 
        330, 60, 50, 100, hPanelRight, (HMENU)IDC_COMBO_FONT_SIZE, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hComboFontSize, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // Add font size options
    const wchar_t* fontSizeOptions[] = { L"12", L"14", L"16", L"18", L"20", L"24", L"28", L"32" };
    for (int i = 0; i < 8; i++) {
        SendMessage(hComboFontSize, CB_ADDSTRING, 0, (LPARAM)fontSizeOptions[i]);
    }
    
    // Set current font size
    wchar_t fontSizeText[10];
    swprintf_s(fontSizeText, 10, L"%d", g_settings.fontSize);
    SetWindowTextW(hComboFontSize, fontSizeText);
    
    // Create opacity label and slider
    HWND hLabelOpacity = CreateWindowW(L"STATIC", L"窗口透明度:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 100, 80, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelOpacity, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hSliderOpacity = CreateWindowW(TRACKBAR_CLASSW, NULL, 
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_ENABLESELRANGE, 
        100, 100, 200, 30, hPanelRight, (HMENU)IDC_SLIDER_OPACITY, 
        GetModuleHandle(NULL), NULL);
    SendMessage(hSliderOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
    SendMessage(hSliderOpacity, TBM_SETPOS, TRUE, (LPARAM)g_settings.opacity);
    
    // Create text opacity label and slider
    HWND hLabelTextOpacity = CreateWindowW(L"STATIC", L"文本透明度:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 140, 80, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelTextOpacity, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hSliderTextOpacity = CreateWindowW(TRACKBAR_CLASSW, NULL, 
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_ENABLESELRANGE, 
        100, 140, 200, 30, hPanelRight, (HMENU)IDC_SLIDER_TEXT_OPACITY, 
        GetModuleHandle(NULL), NULL);
    SendMessage(hSliderTextOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
    SendMessage(hSliderTextOpacity, TBM_SETPOS, TRUE, (LPARAM)g_settings.textOpacity);
    
    // Create transparent mode checkbox
    HWND hCheckTransparentMode = CreateWindowW(L"BUTTON", L"显示背景色", 
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
        100, 180, 200, 20, hPanelRight, (HMENU)IDC_CHECK_TRANSPARENT_MODE, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hCheckTransparentMode, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    if (g_settings.isTransparentBackground) SendMessage(hCheckTransparentMode, BM_SETCHECK, BST_CHECKED, 0);
    
    // Create word cards enable checkbox
  
   
    
    // Create background color label and button
    HWND hLabelBgColor = CreateWindowW(L"STATIC", L"背景颜色:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 220, 80, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelBgColor, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hButtonBgColor = CreateWindowW(L"BUTTON", L"选择颜色", 
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 
        100, 220, 100, 25, hPanelRight, (HMENU)IDC_BUTTON_BG_COLOR, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hButtonBgColor, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // Create text color label and button - 调整位置避免超出面板范围
    HWND hLabelTextColor = CreateWindowW(L"STATIC", L"文本颜色:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 260, 80, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelTextColor, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hButtonTextColor = CreateWindowW(L"BUTTON", L"选择颜色", 
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 
        100, 260, 100, 25, hPanelRight, (HMENU)IDC_BUTTON_TEXT_COLOR, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hButtonTextColor, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // Create font style options - 调整位置避免超出面板范围
    HWND hLabelFontStyle = CreateWindowW(L"STATIC", L"字体样式:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 300, 100, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelFontStyle, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hCheckBold = CreateWindowW(L"BUTTON", L"粗体", 
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
        90, 300, 60, 20, hPanelRight, (HMENU)1120, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hCheckBold, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    if (g_settings.isBold) SendMessage(hCheckBold, BM_SETCHECK, BST_CHECKED, 0);
    
    HWND hCheckItalic = CreateWindowW(L"BUTTON", L"斜体", 
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
        160, 300, 60, 20, hPanelRight, (HMENU)1121, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hCheckItalic, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    if (g_settings.isItalic) SendMessage(hCheckItalic, BM_SETCHECK, BST_CHECKED, 0);
    
    HWND hCheckUnderline = CreateWindowW(L"BUTTON", L"下划线", 
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
        230, 300, 70, 20, hPanelRight, (HMENU)1122, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hCheckUnderline, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    if (g_settings.isUnderline) SendMessage(hCheckUnderline, BM_SETCHECK, BST_CHECKED, 0);
    
    // 按钮已移至SettingsWndProc的WM_CREATE中创建，此处不再重复创建
    
    // 保存控件句柄到全局变量 - 直接使用CreateWindowW返回值
    // 这里不需要再调用GetDlgItem，因为我们已经有了句柄
    
    // 将应用和确定按钮置于顶层，确保它们不会被面板内容覆盖
    // 使用局部变量避免命名冲突
    HWND btnApply = GetDlgItem(hWnd, IDC_BUTTON_APPLY);
    HWND btnOK = GetDlgItem(hWnd, IDC_BUTTON_OK);
    if (btnApply) SetWindowPos(btnApply, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    if (btnOK) SetWindowPos(btnOK, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

// 单词卡设置面板函数
void ShowWordCardsPanel(HWND hWnd, HWND hPanelRight) {
    ClearRightPanel(hPanelRight);
    
    // 确保在添加面板内容前，应用和确定按钮总是可见的
    
    // Create SimSun font for all labels
    HFONT hSimSunFontEarly = CreateFontW(
        11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"SimSun");
    
    // Store the font handle in window properties for cleanup later
    SetPropW(hWnd, L"SimSunFontWordCards", (HANDLE)hSimSunFontEarly);
    
    // 创建CSV文件路径标签
    HWND hLabelCsvPath = CreateWindowW(L"STATIC", L"单词卡CSV文件:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 20, 90, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelCsvPath, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // 创建CSV文件路径显示框
    HWND hEditCsvPath = CreateWindowW(L"EDIT", g_settings.wordCardsCsvPath.c_str(), 
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_AUTOHSCROLL, 
        110, 20, 200, 20, hPanelRight, (HMENU)IDC_EDIT_CSV_PATH, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hEditCsvPath, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // 创建加载CSV按钮
    HWND hButtonLoadCsv = CreateWindowW(L"BUTTON", L"浏览...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        320, 20, 60, 20, hPanelRight, (HMENU)IDC_BUTTON_LOAD_CSV, GetModuleHandle(NULL), NULL);
    SendMessageW(hButtonLoadCsv, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // 创建播放顺序标签
    HWND hLabelPlayOrder = CreateWindowW(L"STATIC", L"播放顺序:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 60, 80, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelPlayOrder, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // 创建顺序播放单选按钮
    HWND hRadioSequential = CreateWindowW(L"BUTTON", L"顺序播放", 
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 
        100, 60, 100, 20, hPanelRight, (HMENU)IDC_RADIO_SEQUENTIAL, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hRadioSequential, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // 创建随机播放单选按钮
    HWND hRadioRandom = CreateWindowW(L"BUTTON", L"随机播放", 
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 
        210, 60, 100, 20, hPanelRight, (HMENU)IDC_RADIO_RANDOM, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hRadioRandom, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // 根据当前设置选择单选按钮
    if (g_settings.isRandomOrder) {
        SendMessage(hRadioRandom, BM_SETCHECK, BST_CHECKED, 0);
    } else {
        SendMessage(hRadioSequential, BM_SETCHECK, BST_CHECKED, 0);
    }
    
    // 创建播放间隔标签
    HWND hLabelInterval = CreateWindowW(L"STATIC", L"播放间隔(秒):", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 100, 100, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelInterval, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // 创建播放间隔输入框
    wchar_t intervalStr[10];
    swprintf_s(intervalStr, 10, L"%d", g_settings.playIntervalSeconds);
    HWND hEditInterval = CreateWindowW(L"EDIT", intervalStr, 
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 
        100, 100, 60, 20, hPanelRight, (HMENU)IDC_EDIT_INTERVAL, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hEditInterval, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hCheckWordCardsEnabled = CreateWindowW(L"BUTTON", L"启用单词卡功能",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        100, 200, 200, 20, hPanelRight, (HMENU)IDC_CHECK_WORD_CARDS_ENABLED,
        GetModuleHandle(NULL), NULL);
    SendMessageW(hCheckWordCardsEnabled, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    if (g_settings.isWordCardsEnabled) SendMessage(hCheckWordCardsEnabled, BM_SETCHECK, BST_CHECKED, 0);


    
    // 将应用和确定按钮置于顶层，确保它们不会被面板内容覆盖
    HWND btnApply = GetDlgItem(hWnd, IDC_BUTTON_APPLY);
    HWND btnOK = GetDlgItem(hWnd, IDC_BUTTON_OK);
    if (btnApply) SetWindowPos(btnApply, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    if (btnOK) SetWindowPos(btnOK, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

// Show standby display settings panel
void ShowStandbyDisplayPanel(HWND hWnd, HWND hPanelRight) {
    ClearRightPanel(hPanelRight);
    
    // 确保在添加面板内容前，应用和确定按钮总是可见的
    
    // Create SimSun font for all labels
    HFONT hSimSunFontEarly = CreateFontW(
        11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"SimSun");
    
    // Store the font handle in window properties for cleanup later
    SetPropW(hWnd, L"SimSunFontStandby", (HANDLE)hSimSunFontEarly);
    
    // Create standby display text label
    HWND hLabelStandbyText = CreateWindowW(L"STATIC", L"待机显示文本:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 20, 100, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelStandbyText, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // Create multiline text box for standby display text
    HWND hEditStandbyText = CreateWindowW(L"EDIT", g_settings.standbyDisplayText.c_str(), 
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 
        120, 20, 260, 120, hPanelRight, (HMENU)IDC_EDIT_STANDBY_TEXT, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hEditStandbyText, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // Create scrolling type label
    HWND hLabelScrollType = CreateWindowW(L"STATIC", L"滚动方式:", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 160, 100, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelScrollType, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // Create radio buttons for scrolling options
    HWND hRadioHorizontal = CreateWindowW(L"BUTTON", L"横向滚动", 
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 
        120, 160, 130, 20, hPanelRight, (HMENU)IDC_RADIO_HORIZONTAL_SCROLL, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hRadioHorizontal, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hRadioVertical = CreateWindowW(L"BUTTON", L"竖向滚动", 
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 
        120, 190, 130, 20, hPanelRight, (HMENU)IDC_RADIO_VERTICAL_SCROLL, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hRadioVertical, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    HWND hRadioStatic = CreateWindowW(L"BUTTON", L"文本静止", 
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 
        120, 220, 130, 20, hPanelRight, (HMENU)1124, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hRadioStatic, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    

    
    // Create scroll speed label
    HWND hLabelScrollSpeed = CreateWindowW(L"STATIC", L"滚动速度 (1-10):", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 280, 100, 20, 
        hPanelRight, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(hLabelScrollSpeed, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
    // Create edit box for scroll speed
    wchar_t scrollSpeedStr[10];
    swprintf_s(scrollSpeedStr, 10, L"%d", g_settings.scrollSpeed);
    HWND hEditScrollSpeed = CreateWindowW(L"EDIT", scrollSpeedStr, 
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 
        120, 280, 60, 20, hPanelRight, (HMENU)1123, 
        GetModuleHandle(NULL), NULL);
    SendMessageW(hEditScrollSpeed, WM_SETFONT, (WPARAM)hSimSunFontEarly, TRUE);
    
 
    
    // 将应用和确定按钮置于顶层，确保它们不会被面板内容覆盖
    // 使用局部变量避免命名冲突
    HWND btnApply = GetDlgItem(hWnd, IDC_BUTTON_APPLY);
    HWND btnOK = GetDlgItem(hWnd, IDC_BUTTON_OK);
    if (btnApply) SetWindowPos(btnApply, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    if (btnOK) SetWindowPos(btnOK, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    
    // Set the selected radio button based on current settings
    switch (g_settings.standbyScrollType) {
    case StandbyScrollType::HorizontalScroll:
        SendMessage(hRadioHorizontal, BM_SETCHECK, BST_CHECKED, 0);
        break;
    case StandbyScrollType::VerticalScroll:
        SendMessage(hRadioVertical, BM_SETCHECK, BST_CHECKED, 0);
        break;
    case StandbyScrollType::Static:
        SendMessage(hRadioStatic, BM_SETCHECK, BST_CHECKED, 0);
        break;
    default:
        // 默认选择文本静止
        SendMessage(hRadioStatic, BM_SETCHECK, BST_CHECKED, 0);
        break;
    }
    
    // 按钮已移至SettingsWndProc的WM_CREATE中创建，此处不再重复创建
    
    // 保存编辑框句柄 - 直接使用CreateWindowW返回值
}

// Apply window settings
void ApplyWindowSettings(HWND hWnd) {
    // 根据透明模式设置窗口分层属性
    if (g_settings.isTransparentBackground) {
        // 透明背景模式：使用LWA_ALPHA设置窗口整体透明度
        // 背景透明度直接通过窗口设置，文本透明度在GDI+绘制时控制
        SetLayeredWindowAttributes(hWnd, 0, g_settings.opacity, LWA_ALPHA);
    } else {
        // 完全透明模式：只使用LWA_COLORKEY使白色区域透明
        // 背景和文本的透明度将在GDI+绘制时控制
        SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 0, LWA_COLORKEY);
    }
    
    // 通知窗口重绘以应用透明度变化
    InvalidateRect(hWnd, NULL, TRUE);

    // Set topmost
    SetWindowPos(hWnd, g_settings.isTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // Set window size while preserving position
    if (g_settings.xPos != CW_USEDEFAULT && g_settings.yPos != CW_USEDEFAULT) {
        SetWindowPos(hWnd, NULL, g_settings.xPos, g_settings.yPos, g_settings.width, g_settings.height, SWP_NOZORDER);
    } else {
        RECT rect;
        GetWindowRect(hWnd, &rect);
        SetWindowPos(hWnd, NULL, rect.left, rect.top, g_settings.width, g_settings.height, SWP_NOZORDER);
        // Update position settings if using default position
        g_settings.xPos = rect.left;
        g_settings.yPos = rect.top;
    }

    // Update font
    if (g_hFont) {
        DeleteObject(g_hFont);
    }
    g_hFont = CreateFontW(
        g_settings.fontSize,
        0,
        0,
        0,
        g_settings.isBold ? FW_BOLD : FW_NORMAL,
        g_settings.isItalic,
        g_settings.isUnderline,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        g_settings.fontName.c_str());
}

// Subwindow procedure callback function
LRESULT CALLBACK PanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBRUSH hBrush = (HBRUSH)dwRefData;
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, hBrush);
        EndPaint(hwnd, &ps);
        return (LRESULT)1;
    }
    else if (uMsg == WM_COMMAND) {
        // 转发WM_COMMAND消息到父窗口
        // 这使得右面板上的按钮点击事件能够被SettingsWndProc处理
        HWND hParent = GetParent(hwnd);
        if (hParent) {
            // 将控件ID保持不变，发送到父窗口
            SendMessage(hParent, WM_COMMAND, wParam, (LPARAM)hwnd);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// Create settings window
HWND CreateSettingsWindow(HWND hWndParent) {
    const wchar_t* settingsClassName = L"SettingsWindowClass";
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Register settings window class
    WNDCLASSEX wcSettings;
    ZeroMemory(&wcSettings, sizeof(WNDCLASSEX));
    wcSettings.cbSize = sizeof(WNDCLASSEX);
    wcSettings.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcSettings.lpfnWndProc = SettingsWndProc;
    wcSettings.cbClsExtra = 0;
    wcSettings.cbWndExtra = 0;
    wcSettings.hInstance = hInstance;
    // 加载设置窗口图标，如果存在自定义图标则使用，否则使用默认图标
#ifdef __RESOURCE_ICON_ICO__
    wcSettings.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    if (!wcSettings.hIcon) {
#endif
        wcSettings.hIcon = LoadIcon(NULL, IDI_APPLICATION);
#ifdef __RESOURCE_ICON_ICO__
    }
#endif
    wcSettings.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcSettings.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcSettings.lpszMenuName = NULL;
    wcSettings.lpszClassName = settingsClassName;
    // 加载设置窗口小图标，如果存在自定义图标则使用，否则使用默认图标
#ifdef __RESOURCE_ICON_ICO__
    wcSettings.hIconSm = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    if (!wcSettings.hIconSm) {
#endif
        wcSettings.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
#ifdef __RESOURCE_ICON_ICO__
    }
#endif

    if (!RegisterClassEx(&wcSettings)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            MessageBoxW(hWndParent, L"Settings window class registration failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
            return NULL;
        }
    }

    // Calculate position to center the settings window on screen
    const int windowWidth = 550;
    const int windowHeight = 480;
    
    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Calculate center position
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    // Create settings window with new position and increased size
    HWND hWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        settingsClassName,
        L"设置",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
        x, y, 550, 480, // Slightly increased size from 500x450 to 550x480
        hWndParent, NULL, hInstance, NULL);

    if (hWnd) {
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
    }

    return hWnd;
}

// Settings window message processing
LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 声明hWnd变量，确保在所有case标签中都可用
    HWND hWnd = hwnd;
    static HWND hSliderOpacity, hCheckTopmost, hButtonBgColor, hButtonTextColor;
    static HWND hComboFont, hComboFontSize, hEditWidth, hEditHeight, hEditDisplayText;
    static HWND hButtonOK, hButtonGeneral, hButtonLoad;
    static HWND hPanelLeft, hPanelRight;
    static HBRUSH hPanelLeftBrush;
    static HFONT hSimSunFont;

    switch (msg) {
    case WM_CREATE: {
        // Create left panel (dark blue background)
        hPanelLeftBrush = CreateSolidBrush(RGB(30, 64, 175));
        hPanelLeft = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
            0, 0, 150, 440, hwnd, (HMENU)IDC_PANEL_LEFT, GetModuleHandle(NULL), NULL);
        SetWindowLongPtr(hPanelLeft, GWLP_USERDATA, (LONG_PTR)hPanelLeftBrush);
        SetWindowSubclass(hPanelLeft, PanelProc, 1, (DWORD_PTR)hPanelLeftBrush);

        // Create navigation buttons in left panel
        hButtonGeneral = CreateWindowW(L"BUTTON", L"常规", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 20, 130, 30, hwnd, (HMENU)IDC_BUTTON_GENERAL, GetModuleHandle(NULL), NULL);
        
        // Add standby display button
        static HWND hButtonStandbyDisplay;
        hButtonStandbyDisplay = CreateWindowW(L"BUTTON", L"待机显示", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 60, 130, 30, hwnd, (HMENU)IDC_BUTTON_STANDBY_DISPLAY, GetModuleHandle(NULL), NULL);
        
        // 创建SimSun字体
        hSimSunFont = CreateFontW(
            11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"SimSun");
        
        // Add word cards button
        static HWND hButtonWordCards;
        hButtonWordCards = CreateWindowW(L"BUTTON", L"单词卡", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 100, 130, 30, hwnd, (HMENU)IDC_BUTTON_WORD_CARDS, GetModuleHandle(NULL), NULL);
        SendMessageW(hButtonWordCards, WM_SETFONT, (WPARAM)hSimSunFont, TRUE);
        
        // Create right panel
        hPanelRight = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
            150, 0, 400, 440, hwnd, (HMENU)IDC_PANEL_RIGHT, GetModuleHandle(NULL), NULL);
        
        // Apply subclass procedure to right panel for message forwarding
        SetWindowSubclass(hPanelRight, PanelProc, 2, (DWORD_PTR)GetStockObject(WHITE_BRUSH));

        // 字体已在上方创建，直接使用
        
        // Apply font to navigation buttons
        SendMessageW(hButtonGeneral, WM_SETFONT, (WPARAM)hSimSunFont, TRUE);
        SendMessageW(hButtonStandbyDisplay, WM_SETFONT, (WPARAM)hSimSunFont, TRUE);
        
        // Store the font handle in window properties for cleanup later
        SetPropW(hwnd, L"SimSunFont", (HANDLE)hSimSunFont);
        
        // 直接创建应用和确定按钮作为设置窗口的子窗口，而不是面板的子窗口
        // 这样按钮就会作用于整个设置菜单，并且在切换面板时不会被清除
        HWND hButtonApply = CreateWindowW(L"BUTTON", L"应用", 
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 
            260, 400, 80, 25, hwnd, (HMENU)IDC_BUTTON_APPLY, 
            GetModuleHandle(NULL), NULL);
        SendMessageW(hButtonApply, WM_SETFONT, (WPARAM)hSimSunFont, TRUE);
        
        // Create OK button and store in global variable
        hButtonOK = CreateWindowW(L"BUTTON", L"确定", 
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 
            350, 400, 80, 25, hwnd, (HMENU)IDC_BUTTON_OK, 
            GetModuleHandle(NULL), NULL);
        SendMessageW(hButtonOK, WM_SETFONT, (WPARAM)hSimSunFont, TRUE);
        
        // 初始显示常规设置面板
        ShowGeneralSettingsPanel(hwnd, hPanelRight);
        
        // 确保应用和确定按钮总是显示在最顶层，不被面板覆盖
        // 刷新按钮的顶层状态
        if (hButtonApply) SetWindowPos(hButtonApply, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        if (hButtonOK) SetWindowPos(hButtonOK, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        
        // 刷新窗口以确保所有控件正确显示
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
    }
    break;

    case WM_COMMAND: {
        // 处理左侧导航按钮点击事件
        if (LOWORD(wParam) == IDC_BUTTON_GENERAL) {
            HWND hPanelRight = GetDlgItem(hwnd, IDC_PANEL_RIGHT);
            ShowGeneralSettingsPanel(hwnd, hPanelRight);
            // 确保按钮在切换面板后仍显示在顶层
              HWND hLocalButtonApply = GetDlgItem(hwnd, IDC_BUTTON_APPLY);
              HWND hLocalButtonOK = GetDlgItem(hwnd, IDC_BUTTON_OK);
              // 先隐藏按钮
              if (hLocalButtonApply) ShowWindow(hLocalButtonApply, SW_HIDE);
              if (hLocalButtonOK) ShowWindow(hLocalButtonOK, SW_HIDE);
              // 刷新窗口
              InvalidateRect(hwnd, NULL, TRUE);
              UpdateWindow(hwnd);
              // 再重新显示并设置为顶层
              if (hLocalButtonApply) {
                  ShowWindow(hLocalButtonApply, SW_SHOW);
                  SetWindowPos(hLocalButtonApply, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
              }
              if (hLocalButtonOK) {
                  ShowWindow(hLocalButtonOK, SW_SHOW);
                  SetWindowPos(hLocalButtonOK, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
              }
        }
        else if (LOWORD(wParam) == IDC_BUTTON_STANDBY_DISPLAY) {
            HWND hPanelRight = GetDlgItem(hwnd, IDC_PANEL_RIGHT);
            ShowStandbyDisplayPanel(hwnd, hPanelRight);
            // 确保按钮在切换面板后仍显示在顶层
              HWND hLocalButtonApply = GetDlgItem(hwnd, IDC_BUTTON_APPLY);
              HWND hLocalButtonOK = GetDlgItem(hwnd, IDC_BUTTON_OK);
              // 先隐藏按钮
              if (hLocalButtonApply) ShowWindow(hLocalButtonApply, SW_HIDE);
              if (hLocalButtonOK) ShowWindow(hLocalButtonOK, SW_HIDE);
              // 刷新窗口
              InvalidateRect(hwnd, NULL, TRUE);
              UpdateWindow(hwnd);
              // 再重新显示并设置为顶层
              if (hLocalButtonApply) {
                  ShowWindow(hLocalButtonApply, SW_SHOW);
                  SetWindowPos(hLocalButtonApply, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
              }
              if (hLocalButtonOK) {
                  ShowWindow(hLocalButtonOK, SW_SHOW);
                  SetWindowPos(hLocalButtonOK, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
              }
        }
        else if (LOWORD(wParam) == IDC_BUTTON_WORD_CARDS) {
            HWND hPanelRight = GetDlgItem(hwnd, IDC_PANEL_RIGHT);
            ShowWordCardsPanel(hwnd, hPanelRight);
            // 确保按钮在切换面板后仍显示在顶层
              HWND hLocalButtonApply = GetDlgItem(hwnd, IDC_BUTTON_APPLY);
              HWND hLocalButtonOK = GetDlgItem(hwnd, IDC_BUTTON_OK);
              // 先隐藏按钮
              if (hLocalButtonApply) ShowWindow(hLocalButtonApply, SW_HIDE);
              if (hLocalButtonOK) ShowWindow(hLocalButtonOK, SW_HIDE);
              // 刷新窗口
              InvalidateRect(hwnd, NULL, TRUE);
              UpdateWindow(hwnd);
              // 再重新显示并设置为顶层
              if (hLocalButtonApply) {
                  ShowWindow(hLocalButtonApply, SW_SHOW);
                  SetWindowPos(hLocalButtonApply, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
              }
              if (hLocalButtonOK) {
                  ShowWindow(hLocalButtonOK, SW_SHOW);
                  SetWindowPos(hLocalButtonOK, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
              }
        }
        

        // 处理应用按钮点击事件 - 现在按钮是设置窗口的直接子窗口，但处理逻辑相同
        if (LOWORD(wParam) == IDC_BUTTON_APPLY) {
            ApplySettings(hwnd);
            return 0;
        }
        // 处理确定按钮点击事件 - 现在按钮是设置窗口的直接子窗口，但处理逻辑相同
        else if (LOWORD(wParam) == IDC_BUTTON_OK) {
            ApplySettings(hwnd);
            
            // Close settings window - 使用DestroyWindow确保窗口被正确销毁
            DestroyWindow(hwnd);
            return 0; // 提前返回，避免继续处理其他消息
        }
        else if (LOWORD(wParam) == IDC_BUTTON_LOAD_CSV) {
            // 处理CSV文件加载
            OPENFILENAMEW ofn;
            wchar_t szFile[260] = L"";
            
            // 初始化OPENFILENAME结构体
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
            
            // 打开文件选择对话框
            if (GetOpenFileNameW(&ofn) == TRUE) {
                // 更新文本框显示选中的文件路径
                HWND hEditCsvPath = GetDlgItem(GetDlgItem(hwnd, IDC_PANEL_RIGHT), IDC_EDIT_CSV_PATH);
                if (hEditCsvPath) {
                    SetWindowTextW(hEditCsvPath, szFile);
                    // 保存到全局设置
                    g_settings.wordCardsCsvPath = szFile;
                }
            }
        }
        else if (LOWORD(wParam) == IDC_BUTTON_BG_COLOR) {
            CHOOSECOLOR cc;
            static COLORREF acrCustClr[16] = { 0 };
            ZeroMemory(&cc, sizeof(CHOOSECOLOR));
            cc.lStructSize = sizeof(CHOOSECOLOR);
            cc.hwndOwner = hwnd;
            cc.lpCustColors = (LPDWORD)acrCustClr;
            cc.rgbResult = g_settings.bgColor;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;

            if (ChooseColor(&cc) == TRUE) {
                g_settings.bgColor = cc.rgbResult;
            }
        }
        else if (LOWORD(wParam) == IDC_BUTTON_TEXT_COLOR) {
            CHOOSECOLOR cc;
            static COLORREF acrCustClr[16] = { 0 };
            ZeroMemory(&cc, sizeof(CHOOSECOLOR));
            cc.lStructSize = sizeof(CHOOSECOLOR);
            cc.hwndOwner = hwnd;
            cc.lpCustColors = (LPDWORD)acrCustClr;
            cc.rgbResult = g_settings.textColor;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;

            if (ChooseColor(&cc) == TRUE) {
                g_settings.textColor = cc.rgbResult;
            }
        }
        else if (LOWORD(wParam) == IDC_CHECK_TRANSPARENT_MODE && HIWORD(wParam) == BN_CLICKED) {
            // 处理透明模式复选框状态变化
            HWND hCheckBox = (HWND)lParam;
            BOOL checked = SendMessage(hCheckBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_settings.isTransparentBackground = checked;
            
            // 实时应用窗口设置
            HWND hMainWnd = GetParent(hwnd);
            if (hMainWnd) {
                ApplyWindowSettings(hMainWnd);
            }
        }
    }
    break;

    case WM_HSCROLL: {
        // 获取设置窗口的父窗口（主窗口）
        HWND hMainWnd = GetParent(hwnd);
        if (!hMainWnd) break;
        
        // 检查是否是透明度滑块的消息
        if (LOWORD(wParam) == TB_THUMBTRACK || LOWORD(wParam) == TB_ENDTRACK) {
            // 检查控件ID
            HWND hSlider = (HWND)lParam;
            int ctrlID = GetDlgCtrlID(hSlider);
            
            if (ctrlID == IDC_SLIDER_OPACITY) {
                // 窗口透明度滑块 - 实时更新
                g_settings.opacity = (BYTE)SendMessage(hSlider, TBM_GETPOS, 0, 0);
                // 直接应用窗口设置
                ApplyWindowSettings(hMainWnd);
            }
            else if (ctrlID == IDC_SLIDER_TEXT_OPACITY) {
                // 文本透明度滑块 - 实时更新
                g_settings.textOpacity = (BYTE)SendMessage(hSlider, TBM_GETPOS, 0, 0);
                // 重绘窗口以立即反映文本透明度变化
                InvalidateRect(hMainWnd, NULL, TRUE);
                UpdateWindow(hMainWnd);
            }
        }
    }
    break;
    
    case WM_DESTROY: {
        // Clean up resources
        RemoveWindowSubclass(hPanelLeft, PanelProc, 1);
        if (hPanelRight) RemoveWindowSubclass(hPanelRight, PanelProc, 2);
        if (hPanelLeftBrush) DeleteObject(hPanelLeftBrush);
        // Clean up SimSun font resource
        HFONT hSimSunFontEarly = (HFONT)GetPropW(hwnd, L"SimSunFontEarly");
        if (hSimSunFontEarly) {
            DeleteObject(hSimSunFontEarly);
            RemovePropW(hwnd, L"SimSunFontEarly");
        }
    }
    break;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// ApplySettings函数实现 - 应用设置并刷新窗口
// 注意：应用和确定按钮现在是设置窗口的直接子窗口，而不是面板的子窗口
// 这样可以确保按钮在切换设置面板时仍然可见并且作用于整个设置菜单
void ApplySettings(HWND hDialogWnd) {
    HWND hPanelRight = GetDlgItem(hDialogWnd, IDC_PANEL_RIGHT);
    
    // 检查是否在单词卡面板
    HWND hEditCsvPath = GetDlgItem(hPanelRight, IDC_EDIT_CSV_PATH);
    bool isWordCardsPanel = (hEditCsvPath && IsWindow(hEditCsvPath));
    
    if (isWordCardsPanel) {
        // 保存单词卡启用状态
        BOOL isChecked = IsDlgButtonChecked(hPanelRight, IDC_CHECK_WORD_CARDS_ENABLED);
        g_settings.isWordCardsEnabled = (isChecked == BST_CHECKED);
        
        // 保存单词卡设置
        wchar_t csvPathBuffer[1024] = {0};
        GetWindowTextW(hEditCsvPath, csvPathBuffer, 1024);
        g_settings.wordCardsCsvPath = csvPathBuffer;
        
        // 保存播放顺序设置
        g_settings.isRandomOrder = (IsDlgButtonChecked(hPanelRight, IDC_RADIO_RANDOM) == BST_CHECKED);
        
        // 保存播放间隔设置
        HWND hEditInterval = GetDlgItem(hPanelRight, IDC_EDIT_INTERVAL);
        if (hEditInterval && IsWindow(hEditInterval)) {
            wchar_t intervalStr[10] = {0};
            GetWindowTextW(hEditInterval, intervalStr, 10);
            int interval = _wtoi(intervalStr);
            // 限制间隔在1-300秒之间
            interval = max(1, min(interval, 300));
            g_settings.playIntervalSeconds = interval;
            
            // 更新显示的间隔值
            swprintf_s(intervalStr, 10, L"%d", interval);
            SetWindowTextW(hEditInterval, intervalStr);
        }
        
        needReloadWordCards = true;
        
        // 获取主窗口句柄 - 使用FindWindowW通过窗口类名查找，更加可靠
        HWND hMainWnd = FindWindowW(L"TransparentWindowClass", NULL);
        
        // 应用设置到窗口和定时器
        // 管理单词卡定时器 - 无论当前状态如何，先销毁旧定时器
        if (hMainWnd && IsWindow(hMainWnd)) {
            // 确保旧定时器被销毁
            KillTimer(hMainWnd, 2);
            
            // 计算新的时间间隔（从用户输入更新后的设置值）
            DWORD intervalMs = g_settings.playIntervalSeconds * 1000;
            
            // 如果单词卡功能已启用且CSV路径有效，则创建新定时器
            if (g_settings.isWordCardsEnabled && !g_settings.wordCardsCsvPath.empty()) {
                // 强制重新创建定时器，确保使用新的间隔值
                // 先短暂延迟确保旧定时器完全销毁
                Sleep(10);
                
                // 创建新定时器并应用新的间隔值
                if (SetTimer(hMainWnd, 2, intervalMs, NULL)) {
                    // 重置切换时间，从设置应用后开始计时
                    lastCardChangeTime = GetTickCount64();
                }
            }
        }
    }
    else {
        // 检查是否在待机显示面板
        HWND hEditStandbyText = GetDlgItem(hPanelRight, IDC_EDIT_STANDBY_TEXT);
        if (hEditStandbyText && IsWindow(hEditStandbyText)) {
            // 保存待机显示设置
            wchar_t textBuffer[1024] = {0};
            GetWindowTextW(hEditStandbyText, textBuffer, 1024);
            g_settings.standbyDisplayText = textBuffer;
            
            // 保存滚动类型设置
            if (IsDlgButtonChecked(hPanelRight, IDC_RADIO_HORIZONTAL_SCROLL) == BST_CHECKED) {
                g_settings.standbyScrollType = StandbyScrollType::HorizontalScroll;
            } else if (IsDlgButtonChecked(hPanelRight, IDC_RADIO_VERTICAL_SCROLL) == BST_CHECKED) {
                g_settings.standbyScrollType = StandbyScrollType::VerticalScroll;
            } else if (IsDlgButtonChecked(hPanelRight, 1124) == BST_CHECKED) {
                g_settings.standbyScrollType = StandbyScrollType::Static;
            } else {
                // 默认保存为文本静止
                g_settings.standbyScrollType = StandbyScrollType::Static;
            }
            
            // 保存斜向角度设置
            
            // 保存滚动速度设置
            HWND hEditScrollSpeed = GetDlgItem(hPanelRight, 1123);
            if (hEditScrollSpeed && IsWindow(hEditScrollSpeed)) {
                wchar_t speedStr[10] = {0};
                GetWindowTextW(hEditScrollSpeed, speedStr, 10);
                int speed = _wtoi(speedStr);
                speed = max(1, min(speed, 10));
                g_settings.scrollSpeed = speed;
            }
        }
        else {
            // 保存常规设置
            HWND hSlider = GetDlgItem(hPanelRight, IDC_SLIDER_OPACITY);
            if (hSlider && IsWindow(hSlider)) {
                g_settings.opacity = (BYTE)SendMessage(hSlider, TBM_GETPOS, 0, 0);
            }
            
            // 保存文本透明度设置
            HWND hTextOpacitySlider = GetDlgItem(hPanelRight, 1125);
            if (hTextOpacitySlider && IsWindow(hTextOpacitySlider)) {
                g_settings.textOpacity = (BYTE)SendMessage(hTextOpacitySlider, TBM_GETPOS, 0, 0);
            }
            
            // 保存透明模式设置
            g_settings.isTransparentBackground = (IsDlgButtonChecked(hPanelRight, IDC_CHECK_TRANSPARENT_MODE) == BST_CHECKED);
            
            // 字体名称设置
            HWND hComboFontWnd = GetDlgItem(hPanelRight, IDC_COMBO_FONT);
            if (hComboFontWnd && IsWindow(hComboFontWnd)) {
                int fontIndex = SendMessage(hComboFontWnd, CB_GETCURSEL, 0, 0);
                if (fontIndex != CB_ERR) {
                    wchar_t fontName[256] = {0};
                    SendMessage(hComboFontWnd, CB_GETLBTEXT, fontIndex, (LPARAM)fontName);
                    g_settings.fontName = fontName;
                }
            }
            
            // 字体大小设置
            HWND hComboFontSizeWnd = GetDlgItem(hPanelRight, IDC_COMBO_FONT_SIZE);
            if (hComboFontSizeWnd && IsWindow(hComboFontSizeWnd)) {
                wchar_t fontSizeText[10] = {0};
                GetWindowTextW(hComboFontSizeWnd, fontSizeText, 10);
                int newFontSize = _wtoi(fontSizeText);
                newFontSize = max(6, min(newFontSize, 200));
                g_settings.fontSize = newFontSize;
                
                swprintf_s(fontSizeText, 10, L"%d", newFontSize);
                SetWindowTextW(hComboFontSizeWnd, fontSizeText);
            }
            
            // 保存字体样式设置
            g_settings.isBold = (IsDlgButtonChecked(hPanelRight, 1120) == BST_CHECKED);
            g_settings.isItalic = (IsDlgButtonChecked(hPanelRight, 1121) == BST_CHECKED);
            g_settings.isUnderline = (IsDlgButtonChecked(hPanelRight, 1122) == BST_CHECKED);

            // 窗口大小设置
            HWND hEditWidthWnd = GetDlgItem(hPanelRight, IDC_EDIT_WIDTH);
            if (hEditWidthWnd && IsWindow(hEditWidthWnd)) {
                wchar_t widthText[10] = {0};
                GetWindowTextW(hEditWidthWnd, widthText, 10);
                g_settings.width = max(_wtoi(widthText), 200);
            }

            HWND hEditHeightWnd = GetDlgItem(hPanelRight, IDC_EDIT_HEIGHT);
            if (hEditHeightWnd && IsWindow(hEditHeightWnd)) {
                wchar_t heightText[10] = {0};
                GetWindowTextW(hEditHeightWnd, heightText, 10);
                g_settings.height = max(_wtoi(heightText), 100);
            }
        }
    }
    
    // 应用设置到主窗口
    HWND hMainWnd = FindWindowW(L"TransparentWindowClass", NULL);
    if (hMainWnd && IsWindow(hMainWnd)) {
        ApplyWindowSettings(hMainWnd);
        scrollX = scrollY = 0;
        needCenterInit = true;
        InvalidateRect(hMainWnd, NULL, FALSE);
        UpdateWindow(hMainWnd);
    }

    // 保存设置到INI文件
    SaveSettingsToIni();
}



// Main window message processing
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static bool isDragging = false;
    static POINT dragOffset;
    // 声明hWnd变量在switch语句之前，确保在所有case标签中都可用
    HWND hWnd = hwnd;

    switch (msg) {
    case WM_ERASEBKGND:
        // 返回1以阻止默认的背景擦除，这是解决闪烁问题的关键
        return 1;

    case WM_CREATE:
        // 启动定时器用于滚动效果
        SetTimer(hwnd, 1, 50, NULL);
        
        // 如果单词卡功能已启用，创建单词卡定时器
        if (g_settings.isWordCardsEnabled && !g_settings.wordCardsCsvPath.empty()) {
            // 计算时间间隔 - 使用当前设置的playIntervalSeconds值
            DWORD intervalMs = g_settings.playIntervalSeconds * 1000;
            
            // 创建定时器 - 确保使用正确的间隔值
            SetTimer(hwnd, 2, intervalMs, NULL);
            lastCardChangeTime = GetTickCount64();
        }
        break;

    case WM_TIMER:
        // 处理单词卡定时器（ID为2）
        if (wParam == 2 && g_settings.isWordCardsEnabled && !g_settings.wordCardsCsvPath.empty()) {

            
            // 切换到下一张单词卡
            SwitchToNextWordCard();
            // 更新窗口
            InvalidateRect(hWnd, NULL, FALSE);
            UpdateWindow(hWnd);
        } else if (wParam == 1) {
            // 更新滚动位置 - 使用InvalidateRect(NULL, FALSE)减少闪烁
            if (g_settings.standbyScrollType == StandbyScrollType::HorizontalScroll) {
                scrollX -= g_settings.scrollSpeed;
                // 重置滚动位置以实现循环效果 - 确保文本完全移出窗口后再重置
                // 增加额外余量确保文本完全移出窗口，补偿GDI和GDI+尺寸计算差异
                if (scrollX + textSize.cx * 1.5 < 0) {
                  RECT rect;
                  GetClientRect(hWnd, &rect);
                  // 直接重置到窗口右侧，不显示两份文本
                  scrollX = rect.right - rect.left;
                }
                // 只更新必要的区域，减少闪烁
                // 计算需要更新的区域，包含文本和一些额外的边距
                RECT updateRect;
                GetClientRect(hWnd, &updateRect);
                updateRect.left = 0;
                updateRect.top = 0;

                // 使用FALSE参数避免擦除背景，这是减少闪烁的关键
                InvalidateRect(hWnd, &updateRect, FALSE);

                // 立即更新窗口，减少累积的闪烁
                UpdateWindow(hWnd);
            }
            else if (g_settings.standbyScrollType == StandbyScrollType::VerticalScroll) {
                scrollY -= g_settings.scrollSpeed;
                // 重置滚动位置以实现循环效果 - 确保文本完全移出窗口后再重置
                // 增加额外余量确保文本完全移出窗口，补偿GDI和GDI+尺寸计算差异
                if (scrollY + textSize.cy < 0) {  // 增加到3倍文本高度以确保完全移出窗口
                    RECT rect;
                    GetClientRect(hWnd, &rect);
                    // 从窗口底部开始向上滚动
                    scrollY = rect.bottom - rect.top;
                }
                // 只更新必要的文本区域
                RECT updateRect;
                GetClientRect(hWnd, &updateRect);
                updateRect.left = 0;
                updateRect.top = 0;

                // 避免全窗口擦除
                InvalidateRect(hWnd, &updateRect, FALSE);
                UpdateWindow(hWnd);
            }
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        
        // 获取整个客户区域进行绘制
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        int width = clientRect.right - clientRect.left;
        int height = clientRect.bottom - clientRect.top;
        
        // 创建普通内存DC和位图
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, width, height);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);
        
        // 确保背景绘制完全覆盖，避免边缘1像素边框
        RECT fillRect = { -1, -1, width + 1, height + 1 };

        // 使用GDI+实现分层绘制和透明度控制
        Graphics graphics(hdcMem);
        // 针对透明模式使用不同的渲染策略
        if (g_settings.isTransparentBackground) {
            // 在半透明背景上使用高质量渲染
            graphics.SetSmoothingMode(SmoothingModeHighQuality);
            graphics.SetTextRenderingHint(TextRenderingHintAntiAlias); // 在半透明背景上使用常规抗锯齿而非ClearType
        } else {
            // 在完全透明窗口上使用更适合的渲染设置
            graphics.SetSmoothingMode(SmoothingModeNone); // 禁用平滑以避免白色边缘
            graphics.SetTextRenderingHint(TextRenderingHintSingleBitPerPixelGridFit); // 使用无抗锯齿渲染以消除白色边缘
        }
        graphics.SetCompositingQuality(CompositingQualityHighQuality);
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        // 设置合成模式为SourceOver，确保透明度效果正确应用
        graphics.SetCompositingMode(CompositingModeSourceOver);


        // 根据透明模式决定背景绘制方式
        if (g_settings.isTransparentBackground) {
            // 透明模式：使用用户设置的背景色和透明度
            // 提取RGB颜色分量
            BYTE r = GetRValue(g_settings.bgColor);
            BYTE g = GetGValue(g_settings.bgColor);
            BYTE b = GetBValue(g_settings.bgColor);
            // 设置背景色和透明度
            Color bgColor(g_settings.opacity, r, g, b);
            SolidBrush bgBrush(bgColor);
            // 填充扩展的矩形区域，确保覆盖窗口边缘
            graphics.FillRectangle(&bgBrush, fillRect.left, fillRect.top, fillRect.right - fillRect.left, fillRect.bottom - fillRect.top);
        } else {
            // 非透明模式：填充为白色，通过颜色键使其透明
            SolidBrush whiteBrush(Color(255, 255, 255));
            // 填充扩展的矩形区域，确保覆盖窗口边缘
            graphics.FillRectangle(&whiteBrush, fillRect.left, fillRect.top, fillRect.right - fillRect.left, fillRect.bottom - fillRect.top);
        }

        // 检查单词卡功能是否已启用
        std::wstring textToDisplay;
        int textLen = 0;
        bool isWordCardMode = false;
        
        if (g_settings.isWordCardsEnabled && !g_settings.wordCardsCsvPath.empty()) {
            isWordCardMode = true;
            // 检查是否需要重新加载单词卡，或者单词卡数据为空时也尝试加载
            if (needReloadWordCards || wordCards.empty()) {
                bool loadSuccess = LoadWordCardsFromCsv(g_settings.wordCardsCsvPath);
                
                needReloadWordCards = false;

            }
            
            // 使用专门的DrawWordCard函数绘制单词卡内容
            if (!wordCards.empty()) {
                DrawWordCard(graphics, hWnd);
                textLen = 0; // 设置为0，跳过后续的常规文本绘制
            } else {
                // 如果单词卡数据仍然为空，显示错误信息
                Font font(L"SimSun", 16);
                SolidBrush brush(Color(255, 255, 0, 0)); // 红色文本
                StringFormat format;
                format.SetAlignment(StringAlignmentCenter);
                format.SetLineAlignment(StringAlignmentCenter);
                std::wstring errorMsg = L"单词卡加载失败，请检查CSV文件是否存在且格式正确";
                graphics.DrawString(errorMsg.c_str(), -1, &font, RectF(0, 0, width, height), &format, &brush);
                

                
                // 不执行后续文本绘制，设置标志并继续到清理部分
                textLen = 0;
            }
        } else {
            // 常规文本绘制模式
            // 获取待机文本内容
            if (g_settings.standbyDisplayText.empty()) {
                textToDisplay = L"";
            } else {
                textToDisplay = g_settings.standbyDisplayText;
                textLen = textToDisplay.length();
            }
        }
        
        // 存储字体
        if (g_hFont) {
            SelectObject(hdcMem, g_hFont);
        }
        
        // 计算文本大小 - 使用DrawTextW和DT_CALCRECT来正确计算多行文本的实际高度
        if (textLen > 0) {
            // 首先获取单行高度用于其他需要的地方
            GetTextExtentPoint32W(hdcMem, textToDisplay.c_str(), textLen, &textSize);
            
            // 然后计算多行文本的实际高度
            RECT textRect = {0, 0, clientRect.right - clientRect.left, 0};
            DrawTextW(hdcMem, textToDisplay.c_str(), textLen, &textRect, 
                DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_CALCRECT);
            // 更新textSize.cy为多行文本的实际高度
            textSize.cy = textRect.bottom - textRect.top;
        }
        
        // 居中初始化逻辑
        if (needCenterInit && textLen > 0) {
            GetClientRect(hWnd, &clientRect);
            int windowWidth = clientRect.right - clientRect.left;
            int windowHeight = clientRect.bottom - clientRect.top;

            // 预先声明变量，确保每个分支都能正确初始化
            int actualTextHeight = 0;
            int actualTextWidth = 0;
            RECT tempRect = clientRect;
            tempRect.left += 20;
            tempRect.right -= 20;
            
            // 根据不同滚动类型设置居中初始位置
            switch (g_settings.standbyScrollType) {
            case StandbyScrollType::HorizontalScroll:
                // 水平滚动：文本从窗口中心开始（居中位置减去文本宽度的一半）
                DrawTextW(hdcMem, textToDisplay.c_str(), textLen, &tempRect, 
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_CALCRECT);
                actualTextHeight = tempRect.bottom - tempRect.top;
                scrollX = windowWidth;
                scrollY = (windowHeight - actualTextHeight) / 2;
                break;
            case StandbyScrollType::Static:
                // 静止文本：直接居中显示
                DrawTextW(hdcMem, textToDisplay.c_str(), textLen, &tempRect, 
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_CALCRECT);
                actualTextHeight = tempRect.bottom - tempRect.top;
                actualTextWidth = tempRect.right - tempRect.left;
                scrollX = (windowWidth - actualTextWidth) / 2;
                scrollY = (windowHeight - actualTextHeight) / 2;
                break;
            case StandbyScrollType::VerticalScroll:
                // 垂直滚动：计算文本尺寸
                DrawTextW(hdcMem, textToDisplay.c_str(), textLen, &tempRect,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_CALCRECT);
                actualTextWidth = tempRect.right - tempRect.left;
                actualTextHeight = tempRect.bottom - tempRect.top;
                // 文本从窗口底部正中开始向上滚动
                scrollX = (windowWidth - actualTextWidth) / 2; // 水平居中
                scrollY = windowHeight; // 从底部开始
                break;
            default:
                // 默认居中
                DrawTextW(hdcMem, textToDisplay.c_str(), textLen, &tempRect,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_CALCRECT);
                actualTextHeight = tempRect.bottom - tempRect.top;
                actualTextWidth = tempRect.right - tempRect.left;
                scrollX = (windowWidth - actualTextWidth) / 2;
                scrollY = (windowHeight - actualTextHeight) / 2;
                break;
            }

            // 初始化完成
            needCenterInit = false;
        }

        // 创建GDI+字体
        FontFamily* fontFamily;
        if (!g_settings.fontName.empty()) {
            fontFamily = new FontFamily(g_settings.fontName.c_str());
        } else {
            fontFamily = new FontFamily(L"KaiTi"); // 默认字体
        }

        // 设置字体样式
        int fontStyle = FontStyleRegular;
        if (g_settings.isBold) fontStyle |= FontStyleBold;
        if (g_settings.isItalic) fontStyle |= FontStyleItalic;
        if (g_settings.isUnderline) fontStyle |= FontStyleUnderline;

        // 创建字体
        Font textFont(fontFamily, g_settings.fontSize, fontStyle, UnitPixel);

        // 提取文本颜色
        BYTE textR = GetRValue(g_settings.textColor);
        BYTE textG = GetGValue(g_settings.textColor);
        BYTE textB = GetBValue(g_settings.textColor);

        // 创建带透明度的文本画笔
        // 优化文本渲染，在透明背景下使用不同的抗锯齿策略
        SolidBrush textBrush(Color(g_settings.textOpacity, textR, textG, textB));
        
        // 根据不同的透明模式设置合适的文本对比度
        if (g_settings.isTransparentBackground) {
            // 在半透明背景上使用适中对比度以获得平衡效果
            graphics.SetTextContrast(120);
        } else {
            // 在完全透明窗口上禁用特殊对比度处理，使用默认值以避免白色边缘
            graphics.SetTextContrast(4);
        }

        // 根据不同滚动类型在主DC上绘制文本
        if (g_settings.standbyScrollType == StandbyScrollType::Static) {
            // 静止文本 - 完全居中显示
            StringFormat stringFormat;
            stringFormat.SetAlignment(StringAlignmentCenter);
            stringFormat.SetLineAlignment(StringAlignmentCenter);
            stringFormat.SetTrimming(StringTrimmingEllipsisCharacter);
            
            // 针对不同透明模式使用不同的格式标志
            if (g_settings.isTransparentBackground) {
                // 半透明背景使用标准设置
                stringFormat.SetFormatFlags(StringFormatFlagsNoClip);
            } else {
                // 完全透明窗口使用特殊设置避免白色边缘
                stringFormat.SetFormatFlags(StringFormatFlagsNoClip | StringFormatFlagsMeasureTrailingSpaces);
            }
            
            graphics.DrawString(textToDisplay.c_str(), -1, &textFont, 
                RectF(0, 0, (REAL)width, (REAL)height), 
                &stringFormat, &textBrush);
        }
        else if (g_settings.standbyScrollType == StandbyScrollType::HorizontalScroll) {
            // 水平滚动效果
            StringFormat horizontalFormat;
            horizontalFormat.SetAlignment(StringAlignmentNear);
            horizontalFormat.SetLineAlignment(StringAlignmentCenter);
            horizontalFormat.SetTrimming(StringTrimmingNone);
            horizontalFormat.SetFormatFlags(StringFormatFlagsNoClip); // 避免文本裁剪导致的边框效果
            
            // 绘制文本，允许文本完全移出窗口
            graphics.DrawString(textToDisplay.c_str(), -1, &textFont, 
                PointF((REAL)scrollX, (REAL)(height / 2)), 
                &horizontalFormat, &textBrush);
        }
        else if (g_settings.standbyScrollType == StandbyScrollType::VerticalScroll) {
            // 垂直滚动效果
            StringFormat verticalFormat;
            verticalFormat.SetAlignment(StringAlignmentCenter);
            verticalFormat.SetLineAlignment(StringAlignmentNear);
            verticalFormat.SetTrimming(StringTrimmingNone);
            verticalFormat.SetFormatFlags(StringFormatFlagsNoClip);
            
            // 计算实际文本高度
            RectF textBound;
            graphics.MeasureString(textToDisplay.c_str(), -1, &textFont, 
                RectF(20, 0, (REAL)(width - 40), 10000.0f), 
                &verticalFormat, &textBound);
            float actualTextHeight = textBound.Height;
            
            // 绘制文本，允许文本完全移出窗口
            graphics.DrawString(textToDisplay.c_str(), -1, &textFont, 
                RectF(20, (REAL)scrollY, (REAL)(width - 40), actualTextHeight), 
                &verticalFormat, &textBrush);
        }
        else {
            // 默认居中显示
            StringFormat defaultFormat;
            defaultFormat.SetAlignment(StringAlignmentNear);
            defaultFormat.SetLineAlignment(StringAlignmentNear);
            defaultFormat.SetTrimming(StringTrimmingEllipsisCharacter);
            defaultFormat.SetFormatFlags(StringFormatFlagsNoClip);
            
            graphics.DrawString(textToDisplay.c_str(), -1, &textFont, 
                RectF(20, 20, (REAL)(width - 40), (REAL)(height - 40)), 
                &defaultFormat, &textBrush);
        }

        // 直接复制内存DC到窗口DC
        // 窗口透明度已在ApplyWindowSettings函数中设置:
        // - 勾选透明模式时: 使用LWA_ALPHA控制整体透明度
        // - 不勾选透明模式时: 使用LWA_COLORKEY控制背景透明
        BitBlt(hdc, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);
        

        // 立即刷新窗口，减少闪烁
        // 移除FlushInstructionCache，Windows API不推荐在这里使用
        UpdateWindow(hWnd);

        // 清理GDI+资源
        graphics.ReleaseHDC(hdcMem);
        if (fontFamily) {
            delete fontFamily;
        }

        // 清理双缓冲资源，确保所有资源正确释放
        if (hbmOld) {
            SelectObject(hdcMem, hbmOld);
        }
        if (hbmMem) {
            DeleteObject(hbmMem);
        }
        if (hdcMem) {
            DeleteDC(hdcMem);
        }

        EndPaint(hWnd, &ps);
        break;
    }

    case WM_LBUTTONDOWN: {
        if (!g_settings.isLocked) {
            isDragging = true;
            dragOffset.x = LOWORD(lParam);
            dragOffset.y = HIWORD(lParam);
            SetCapture(hWnd);
        }
    }
                       break;

    case WM_LBUTTONUP: {
        if (isDragging) {
            isDragging = false;
            ReleaseCapture();
        }
    }
                     break;

    case WM_MOUSEMOVE: {
        if (isDragging) {
            POINT pt;
            GetCursorPos(&pt);
            RECT rect;
            GetWindowRect(hWnd, &rect);
            int newX = pt.x - dragOffset.x;
            int newY = pt.y - dragOffset.y;
            MoveWindow(hWnd, newX, newY, rect.right - rect.left, rect.bottom - rect.top, TRUE);

            // Update saved position while dragging
            g_settings.xPos = newX;
            g_settings.yPos = newY;
        }
    }
                     break;

    case WM_MOVE: {
        // Update saved position when window is moved
        int x = (int)(short)LOWORD(lParam);
        int y = (int)(short)HIWORD(lParam);
        g_settings.xPos = x;
        g_settings.yPos = y;
    }
                break;

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDM_EXIT:
            PostQuitMessage(0);
            break;
        case IDM_SETTINGS:
            CreateSettingsWindow(hWnd);
            break;
        case IDM_ABOUT:
            MessageBoxW(hWnd, L"v1.0", L"关于", MB_OK);
            break;
        case IDM_SHOW:
            ShowWindow(hWnd, SW_SHOW);
            break;
        case IDM_LOCK: {
            g_settings.isLocked = !g_settings.isLocked;
            // Update menu item text
            if (g_hPopupMenu) {
                ModifyMenuW(g_hPopupMenu, IDM_LOCK, MF_BYCOMMAND | MF_STRING, IDM_LOCK, g_settings.isLocked ? L"解锁窗口" : L"锁定窗口");
            }

            // Set or remove WS_EX_TRANSPARENT style for mouse click-through
            LONG_PTR exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
            if (g_settings.isLocked) {
                // Add WS_EX_TRANSPARENT to make window pass through mouse events
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
            }
            else {
                // Remove WS_EX_TRANSPARENT to restore normal mouse behavior
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
            }
            // Apply the new style
            SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
                     break;
        case IDM_TOPMOST: {
            g_settings.isTopmost = !g_settings.isTopmost;
            // Update menu item check state
            if (g_hPopupMenu) {
                UINT menuFlags = MF_BYCOMMAND | MF_STRING | (g_settings.isTopmost ? MF_CHECKED : MF_UNCHECKED);
                ModifyMenuW(g_hPopupMenu, IDM_TOPMOST, menuFlags, IDM_TOPMOST, L"窗口置顶");
            }

            // Apply topmost setting to window
            SetWindowPos(hWnd, g_settings.isTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            // Save setting to INI file
            SaveSettingsToIni();
        }
                        break;
        }
    }
                   break;

    case WM_DESTROY: {
        // 销毁定时器
        KillTimer(hWnd, 1);
        RemoveTaskbarIcon();
        if (g_hPopupMenu) DestroyMenu(g_hPopupMenu);
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
    }
                   break;

    case WM_RBUTTONDOWN: {
        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(hWnd);
        
        // 根据窗口可见性动态调整显示窗口菜单项
        bool isWindowVisible = IsWindowVisible(hWnd);
        if (g_hPopupMenu) {
            // 先检查菜单项是否存在
            MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
            mii.fMask = MIIM_ID;
            
            // 根据窗口状态决定是否显示Show Window菜单项
            if (isWindowVisible) {
                // 窗口可见时，移除显示窗口菜单项
                DeleteMenu(g_hPopupMenu, IDM_SHOW, MF_BYCOMMAND);
            } else {
                // 窗口不可见时，确保显示窗口菜单项存在
                if (!GetMenuItemInfoW(g_hPopupMenu, IDM_SHOW, FALSE, &mii)) {
                    // 如果菜单项不存在，则添加它
                    InsertMenuW(g_hPopupMenu, 0, MF_BYPOSITION | MF_STRING, IDM_SHOW, L"显示窗口");
                }
            }
        }
        
        TrackPopupMenu(g_hPopupMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
        // Send WM_NULL message to ensure menu commands are processed
        PostMessage(hWnd, WM_NULL, 0, 0);
    }
                       break;

    case WM_TRAYMESSAGE: {
        if (lParam == WM_RBUTTONDOWN) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            
            // 根据窗口可见性动态调整显示窗口菜单项
            bool isWindowVisible = IsWindowVisible(hWnd);
            if (g_hPopupMenu) {
                // 先检查菜单项是否存在
                MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
                mii.fMask = MIIM_ID;
                
                // 根据窗口状态决定是否显示Show Window菜单项
                if (isWindowVisible) {
                    // 窗口可见时，移除显示窗口菜单项
                    DeleteMenu(g_hPopupMenu, IDM_SHOW, MF_BYCOMMAND);
                } else {
                    // 窗口不可见时，确保显示窗口菜单项存在
                    if (!GetMenuItemInfoW(g_hPopupMenu, IDM_SHOW, FALSE, &mii)) {
                        // 如果菜单项不存在，则添加它
                        InsertMenuW(g_hPopupMenu, 0, MF_BYPOSITION | MF_STRING, IDM_SHOW, L"显示窗口");
                    }
                }
            }
            
            TrackPopupMenu(g_hPopupMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
            // Send WM_NULL message to ensure menu commands are processed
            PostMessage(hWnd, WM_NULL, 0, 0);
        }
        else if (lParam == WM_LBUTTONDBLCLK) {
            ShowWindow(hWnd, IsWindowVisible(hWnd) ? SW_HIDE : SW_SHOW);
        }
    }
                       break;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// 加载并解析CSV文件中的单词卡内容
bool LoadWordCardsFromCsv(const std::wstring& filePath) {
    wordCards.clear();
    
    // 检查文件是否存在
    if (GetFileAttributesW(filePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    
    // 使用Windows API打开文件
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // 读取文件内容
    DWORD fileSize = GetFileSize(hFile, nullptr);
    
    std::vector<char> fileData(fileSize + 1); // +1 for null terminator
    DWORD bytesRead;
    if (!ReadFile(hFile, fileData.data(), fileSize, &bytesRead, nullptr)) {
        CloseHandle(hFile);
        return false;
    }
    
    fileData[bytesRead] = '\0'; // 添加null终止符
    CloseHandle(hFile);
    
    // 转换为宽字符串
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, fileData.data(), -1, nullptr, 0);
    std::vector<wchar_t> wideBuffer(wideSize);
    MultiByteToWideChar(CP_UTF8, 0, fileData.data(), -1, wideBuffer.data(), wideSize);
    
    std::wstring fileContent(wideBuffer.data());
    
    // 简单的行分割和解析
    size_t startPos = 0;
    int lineCount = 0;
    
    while (startPos < fileContent.length()) {
        lineCount++;
        
        // 找到行尾
        size_t endPos = fileContent.find(L"\n", startPos);
        if (endPos == std::wstring::npos) {
            endPos = fileContent.length();
        }
        
        // 提取当前行
        std::wstring line = fileContent.substr(startPos, endPos - startPos);
        
        // 去除行尾的\r
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        
        // 跳过空行
        if (line.empty()) {
            startPos = endPos + 1;
            continue;
        }
        

        
        // 按逗号分割段落
        std::vector<std::wstring> paragraph;
        size_t pos = 0;
        while (pos < line.size()) {
            size_t nextPos = line.find(L',', pos);
            if (nextPos == std::wstring::npos) {
                // 最后一个段落
                std::wstring paraText = line.substr(pos);
                // 去除段落前后的空白字符和内部换行符
                // 1. 移除前后空白字符
                paraText.erase(0, paraText.find_first_not_of(L" \t\r\n"));
                if (!paraText.empty()) {
                    paraText.erase(paraText.find_last_not_of(L" \t\r\n") + 1);
                    if (!paraText.empty()) {
                        // 2. 移除段落内部的所有换行符和回车符
                        size_t found = paraText.find(L"\n");
                        while (found != std::wstring::npos) {
                            paraText.erase(found, 1);
                            found = paraText.find(L"\n", found);
                        }
                        
                        // 移除所有回车符
                        found = paraText.find(L"\r");
                        while (found != std::wstring::npos) {
                            paraText.erase(found, 1);
                            found = paraText.find(L"\r", found);
                        }
                        
                        paragraph.push_back(paraText);
                    }
                }
                break;
            } else {
                std::wstring paraText = line.substr(pos, nextPos - pos);
                // 去除段落前后的空白字符和内部换行符
                // 1. 移除前后空白字符
                paraText.erase(0, paraText.find_first_not_of(L" \t\r\n"));
                if (!paraText.empty()) {
                    paraText.erase(paraText.find_last_not_of(L" \t\r\n") + 1);
                    if (!paraText.empty()) {
                        // 2. 移除段落内部的所有换行符和回车符
                        size_t found = paraText.find(L"\n");
                        while (found != std::wstring::npos) {
                            paraText.erase(found, 1);
                            found = paraText.find(L"\n", found);
                        }
                        
                        // 移除所有回车符
                        found = paraText.find(L"\r");
                        while (found != std::wstring::npos) {
                            paraText.erase(found, 1);
                            found = paraText.find(L"\r", found);
                        }
                        
                        paragraph.push_back(paraText);
                    }
                }
                pos = nextPos + 1;
            }
        }
        
        if (!paragraph.empty()) {
            wordCards.push_back(paragraph);

        }
        
        startPos = endPos + 1;
    }
    

    // 初始化洗牌索引列表
    shuffledIndices.clear();
    for (int i = 0; i < static_cast<int>(wordCards.size()); i++) {
        shuffledIndices.push_back(i);
    }
    
    // 使用Fisher-Yates洗牌算法打乱顺序
    for (int i = static_cast<int>(shuffledIndices.size()) - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        std::swap(shuffledIndices[i], shuffledIndices[j]);
    }
    
    shuffledIndex = 0; // 重置洗牌索引位置
    
    lastCardChangeTime = GetTickCount64();
    return !wordCards.empty();
}

// 绘制单词卡内容
void DrawWordCard(Graphics& graphics, HWND hWnd) {
    if (wordCards.empty() || static_cast<size_t>(currentCardIndex) >= wordCards.size()) {
        return;
    }
    
    // 获取窗口客户区大小
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;
    
    // 创建字体
    FontFamily fontFamily(g_settings.fontName.c_str());
    
    // 设置字体样式
    int fontStyle = FontStyleRegular;
    if (g_settings.isBold) fontStyle |= FontStyleBold;
    if (g_settings.isItalic) fontStyle |= FontStyleItalic;
    if (g_settings.isUnderline) fontStyle |= FontStyleUnderline;
    
    Font font(&fontFamily, g_settings.fontSize, fontStyle, UnitPixel);
    
    // 设置文本颜色（考虑透明度）
    // 使用ARGB构造函数设置颜色和透明度
    BYTE r = GetRValue(g_settings.textColor);
    BYTE g = GetGValue(g_settings.textColor);
    BYTE b = GetBValue(g_settings.textColor);
    Color textColor(g_settings.textOpacity, r, g, b);
    SolidBrush brush(textColor);
    
    // 定义段落间距和边距
    float paragraphSpacing = 0.0f; // 进一步减小段落间距，使文本更紧凑
    
    // 设置文本格式：确保文本块整体居中，但每行文本相对于文本块左对齐
    StringFormat format;
    format.SetAlignment(StringAlignmentNear); // 文本相对于布局矩形左对齐
    format.SetLineAlignment(StringAlignmentNear); // 垂直方向顶部对齐
    format.SetFormatFlags(StringFormatFlags::StringFormatFlagsNoClip | StringFormatFlags::StringFormatFlagsLineLimit);
    float contentMargin = 20.0f; // 增加适当的边距
    
    const auto& paragraphs = wordCards[currentCardIndex];
    
    // 考虑可用宽度（减去边距）
    float availableWidth = width - 2 * contentMargin;
    
    // 第一步：计算所有段落的尺寸，并找出最大宽度
    std::vector<RectF> paragraphBounds;
    float totalHeight = 0.0f;
    float maxWidth = 0.0f;
    
    for (const auto& para : paragraphs) {
        RectF paraBounds;
        // 测量每个段落的尺寸，使用可用宽度作为最大宽度
        graphics.MeasureString(para.c_str(), para.length(), &font, 
            RectF(0, 0, availableWidth, height), &format, &paraBounds);
        
        paragraphBounds.push_back(paraBounds);
        totalHeight += paraBounds.Height;
        maxWidth = max(maxWidth, paraBounds.Width);
    }
    
    // 计算文字块的起始位置，将其置于窗口左上角
    float blockStartX = contentMargin;  // 文本块的X坐标（左侧边距）
    float blockStartY = contentMargin;  // 文本块的Y坐标（顶部边距）
    
    // 第二步：绘制每个段落，确保每行文字的X坐标与文本块的X坐标对齐
    float currentY = blockStartY;
    for (size_t i = 0; i < paragraphs.size(); i++) {
        const auto& para = paragraphs[i];
        
        // 使用文本块的X坐标作为每个段落的起始X坐标，确保对齐
        RectF layoutRect(blockStartX , currentY, maxWidth, height - currentY);
        graphics.DrawString(para.c_str(), para.length(), &font, layoutRect, &format, &brush);
        
        // 更新下一段落的Y坐标
        currentY += paragraphBounds[i].Height;
    }
}

// 切换到下一张单词卡
void SwitchToNextWordCard() {
    if (wordCards.empty()) {
        return;
    }
    
    // 直接更新索引，不再检查时间间隔（由定时器保证间隔）
    if (g_settings.isRandomOrder) {
        // 随机顺序 - 使用洗牌算法确保在一轮中不重复
        if (shuffledIndices.empty() || shuffledIndex >= static_cast<int>(shuffledIndices.size())) {
            // 如果洗牌列表为空或已遍历完所有单词，重新洗牌
            shuffledIndices.clear();
            for (int i = 0; i < static_cast<int>(wordCards.size()); i++) {
                shuffledIndices.push_back(i);
            }
            
            // 使用Fisher-Yates洗牌算法打乱顺序
            for (int i = static_cast<int>(shuffledIndices.size()) - 1; i > 0; i--) {
                int j = rand() % (i + 1);
                std::swap(shuffledIndices[i], shuffledIndices[j]);
            }
            
            shuffledIndex = 0;
        }
        
        // 使用洗牌后的索引
        currentCardIndex = shuffledIndices[shuffledIndex];
        shuffledIndex++;
    } else {
        // 顺序播放
        currentCardIndex = (currentCardIndex + 1) % wordCards.size();
    }
    
    // 重置时间戳
    lastCardChangeTime = GetTickCount64();
}
