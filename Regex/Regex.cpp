//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Regex.h"

//////////////////////////////////////////////////////////////////////

using namespace std;

using wchar = wchar_t;

#ifdef UNICODE
typedef wstring tstring;
typedef wchar_t tchar;
#else
typedef string tstring;
typedef char tchar;
#endif

typedef basic_regex<tchar> tregex;
typedef match_results<const tchar *> tcmatch;
typedef match_results<tstring::const_iterator> tsmatch;

//////////////////////////////////////////////////////////////////////

#define MAX_LOADSTRING 100

HINSTANCE hInst;
TCHAR szTitle[MAX_LOADSTRING];
TCHAR szWindowClass[MAX_LOADSTRING];

HWND gInputBox1;
HWND gInputBox2;
HWND gResults;

const int fontHeight = 13;

HFONT font = CreateFont(-fontHeight, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, TEXT("Consolas"));

//////////////////////////////////////////////////////////////////////

ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);

//////////////////////////////////////////////////////////////////////

struct Selection
{
	int begin;
	int end;

	Selection(int b, int e)
		: begin(b)
		, end(e)
	{
	}
};

//////////////////////////////////////////////////////////////////////

struct ControlFreezer
{
	ControlFreezer(HWND w) : control(w)
	{
		SendMessage(control, WM_SETREDRAW, FALSE, 0);
	}

	~ControlFreezer()
	{
		SendMessage(control, WM_SETREDRAW, TRUE, 0);
	}

	HWND control;
};

//////////////////////////////////////////////////////////////////////

string Format_V(char const *fmt, va_list v)
{
	char buffer[512];
	int l = _vscprintf(fmt, v);
	if(l >= _countof(buffer))
	{
		char *buf = new char[l + 1];
		_vsnprintf_s(buf, l + 1, _TRUNCATE, fmt, v);
		string s(buf);
		delete(buf);
		return s;
	}
	_vsnprintf_s(buffer, _countof(buffer), _TRUNCATE, fmt, v);
	return string(buffer);
}

//////////////////////////////////////////////////////////////////////

wstring Format_V(wchar const *fmt, va_list v)
{
	wchar buffer[512];
	int l = _vscwprintf(fmt, v);
	if(l >= _countof(buffer))
	{
		wchar *buf = new wchar[l + 1];
		_vsnwprintf_s(buf, l + 1, _TRUNCATE, fmt, v);
		wstring s(buf);
		delete(buf);
		return s;
	}
	_vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, fmt, v);
	return wstring(buffer);
}

//////////////////////////////////////////////////////////////////////

void Trace(wchar const *strMsg, ...)
{
	va_list args;
	va_start(args, strMsg);
	OutputDebugStringW(Format_V(strMsg, args).c_str());
	va_end(args);
}

//////////////////////////////////////////////////////////////////////

void Trace(char const *strMsg, ...)
{
	va_list args;
	va_start(args, strMsg);
	OutputDebugStringA(Format_V(strMsg, args).c_str());
	va_end(args);
}

//////////////////////////////////////////////////////////////////////

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int nCmdShow)
{
	MSG msg;
	HACCEL hAccelTable;

	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_REGEX, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_REGEX));

	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}

//////////////////////////////////////////////////////////////////////

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_REGEX));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(GetStockBrush(BLACK_BRUSH));
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_REGEX);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//////////////////////////////////////////////////////////////////////

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;

   HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
	  return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//////////////////////////////////////////////////////////////////////

template <typename T> void Zero(T &t)
{
	ZeroMemory(&t, sizeof(t));
}

//////////////////////////////////////////////////////////////////////

HTREEITEM AddTreeItem(HTREEITEM parent, tchar const *text, int begin, int end)
{

	TVINSERTSTRUCT i;
	i.hParent = parent;
	i.hInsertAfter = TVI_LAST;
	Zero(i.item);
	i.item.stateMask = TVIS_EXPANDED;
	if(parent == TVI_ROOT)
	{
		i.item.stateMask |= TVIS_BOLD;
	}
	i.item.state = i.item.stateMask;
	i.item.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
	i.item.pszText = const_cast<tchar *>(text); // !?
	i.item.lParam = (LPARAM)new Selection(begin, end);
	TreeView_Expand(gResults, parent, TVE_EXPAND);
	HTREEITEM item = TreeView_InsertItem(gResults, &i);
	return item;
}

//////////////////////////////////////////////////////////////////////

