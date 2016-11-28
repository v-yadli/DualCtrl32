#include "DualCtrl32.h"
#include "UI.h"
#include "..\inih\ini.h"
#include "Shlwapi.h"
#include "Strsafe.h"

#if _DEBUG
#define debug(x) do{printf(x);}while(false)
#else
#define debug(x) do{}while(false)
#endif

enum KeyboardEvent
{
    CTRL_DOWN,
    CTRL_UP,
    ALT_DOWN,
    ALT_UP,
    OTHER_KEY_UP,
    OTHER_KEY_DOWN,
    MOUSE_MOVE,
    MOUSE_BUTTON,
    TIMEOUT,    //CTRL flush timer fired
    NONE,       //Not interested, pass through
    INTERCEPT,  //Unknown, intercept
};

#define DC32_CONFIG_FILENAME     "\\DualCtrl32.ini"
#define DC32_CTRLFLUSH_TIMEOUT   (10086)
#define DC32_PATHMAX             (1024)

//============ Configuration ============

#define CONFIG_CTRLFLUSH_TIMEOUT "TimerTrigger"
#define CONFIG_CTRLFLUSH_MOUSE   "MouseTrigger"
#define CONFIG_CTRLFLUSH_KEY     "KeyTrigger"
#define CONFIG_ALTDRAG_FIX       "AltDragFix"

UINT          g_ctrlflush_timeout = 0;     // 0: Disable; >0: force flush CTRL-DOWN event after timeout
BOOL          g_ctrlflush_mouse   = TRUE;  // FALSE: do not flush CTRL-DOWN event on mouse movement
BOOL          g_ctrlflush_key     = TRUE;  // FALSE: do not flush CTRL-DOWN event on key strokes
BOOL          g_altdrag_fix       = TRUE;  // FALSE: Disable AltDrag fix.

//============ State registers ============

BOOL          g_ctrl_down         = FALSE; // TRUE if CTRL is DOWN
BOOL          g_ctrl_flushed      = FALSE; // TRUE if CTRL-DOWN is flushed, and we should send CTRL-UP when the key is released
BOOL          g_ctrl_thru         = FALSE; // TRUE if we intentionally bypass our hook to let a CTRL_DOWN/CTRL_UP thru
BOOL          g_alt_down          = FALSE; // TRUE if ALT is DOWN
BOOL          g_altdrag_activated = FALSE; // TRUE if we infer the activation of AltDrag (currently not working)
BOOL          g_altdrag_ctrlseq   = FALSE; // TRUE if we detect AltDrag behavior sequence that needs a fix
BOOL          g_ignore_timer      = FALSE; // TRUE if timer should be ignored (CTRL-UP received before timer fires)

