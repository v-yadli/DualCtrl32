#pragma once
#include "DualCtrl32.h"

#define DC32_TRAY_MESSAGE (WM_APP + 1)
#define DC32_EXIT_MESSAGE (WM_APP + 2)
NOTIFYICONDATA g_nid;

void ShowContextMenu(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();
	if(hMenu)
	{
		InsertMenu(hMenu, -1, MF_BYPOSITION, DC32_EXIT_MESSAGE, _T("Exit"));
		SetForegroundWindow(hWnd);

		TrackPopupMenu(hMenu, TPM_BOTTOMALIGN,
			pt.x, pt.y, 0, hWnd, NULL );
		DestroyMenu(hMenu);
	}
}

LRESULT CALLBACK WndMessageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
    switch (msg)
    {
    case DC32_TRAY_MESSAGE:
        switch (lParam)
        {
        case WM_RBUTTONDOWN:
        case WM_CONTEXTMENU:
            ShowContextMenu(hwnd);
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam); 
        switch (wmId)
        {
        case DC32_EXIT_MESSAGE:
            DestroyWindow(hwnd);
        }
        return 1;
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void SetupUI(HINSTANCE hInstance)
{
    static const LPWSTR class_name = L"DUALCTRL32";
    WNDCLASSEX wx                  ={};
    wx.cbSize                      = sizeof(WNDCLASSEX);
    wx.lpfnWndProc                 = WndMessageProc;        // function which will handle messages
    wx.hInstance                   = hInstance;
    wx.lpszClassName               = class_name;
    HWND hwnd                      = NULL;
    if (RegisterClassEx(&wx))
    {
        hwnd = CreateWindow(class_name, L"DualCtrl32", 0, 0, 0, 1, 1, NULL, NULL, hInstance, NULL);
    }

    if (hwnd == NULL) return;

    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DUALCTRL32));
    g_nid.cbSize  = sizeof(NOTIFYICONDATA);
    g_nid.hWnd    = hwnd;
    g_nid.uID     = IDI_DUALCTRL32;
    g_nid.uFlags  = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.hIcon   = hIcon;
    g_nid.uCallbackMessage = DC32_TRAY_MESSAGE;
	// tooltip message
    lstrcpyn(g_nid.szTip, _T("DualCtrl32"), sizeof(g_nid.szTip)/sizeof(TCHAR));
    Shell_NotifyIcon(NIM_ADD, &g_nid);

	// free icon handle
	if(g_nid.hIcon && DestroyIcon(g_nid.hIcon))
		g_nid.hIcon = NULL;
}

