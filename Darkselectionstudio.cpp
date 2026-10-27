// Dark Selection Studio
// Derleme (MSVC):  cl /EHsc /utf-8 DarkSelectionStudio.cpp user32.lib gdi32.lib advapi32.lib shell32.lib /link /SUBSYSTEM:WINDOWS
// Derleme (MinGW): g++ DarkSelectionStudio.cpp -o DarkSelectionStudio.exe -municode -mwindows -lgdi32 -ladvapi32 -lshell32

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM
#include <shellapi.h>
#include <string>
#include <cstdio>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")

// ---------------------------------------------------------------- Veriler ---

struct Preset {
    const wchar_t* name;
    COLORREF color;
};

static const Preset PRESETS[] = {
    { L"Koyu Kirmizi", RGB(139,   0,   0) },
    { L"Bordo",        RGB(100,   0,  24) },
    { L"Koyu Mor",     RGB(75,   0, 110) },
    { L"Koyu Yesil",   RGB(0,  90,  45) },
    { L"Gece Mavisi",  RGB(20,  45, 100) },
    { L"Antrasit",     RGB(55,  55,  60) },
};

static constexpr int PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);
static constexpr int WIN_W = 650;
static constexpr int WIN_H = 470;

// Tema renkleri
static constexpr COLORREF C_BG = RGB(19, 19, 22);
static constexpr COLORREF C_TEXT = RGB(238, 238, 242);
static constexpr COLORREF C_TEXT_DIM = RGB(160, 160, 170);
static constexpr COLORREF C_HEADING = RGB(205, 205, 215);
static constexpr COLORREF C_BTN = RGB(29, 29, 34);
static constexpr COLORREF C_BTN_SEL = RGB(47, 47, 54);
static constexpr COLORREF C_BORDER = RGB(65, 65, 74);
static constexpr COLORREF C_BORDER_SEL = RGB(235, 235, 240);

static HWND  g_hwnd = nullptr;
static int   g_selected = 0;
static HFONT g_font = nullptr;
static HFONT g_titleFont = nullptr;

// ------------------------------------------------------------- Yardimcilar ---

static COLORREF SelectedColor() { return PRESETS[g_selected].color; }

static std::wstring RgbText(COLORREF c) {
    wchar_t b[32];
    swprintf_s(b, L"%d %d %d", GetRValue(c), GetGValue(c), GetBValue(c));
    return b;
}

static std::wstring HexText(COLORREF c) {
    wchar_t b[16];
    swprintf_s(b, L"#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    return b;
}

static bool PointInRect(POINT p, const RECT& r) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

// Yerlesim
static RECT PresetRect(int i) {
    int x = 24 + (i % 3) * 198;
    int y = 145 + (i / 3) * 66;
    return RECT{ x, y, x + 184, y + 52 };
}
static RECT ApplyRect() { return RECT{ 24, 355, 220, 405 }; }
static RECT RefreshRect() { return RECT{ 232, 355, 444, 405 }; }

// ------------------------------------------------------------------ Cizim ---

static void FillRectColor(HDC dc, const RECT& r, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &r, brush);
    DeleteObject(brush);
}