VOID             LoadConfig();
BOOL             ReadBool(LPCSTR);
INT              ReadConfigEntry(LPVOID, LPCSTR, LPCSTR, LPCSTR);
VOID CALLBACK    CtrlTimeout(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
KeyboardEvent    GetEvent(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DualCtrl32(int nCode, WPARAM wParam, LPARAM lParam);

VOID LoadConfig()
{
    debug("Loading config\n");
    HMODULE hModule = GetModuleHandle(NULL);
    if (hModule == NULL)
    {
        debug("hModule\n");
        return;
    }

    CHAR path[DC32_PATHMAX];
    if (DC32_PATHMAX == GetModuleFileNameA(hModule, path, DC32_PATHMAX))
    {
        debug("wPath\n");
        return;
    }
    PathRemoveFileSpecA(path);
    if (S_OK != StringCbCatA(path, DC32_PATHMAX, DC32_CONFIG_FILENAME))
    {
        debug("StringCbCat\n");
        return;
    }

    debug("Config path = "); debug(path); debug("\n");
    if (ini_parse(path, ReadConfigEntry, NULL))
    {
        debug("ini_parse error\n");
    }
}

#define MATCH(x) strcmp(x, key) == 0

BOOL ReadBool(LPCSTR key)
{
    return MATCH("TRUE") || MATCH("True") || MATCH("true") ||
           MATCH("Enabled") || MATCH("ENABLED");
}

INT ReadConfigEntry(LPVOID o, LPCSTR section, LPCSTR key, LPCSTR value)
{
    UNREFERENCED_PARAMETER(o);
    UNREFERENCED_PARAMETER(section);

    debug("Load config entry: "); debug(key); debug("="); debug(value); debug("\n");

    if (MATCH(CONFIG_CTRLFLUSH_KEY)) { g_ctrlflush_key = ReadBool(value); }
    if (MATCH(CONFIG_CTRLFLUSH_MOUSE)) { g_ctrlflush_mouse = ReadBool(value); }
    if (MATCH(CONFIG_CTRLFLUSH_TIMEOUT)) { g_ctrlflush_timeout = atoi(value); }
    if (MATCH(CONFIG_ALTDRAG_FIX)) { g_altdrag_fix = ReadBool(value); }

    return 1;
}

VOID CALLBACK CtrlTimeout(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    debug("Timer fired\n");
    KillTimer(hwnd, idEvent);
    if (!g_ignore_timer)
    {
        DualCtrl32(DC32_CTRLFLUSH_TIMEOUT, NULL, NULL);
    }
}

KeyboardEvent GetEvent(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
        switch (wParam)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            switch (p->vkCode)
            {
            case VK_LCONTROL: return KeyboardEvent::CTRL_DOWN;
            case VK_LMENU: return KeyboardEvent::ALT_DOWN;
            default: if (g_ctrlflush_key) return KeyboardEvent::OTHER_KEY_DOWN;
            }
        case WM_KEYUP:
        case WM_SYSKEYUP:
            switch (p->vkCode)
            {
            case VK_LCONTROL: return KeyboardEvent::CTRL_UP;
            case VK_LMENU: return KeyboardEvent::ALT_UP;
            default: if (g_ctrlflush_key) return KeyboardEvent::OTHER_KEY_UP;
            }
        case WM_MOUSEMOVE:
            if (g_ctrlflush_mouse) return KeyboardEvent::MOUSE_MOVE;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            if (g_ctrlflush_mouse) return KeyboardEvent::MOUSE_BUTTON;
        default:
            return KeyboardEvent::NONE;
        }
    }
    else if (nCode == DC32_CTRLFLUSH_TIMEOUT)
    {
        return KeyboardEvent::TIMEOUT;
    }
    return KeyboardEvent::INTERCEPT;
}