void Update()
{
	ControlFreezer freezeResults(gResults);

	TreeView_DeleteAllItems(gResults);

	tchar buffer[4096];

	GetWindowText(gInputBox2, buffer, ARRAYSIZE(buffer) - 1);
	tstring match = buffer;

	GetWindowText(gInputBox1, buffer, ARRAYSIZE(buffer) - 1);

	tregex r;
	try
	{
		r.assign(buffer);
	}
	catch(regex_error &)
	{
		return;
	}

	using it = regex_iterator < tstring::iterator >;
	it rend;
	it a(match.begin(), match.end(), r);
	while(a != rend)
	{
		auto &match0 = (*a)[0];
		auto start = a->position();
		auto end = start + a->length();
		HTREEITEM item = AddTreeItem(TVI_ROOT, match0.str().c_str(), start, end);
		for(int i = 1; i < a->length(); ++i)
		{
			auto &match = (*a)[i];
			if(match.matched)
			{
				auto start2 = start + std::distance(match0.first, match.first);
				auto end2 = start + std::distance(match0.first, match.second);
				AddTreeItem(item, match.str().c_str(), start2, end2);
			}
		}
		++a;
	}
}

//////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CREATE:
		{
			RECT rc;
			RECT r2;
			int fh = fontHeight * 125 / 100 + 2;
			int y = 0;
			GetClientRect(hWnd, &rc);

			gInputBox1 = CreateWindow(TEXT("Edit"), TEXT("\\s*([A-Za-z0-9]*)\\s*\\=\\s*([A-Za-z0-9]*)\\s*"), WS_VISIBLE | WS_CHILD, 0, 0, rc.right, fh, hWnd, (HMENU)ID_INPUT1, hInst, NULL);
			GetWindowRect(gInputBox1, &r2);
			y += 2 + (r2.bottom - r2.top);

			gInputBox2 = CreateWindow(TEXT("Edit"), TEXT("	float3 position				: semantic(name = mColor, type = byte, stream = 2, instances = 2);"), ES_NOHIDESEL | ES_MULTILINE | WS_VISIBLE | WS_CHILD, 0, y, rc.right, fh * 8, hWnd, (HMENU)ID_INPUT2, hInst, NULL);
			GetWindowRect(gInputBox2, &r2);
			y += 2 + (r2.bottom - r2.top);

			gResults = CreateWindowEx(0, WC_TREEVIEW, TEXT("Results"), WS_VISIBLE | WS_CHILD | TVS_FULLROWSELECT, 0, y, rc.right, 400, hWnd, (HMENU)ID_TREEVIEW, hInst, NULL);

			SendMessage(gInputBox1, WM_SETFONT, (WPARAM)font, MAKELPARAM(true, 0));
			SendMessage(gInputBox2, WM_SETFONT, (WPARAM)font, MAKELPARAM(true, 0));

			Update();
		}
		break;

		case WM_SIZE:
		{
			RECT rc, rc2;
			int fh = fontHeight * 125 / 100 + 2;
			GetClientRect(hWnd, &rc);

			GetWindowRect(gInputBox1, &rc2);
			ScreenToClient(hWnd, (LPPOINT)&rc2.left);
			ScreenToClient(hWnd, (LPPOINT)&rc2.right);
			MoveWindow(gInputBox1, rc2.left, rc2.top, rc.right, rc2.bottom - rc2.top, FALSE);

			GetWindowRect(gInputBox2, &rc2);
			ScreenToClient(hWnd, (LPPOINT)&rc2.left);
			ScreenToClient(hWnd, (LPPOINT)&rc2.right);
			MoveWindow(gInputBox2, rc2.left, rc2.top, rc.right, rc2.bottom - rc2.top, FALSE);

			GetWindowRect(gResults, &rc2);
			ScreenToClient(hWnd, (LPPOINT)&rc2.left);
			ScreenToClient(hWnd, (LPPOINT)&rc2.right);
			MoveWindow(gResults, rc2.left, rc2.top, rc.right, rc.bottom - rc2.top, FALSE);
		}
		break;

		case WM_NOTIFY:
		{
			switch(((NMHDR *)lParam)->idFrom)
			{
				case ID_TREEVIEW:
				{
					NMTREEVIEW *nmtv = (NMTREEVIEW *)lParam;
					switch(nmtv->hdr.code)
					{
						case TVN_SELCHANGED:
						{
							if(nmtv->action != TVC_UNKNOWN)
							{
								tchar buffer[100];
								HTREEITEM item = nmtv->itemNew.hItem;
								TVITEM i;
								Zero(i);
								i.mask = TVIF_PARAM | TVIF_TEXT;
								i.pszText = buffer;
								i.cchTextMax = 99;
								i.hItem = item;
								TreeView_GetItem(gResults, &i);
								Selection *s = (Selection *)i.lParam;
								Edit_SetSel(gInputBox2, s->begin, s->end);
							}
						}
						break;
					}
				}
				break;
			}
		}
		break;

		case WM_COMMAND:
		{
			int wmId = LOWORD(wParam);
			int wmEvent = HIWORD(wParam);

			switch(wmId)
			{
				case IDM_EXIT:
					DestroyWindow(hWnd);
					break;

				case ID_INPUT1:
				case ID_INPUT2:
					switch(HIWORD(wParam))
					{
						case EN_CHANGE:
							Update();
						break;
					}
					break;

				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
		break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