static void DrawOutline(HDC dc, const RECT& r, COLORREF color) {
    HPEN   pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, r.left, r.top, r.right, r.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

static void DrawTextIn(HDC dc, const wchar_t* text, RECT r, COLORREF color,
    HFONT font, UINT format) {
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(dc, font);
    DrawTextW(dc, text, -1, &r, format);
    SelectObject(dc, old);
}

static void DrawTextAt(HDC dc, const wchar_t* text, int x, int y,
    COLORREF color, HFONT font) {
    RECT r{ x, y, WIN_W - 24, y + 30 };
    DrawTextIn(dc, text, r, color, font, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

// leftPad > 0 ise etiket sola yaslanir (renk kutusu icin yer birakir)
static void DrawButton(HDC dc, const RECT& r, const wchar_t* label,
    COLORREF fill, COLORREF border, COLORREF text,
    int leftPad = 0) {
    FillRectColor(dc, r, fill);
    DrawOutline(dc, r, border);

    if (leftPad > 0) {
        RECT lr{ r.left + leftPad, r.top, r.right - 4, r.bottom };
        DrawTextIn(dc, label, lr, text, g_font, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    else {
        RECT lr = r;
        DrawTextIn(dc, label, lr, text, g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void DrawPresetButton(HDC dc, int i) {
    RECT r = PresetRect(i);
    bool selected = (i == g_selected);

    DrawButton(dc, r, PRESETS[i].name,
        selected ? C_BTN_SEL : C_BTN,
        selected ? C_BORDER_SEL : C_BORDER,
        C_TEXT, 38);

    RECT swatch{ r.left + 9, r.top + 12, r.left + 29, r.top + 40 };
    FillRectColor(dc, swatch, PRESETS[i].color);
    DrawOutline(dc, swatch, RGB(100, 100, 110));
}

static void Paint(HDC dc, HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);
    FillRectColor(dc, client, C_BG);

    DrawTextAt(dc, L"Dark Selection Studio", 24, 18, RGB(245, 245, 248), g_titleFont);
    DrawTextAt(dc, L"Windows 10 | Masaustu secim dikdortgeni renkleri", 25, 55, C_TEXT_DIM, g_font);
    DrawTextAt(dc, L"HAZIR KOYU RENKLER", 24, 103, C_HEADING, g_font);

    for (int i = 0; i < PRESET_COUNT; ++i)
        DrawPresetButton(dc, i);

    DrawTextAt(dc, L"SECILI RENK", 24, 282, C_HEADING, g_font);
    RECT preview{ 24, 313, 58, 341 };
    FillRectColor(dc, preview, SelectedColor());
    DrawTextAt(dc, HexText(SelectedColor()).c_str(), 70, 311, RGB(245, 245, 248), g_titleFont);
    DrawTextAt(dc, (L"RGB: " + RgbText(SelectedColor())).c_str(), 200, 314,
        RGB(165, 165, 175), g_font);

    DrawButton(dc, ApplyRect(), L"Rengi Uygula",
        RGB(105, 0, 0), RGB(155, 35, 35), RGB(255, 255, 255));
    DrawButton(dc, RefreshRect(), L"Explorer'i Yenile",
        RGB(37, 37, 44), RGB(75, 75, 86), RGB(240, 240, 245));

    DrawTextAt(dc, L"Not: Kenarlik ve dolgu icin ayni RGB degeri kullanilir.",
        24, 414, RGB(135, 135, 145), g_font);
}

// ------------------------------------------------------------ Sistem islemi ---

static bool SetColorValue(HKEY key, const wchar_t* name, COLORREF c) {
    std::wstring value = RgbText(c);
    return RegSetValueExW(key, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

static bool ApplySelectionColor(COLORREF c) {
    // 1) Kalici kayit (oturum kapatip acinca da gecerli)
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Colors", 0,
        KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        MessageBoxW(g_hwnd, L"Registry anahtari acilamadi.", L"Hata", MB_ICONERROR);
        return false;
    }
    bool ok = SetColorValue(key, L"Hilight", c);
    ok = SetColorValue(key, L"HotTrackingColor", c) && ok;
    RegCloseKey(key);

    // 2) Calisan oturuma hemen uygula
    INT      elements[2] = { COLOR_HIGHLIGHT, COLOR_HOTLIGHT };
    COLORREF values[2] = { c, c };
    SetSysColors(2, elements, values);

    DWORD_PTR result = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
        reinterpret_cast<LPARAM>(L"Control Panel\\Colors"),
        SMTO_ABORTIFHUNG, 2000, &result);

    if (!ok) {
        MessageBoxW(g_hwnd, L"Renklerden biri Registry'ye yazilamadi.", L"Hata", MB_ICONERROR);
        return false;
    }

    InvalidateRect(g_hwnd, nullptr, FALSE);
    MessageBoxW(g_hwnd,
        L"Renk degerleri kaydedildi.\n\nGerekirse 'Explorer'i Yenile' dugmesine bas.",
        L"Uygulandi", MB_ICONINFORMATION);
    return true;
}

static void RestartExplorer() {
    int answer = MessageBoxW(g_hwnd,
        L"Explorer yeniden baslatilacak. Gorev cubugu ve masaustu kisa sure kaybolabilir.\n"
        L"Devam edilsin mi?",
        L"Explorer'i Yenile", MB_YESNO | MB_ICONQUESTION);
    if (answer != IDYES) return;

    ShellExecuteW(nullptr, L"open", L"cmd.exe",
        L"/c taskkill /f /im explorer.exe >nul 2>&1 & start explorer.exe",
        nullptr, SW_HIDE);
}

// ------------------------------------------------------------- Pencere/Mesaj ---

static void HandleClick(HWND hwnd, POINT p) {
    for (int i = 0; i < PRESET_COUNT; ++i) {
        if (PointInRect(p, PresetRect(i))) {
            g_selected = i;
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
    }
    if (PointInRect(p, ApplyRect())) { ApplySelectionColor(SelectedColor()); return; }
    if (PointInRect(p, RefreshRect())) { RestartExplorer(); return; }
}

static HFONT MakeFont(int height, int weight) {
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_font = MakeFont(18, FW_NORMAL);
        g_titleFont = MakeFont(27, FW_SEMIBOLD);
        return 0;

    case WM_ERASEBKGND:
        return 1;   // Paint() tum alani zaten boyuyor

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        Paint(dc, hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONUP:
        HandleClick(hwnd, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
        return 0;

    case WM_DESTROY:
        if (g_font)      DeleteObject(g_font);
        if (g_titleFont) DeleteObject(g_titleFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCmd) {
    const wchar_t CLASS_NAME[] = L"DarkSelectionStudioWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Pencere sinifi olusturulamadi.", L"Hata", MB_ICONERROR);
        return 1;
    }

    const DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r{ 0, 0, WIN_W, WIN_H };
    AdjustWindowRect(&r, style, FALSE);

    g_hwnd = CreateWindowExW(0, CLASS_NAME, L"Dark Selection Studio", style,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, instance, nullptr);
    if (!g_hwnd) {
        MessageBoxW(nullptr, L"Pencere olusturulamadi.", L"Hata", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hwnd, showCmd);
    UpdateWindow(g_hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