LRESULT CALLBACK DualCtrl32(int nCode, WPARAM wParam, LPARAM lParam)
{
    KeyboardEvent event     = GetEvent(nCode, wParam, lParam);
    BOOL          intercept = FALSE;

    if (g_ctrl_thru)
    {
        if (event == KeyboardEvent::CTRL_DOWN || event == KeyboardEvent::CTRL_UP)
        {
            debug("Ignore once.\n");
            g_ctrl_thru = FALSE;
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }
    }

    switch (event)
    {
    case KeyboardEvent::CTRL_DOWN:
        if (g_ctrl_down)
        {
            //  Another ctrl-down event while we already have it.
            //  Ignore this event.
            //debug("Ignore repetitive ctrl-down.\n");
        }
        else if (!g_ctrl_flushed)
        {
            debug("Cache ctrl-down event.\n");
            g_ctrl_down = TRUE;
            if (g_ctrlflush_timeout > 0)
            {
                g_ignore_timer = FALSE;
                SetTimer(NULL, DC32_CTRLFLUSH_TIMEOUT, g_ctrlflush_timeout, CtrlTimeout);
            }
        }
        intercept = TRUE;
        break;
    case KeyboardEvent::CTRL_UP:
        if (g_ctrl_down)
        {
            g_ctrl_down = FALSE;
            intercept   = TRUE;
            if (!g_altdrag_fix || (!g_alt_down && !g_altdrag_ctrlseq))
            {
                //  Translate to ESC
                debug("Translate to ESC.\n");
                keybd_event(VK_ESCAPE, 0x81, 0, 0);
                keybd_event(VK_ESCAPE, 0x81, KEYEVENTF_KEYUP, 0);
                g_ignore_timer = TRUE;
            }
            else if (g_alt_down)
            {
                debug("AltDrag fix #1, ignore CTRL-UP, activate AltDrag indicator.\n");
                g_altdrag_ctrlseq = TRUE;
            }
            else if (g_altdrag_ctrlseq)
            {
                //  Send RCTRL to escape context menu (e.g. ribbon hot keys, the menu items)
                debug("AltDrag fix #2, ignore CTRL-UP, send RCTRL-UP RCTRL-DOWN instead\n");
                g_altdrag_ctrlseq = FALSE;
                keybd_event(VK_RCONTROL, 0, 0, 0);
                keybd_event(VK_RCONTROL, 0, KEYEVENTF_KEYUP, 0);
            }
        }
        else if (g_ctrl_flushed)
        {
            debug("Flush complete, CTRL released.\n");
            g_ctrl_flushed = FALSE;
            g_ctrl_thru = TRUE;
            keybd_event(VK_LCONTROL, 0x9D, KEYEVENTF_KEYUP, 0);
            intercept = TRUE;
        }
        else
        {
            intercept = TRUE;
        }
        break;
    case KeyboardEvent::MOUSE_BUTTON:
        if (g_alt_down)
        {
            //  If there's mouse activity when alt is pressed,
            //  AltDrag will be activated.
            //  XXX not activated. Seems that AltDrag intercepted this message.
            debug("AltDrag activated");
            g_altdrag_activated = TRUE;
        }
        /* FALLTHROUGH */
    case KeyboardEvent::MOUSE_MOVE:
    case KeyboardEvent::OTHER_KEY_DOWN:
    case KeyboardEvent::TIMEOUT:
        if (g_ctrl_down)
        {
            //  Flush the cached ctrl-down event
            debug("Flush ctrl-down.\n");
            g_ctrl_down    = FALSE;
            g_ctrl_flushed = TRUE;
            g_ctrl_thru    = TRUE;
            keybd_event(VK_LCONTROL, 0x9D, 0, 0);
        }
        //  Intercept TIMEOUT event as it is not a real hook message
        intercept = (event == KeyboardEvent::TIMEOUT);
        break;
    case KeyboardEvent::OTHER_KEY_UP:
    case KeyboardEvent::NONE:
        intercept = FALSE;
        break;
    case KeyboardEvent::ALT_DOWN:
        if (!g_alt_down)
        {
            g_alt_down = TRUE;
            debug("Alt down\n");
            intercept = FALSE;
        }
        else
        {
            intercept = TRUE;
        }
        break;
    case KeyboardEvent::ALT_UP:
        debug("Alt up\n");
        intercept  = FALSE;
        g_alt_down = FALSE;
        if (g_altdrag_activated)
        {
            //  Expect AltDrag sequence II
            g_altdrag_ctrlseq = TRUE;
        }
        g_altdrag_activated = FALSE;
        break;
    case KeyboardEvent::INTERCEPT:
        intercept = TRUE;
        break;
    }

    return (intercept ? 1 : CallNextHookEx(NULL, nCode, wParam, lParam));
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                       _In_opt_ HINSTANCE hPrevInstance,
                       _In_ LPTSTR    lpCmdLine,
                       _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

#if _DEBUG
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
#endif


    // Install the low-level keyboard & mouse hooks
    HHOOK hhkLLKeyboard = SetWindowsHookEx(WH_KEYBOARD_LL, DualCtrl32, 0, 0);
    HHOOK hhkLLMouse    = SetWindowsHookEx(WH_MOUSE_LL, DualCtrl32, 0, 0);

    LoadConfig();
    SetupUI(hInstance);

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
