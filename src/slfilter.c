/* slfilter.c - Sprint-Layout 6.0 makro library filter
 *
 * Loaded as a version.dll proxy from the Sprint-Layout directory.
 * Adds a filter edit box into the component library title bar (the
 * caption strip above the makro TreeView, TV_Makros). Typing filters
 * the library: matching makros are listed in a list box that replaces
 * the tree while filtering. Enter / double-click on a result jumps to
 * the item in the ORIGINAL tree (expand + select + simulated
 * double-click), so the app's own preview and "place makro" flow runs
 * unmodified.
 *
 * The tree itself is never modified: Sprint-Layout's custom tree
 * (TTreeViewx3M) breaks its internal state when items are deleted and
 * re-inserted from outside, so filtering only READS items and jumps.
 *
 * Build (TCC, 32-bit):
 *   i386-win32-tcc.exe -shared -O2 -o version.dll slfilter.c -luser32 -lkernel32 -lgdi32
 *   python fix_exports.py version.dll
 */
#define WIN32_LEAN_AND_MEAN
#if defined(__TINYC__)
#define VER_H
#define _WINVER_H
#endif
#include <windows.h>
#include "tvdefs.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* version.dll proxy forwarding                                        */
/* ------------------------------------------------------------------ */

static HMODULE g_hRealVersion;

static void *realproc(const char *name)
{
    if (!g_hRealVersion) {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);
        lstrcatA(path, "\\version.dll");
        g_hRealVersion = LoadLibraryA(path);
        if (!g_hRealVersion) return NULL;
    }
    return (void *)GetProcAddress(g_hRealVersion, name);
}

#if defined(__TINYC__)
#define PROXY(name, ret, err, args, arglist, cast)          \
    typedef ret (WINAPI *PFN_##name) cast;                  \
    __declspec(dllexport) ret WINAPI name args              \
    {                                                       \
        void *vp = realproc(#name);                         \
        PFN_##name f;                                       \
        if (!vp) return err;                                \
        f = (PFN_##name)vp;                                 \
        return f arglist;                                   \
    }
#else
#define PROXY(name, ret, err, args, arglist, cast)          \
    typedef ret (WINAPI *PFN_##name) cast;                  \
    ret WINAPI hlp_##name args                              \
    {                                                       \
        void *vp = realproc(#name);                         \
        PFN_##name f;                                       \
        if (!vp) return err;                                \
        f = (PFN_##name)vp;                                 \
        return f arglist;                                   \
    }
#endif

PROXY(GetFileVersionInfoW, BOOL, FALSE, (LPCWSTR a, DWORD b, DWORD c, LPVOID d), (a,b,c,d), (LPCWSTR,DWORD,DWORD,LPVOID))
PROXY(GetFileVersionInfoSizeW, DWORD, 0, (LPCWSTR a, DWORD *b), (a,b), (LPCWSTR,DWORD*))
PROXY(VerQueryValueW, BOOL, FALSE, (const LPVOID a, LPCWSTR b, LPVOID *c, PUINT d), (a,b,c,d), (const LPVOID,LPCWSTR,LPVOID*,PUINT))
PROXY(GetFileVersionInfoA, BOOL, FALSE, (LPCSTR a, DWORD b, DWORD c, LPVOID d), (a,b,c,d), (LPCSTR,DWORD,DWORD,LPVOID))
PROXY(GetFileVersionInfoSizeA, DWORD, 0, (LPCSTR a, DWORD *b), (a,b), (LPCSTR,DWORD*))
PROXY(VerQueryValueA, BOOL, FALSE, (const LPVOID a, LPCSTR b, LPVOID *c, PUINT d), (a,b,c,d), (const LPVOID,LPCSTR,LPVOID*,PUINT))
PROXY(GetFileVersionInfoExW, BOOL, FALSE, (DWORD a, LPCWSTR b, DWORD c, DWORD d, LPVOID e), (a,b,c,d,e), (DWORD,LPCWSTR,DWORD,DWORD,LPVOID))
PROXY(GetFileVersionInfoSizeExW, DWORD, 0, (DWORD a, LPCWSTR b, DWORD *c), (a,b,c), (DWORD,LPCWSTR,DWORD*))
PROXY(GetFileVersionInfoExA, BOOL, FALSE, (DWORD a, LPCSTR b, DWORD c, DWORD d, LPVOID e), (a,b,c,d,e), (DWORD,LPCSTR,DWORD,DWORD,LPVOID))
PROXY(GetFileVersionInfoSizeExA, DWORD, 0, (DWORD a, LPCSTR b, DWORD *c), (a,b,c), (DWORD,LPCSTR,DWORD*))

/* ------------------------------------------------------------------ */
/* filter implementation                                               */
/* ------------------------------------------------------------------ */

#define WM_FILTER_TIMER   0x5A17       /* debounce timer id           */
#define SL_FILTER_MSG     (WM_APP + 0x5A17)  /* deferred filter work  */
#define SL_INSTALL_MSG    (WM_APP + 0x5A18)  /* UI-thread install     */
#define EDIT_MINH         18
#define HOTKEY_ID         0xB00B

typedef struct SNODE {
    struct SNODE *child, *next, *parent;
    char   text[260];
    char   path[560];              /* ancestors joined with '\'  */
    HTREEITEM h;                   /* live handle (read-only!)   */
    int    isLeaf;
} SNODE;

static HWND   g_tree;                /* makro treeview             */
static HWND   g_edit;                /* filter edit (title bar)    */
static HWND   g_list;                /* result list (replaces tree)*/
static HWND   g_bar;                 /* title bar hosting the edit */
static WNDPROC g_oldTree, g_oldEdit, g_oldList, g_oldBar, g_oldPanel;
static HWND   g_panel;                /* tree's parent, hosts the list */
static HFONT  g_font;
static SNODE *g_snap;                /* read-only item snapshot    */
static int    g_snapCount;
static int    g_rebuilding;          /* re-entrancy guard          */
static int    g_lastCount = -1;      /* population stability check */
static UINT   g_dpiScale = 96;
static int    iabs(int v) { return v < 0 ? -v : v; }
static int    g_dragPending = 0;
static POINT  g_dragStartPt;

static void install_on_ui_thread(HWND tree);   /* runs on UI thread */
static void sync_edit_pos(void);               /* keep edit+list glued */

static void dbg(const char *fmt, ...)
{
    static FILE *lf;
    static int enabled = -1;
    va_list ap;
    char tmp[MAX_PATH];
    if (enabled < 0) {
        enabled = GetEnvironmentVariableA("SLFILTER_LOG", NULL, 0) ? 1 : 0;
        if (enabled && !lf) {
            GetTempPathA(MAX_PATH, tmp);
            lstrcatA(tmp, "slfilter.log");
            lf = fopen(tmp, "a");
        }
    }
    if (!lf) return;
    va_start(ap, fmt);
    vfprintf(lf, fmt, ap);
    va_end(ap);
    fputc('\n', lf);
    fflush(lf);
}

/* ---- DBCS-safe case-insensitive substring (GBK / CP_ACP) ---------- */

#ifndef NORM_IGNORECASE
#define NORM_IGNORECASE  0x00000001
#endif
#ifndef CSTR_EQUAL
#define CSTR_EQUAL       2
#endif
#ifndef LOCALE_USER_DEFAULT
#define LOCALE_USER_DEFAULT 0x0400
#endif

static int contains_ci(const char *hay, const char *needle)
{
    int nl;
    const char *p;
    if (!needle || !(nl = (int)strlen(needle))) return 1;
    if (!hay || !*hay) return 0;
    for (p = hay; *p; p = CharNextA(p)) {
        int left = (int)strlen(p);
        if (left < nl) break;
        if (CompareStringA(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                           p, nl, needle, nl) == CSTR_EQUAL) return 1;
    }
    return 0;
}

/* filter tokens: up to 8, space separated, all must match */
static char g_tok[8][64];
static int  g_ntok;

static void parse_tokens(const char *f)
{
    const char *p = f;
    g_ntok = 0;
    while (*p && g_ntok < 8) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        {
            int n = 0;
            while (*p && *p != ' ' && *p != '\t' && n < 63)
                g_tok[g_ntok][n++] = *p++;
            g_tok[g_ntok][n] = 0;
            if (n) g_ntok++;
        }
    }
}

static int path_matches(const char *path)
{
    int t;
    for (t = 0; t < g_ntok; t++)
        if (!contains_ci(path, g_tok[t])) return 0;
    return 1;
}

/* ---- read-only snapshot -------------------------------------------- */

static SNODE *snap_item(HWND tv, HTREEITEM h, SNODE *parent)
{
    TVITEMA ti;
    char buf[260];
    SNODE *n = (SNODE *)LocalAlloc(LMEM_ZEROINIT, sizeof(SNODE)), **tail;
    HTREEITEM c;
    if (!n) return NULL;
    memset(&ti, 0, sizeof(ti));
    ti.hItem = h;
    ti.mask = TVIF_TEXT;
    ti.pszText = buf;
    ti.cchTextMax = sizeof(buf);
    if (!SendMessageA(tv, TVM_GETITEMA, 0, (LPARAM)&ti)) {
        LocalFree(n);
        return NULL;
    }
    n->h = h;
    n->parent = parent;
    lstrcpynA(n->text, buf, 259);
    if (parent) {
        lstrcpynA(n->path, parent->path, sizeof(n->path) - 1);
        if (n->path[0]) lstrcatA(n->path, "\\");
    }
    lstrcatA(n->path, n->text);
    g_snapCount++;
    c = TreeView_GetChild(tv, h);
    n->isLeaf = (c == NULL);
    tail = &n->child;
    while (c) {
        SNODE *sc = snap_item(tv, c, n);
        if (sc) { *tail = sc; tail = &sc->next; }
        c = TreeView_GetNextSibling(tv, c);
    }
    return n;
}

static void snap_free(SNODE *n)
{
    while (n) {
        SNODE *nx = n->next;
        snap_free(n->child);
        LocalFree(n);
        n = nx;
    }
}

static void snapshot_if_needed(void)
{
    int cnt = TreeView_GetCount(g_tree);
    HTREEITEM h;
    SNODE **tail;
    if (!cnt) return;
    if (g_snap && cnt == g_snapCount) return;   /* unchanged */
    if (g_snap) { snap_free(g_snap); g_snap = NULL; }
    g_snapCount = 0;
    h = TreeView_GetRoot(g_tree);
    tail = &g_snap;
    while (h) {
        SNODE *n = snap_item(g_tree, h, NULL);
        if (n) { *tail = n; tail = &n->next; }
        h = TreeView_GetNextSibling(g_tree, h);
    }
    dbg("snapshot items=%d", g_snapCount);
}

/* ---- jump to an original tree node ---------------------------------- */

/* preview node in original tree without stealing focus or closing list */
static void preview_node(SNODE *n)
{
    HTREEITEM a;
    if (!g_tree || !n || !n->h) return;
    for (a = TreeView_GetParent(g_tree, n->h); a; ) {
        TreeView_Expand(g_tree, a, TVE_EXPAND);
        a = TreeView_GetParent(g_tree, a);
    }
    TreeView_EnsureVisible(g_tree, n->h);
    TreeView_Select(g_tree, n->h, TVGN_CARET);
    dbg("preview [%s]", n->path);
}

static void jump_to(SNODE *n)
{
    HTREEITEM a;
    /* locate only: expand ancestors so the item is visible, then select
     * it (the app's OnChange fires and loads the preview). No simulated
     * mouse events are sent. */
    for (a = TreeView_GetParent(g_tree, n->h); a; ) {
        TreeView_Expand(g_tree, a, TVE_EXPAND);
        a = TreeView_GetParent(g_tree, a);
    }
    TreeView_EnsureVisible(g_tree, n->h);
    TreeView_Select(g_tree, n->h, TVGN_CARET);
    /* unfocused trees draw the selection dark and unreadable; hand focus
     * back so the selection renders like a user-picked item */
    SetFocus(g_tree);
    dbg("jump to [%s]", n->path);
}

/* fill the list box with matches (match against component filename only) */
static void fill_list(const char *f)
{
    SNODE *n;
    int shown = 0;
    parse_tokens(f);
    SendMessageA(g_list, LB_RESETCONTENT, 0, 0);
    for (n = g_snap; n && shown < 4000; ) {
        if (n->isLeaf && path_matches(n->text)) {
            SendMessageA(g_list, LB_ADDSTRING, 0, (LPARAM)n->path);
            shown++;
        }
        /* depth-first iteration without recursion */
        if (n->child) {
            n = n->child;
        } else {
            while (n && !n->next) n = n->parent;
            if (n) n = n->next;
        }
    }
    dbg("filter [%s] -> %d matches (snap=%d)", f, shown, g_snapCount);
}

/* ---- subclass procs ------------------------------------------------ */

/* the tree is hidden while the result list covers it, so the list can
 * never mix with tree text; the tree comes back when filtering ends */
static void hide_list(void)
{
    if (g_list && IsWindowVisible(g_list)) {
        ShowWindow(g_list, SW_HIDE);
        if (g_tree) ShowWindow(g_tree, SW_SHOW);
    }
}

static void list_preview_selected(void)
{
    int idx = (int)SendMessageA(g_list, LB_GETCURSEL, 0, 0);
    char s[700];
    SNODE *n;
    if (idx < 0) return;
    if (SendMessageA(g_list, LB_GETTEXT, (WPARAM)idx, (LPARAM)s) == (LRESULT)LB_ERR) return;
    for (n = g_snap; n; ) {
        if (n->isLeaf && !lstrcmpA(n->path, s)) {
            preview_node(n);
            return;
        }
        if (n->child) {
            n = n->child;
        } else {
            while (n && !n->next) n = n->parent;
            if (n) n = n->next;
        }
    }
}

static void list_jump_selected(void)
{
    int idx = (int)SendMessageA(g_list, LB_GETCURSEL, 0, 0);
    char s[700];
    SNODE *n;
    if (idx < 0) idx = 0;
    SendMessageA(g_list, LB_SETCURSEL, (WPARAM)idx, 0);
    if (SendMessageA(g_list, LB_GETTEXT, (WPARAM)idx, (LPARAM)s) == (LRESULT)LB_ERR) return;
    /* find the snapshot node with this path (list was built from it) */
    for (n = g_snap; n; ) {
        if (n->isLeaf && !lstrcmpA(n->path, s)) {
            hide_list();
            jump_to(n);
            /* done locating: reset the filter for the next search */
            if (g_edit) SetWindowTextA(g_edit, "");
            return;
        }
        if (n->child) {
            n = n->child;
        } else {
            while (n && !n->next) n = n->parent;
            if (n) n = n->next;
        }
    }
}

static void start_drag_from_list(void)
{
    int idx = (int)SendMessageA(g_list, LB_GETCURSEL, 0, 0);
    char s[700];
    SNODE *n = NULL, *cur;
    RECT rc;
    int x, y;

    if (idx < 0) return;
    if (SendMessageA(g_list, LB_GETTEXT, (WPARAM)idx, (LPARAM)s) == (LRESULT)LB_ERR) return;

    for (cur = g_snap; cur; ) {
        if (cur->isLeaf && !lstrcmpA(cur->path, s)) {
            n = cur;
            break;
        }
        if (cur->child) cur = cur->child;
        else {
            while (cur && !cur->next) cur = cur->parent;
            if (cur) cur = cur->next;
        }
    }
    if (!n || !n->h) return;

    /* Ensure node is visible and selected in tree */
    preview_node(n);

    /* Hide list and restore tree */
    ShowWindow(g_list, SW_HIDE);
    if (g_tree) {
        ShowWindow(g_tree, SW_SHOW);
        UpdateWindow(g_tree);

        /* Get item rect in tree client coordinates */
        *(HTREEITEM *)&rc = n->h;
        if (!SendMessageA(g_tree, TVM_GETITEMRECT, TRUE, (LPARAM)&rc)) {
            rc.left = 10; rc.top = 10;
        }
        x = rc.left + 5;
        y = rc.top + 5;

        /* Reset the filter search box */
        if (g_edit) SetWindowTextA(g_edit, "");

        /* Send LBUTTONDOWN then MOUSEMOVE to tree to trigger Delphi VCL BeginDrag */
        SendMessageA(g_tree, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        SendMessageA(g_tree, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x + 10, y + 10));
    }
    dbg("start_drag_from_list [%s]", n->path);
}

static LRESULT CALLBACK listProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_LBUTTONDOWN: {
        LRESULT r = CallWindowProcA(g_oldList, hwnd, msg, wp, lp);
        g_dragPending = 1;
        g_dragStartPt.x = (short)LOWORD(lp);
        g_dragStartPt.y = (short)HIWORD(lp);
        list_preview_selected();
        return r;
    }
    case WM_MOUSEMOVE: {
        if (g_dragPending && (wp & MK_LBUTTON)) {
            int cx = (short)LOWORD(lp);
            int cy = (short)HIWORD(lp);
            if (iabs(cx - g_dragStartPt.x) >= 5 || iabs(cy - g_dragStartPt.y) >= 5) {
                g_dragPending = 0;
                ReleaseCapture();
                start_drag_from_list();
                return 0;
            }
        }
        break;
    }
    case WM_LBUTTONUP: {
        LRESULT r;
        g_dragPending = 0;
        r = CallWindowProcA(g_oldList, hwnd, msg, wp, lp);
        list_preview_selected();
        return r;
    }
    case WM_CAPTURECHANGED:
    case WM_KILLFOCUS:
        g_dragPending = 0;
        break;
    case WM_GETDLGCODE: {
        LRESULT code = CallWindowProcA(g_oldList, hwnd, msg, wp, lp);
        return code | DLGC_WANTARROWS | DLGC_WANTCHARS;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        return 1;
    }
    case WM_MOUSEWHEEL:
    case WM_VSCROLL:
    case WM_HSCROLL: {
        LRESULT r = CallWindowProcA(g_oldList, hwnd, msg, wp, lp);
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
        return r;
    }

    case WM_KEYUP: {
        LRESULT r = CallWindowProcA(g_oldList, hwnd, msg, wp, lp);
        if (wp == VK_UP || wp == VK_DOWN || wp == VK_PRIOR || wp == VK_NEXT ||
            wp == VK_HOME || wp == VK_END) {
            list_preview_selected();
        }
        return r;
    }
    case WM_KEYDOWN:
        if (wp == VK_RETURN) { list_jump_selected(); return 0; }
        if (wp == VK_ESCAPE) {
            SetWindowTextA(g_edit, "");
            SetFocus(g_edit);
            return 0;
        }
        if (wp == VK_UP) {
            int sel = (int)SendMessageA(hwnd, LB_GETCURSEL, 0, 0);
            if (sel <= 0) {
                SetFocus(g_edit);
                return 0;
            }
        }
        if (wp == VK_DOWN || wp == VK_UP || wp == VK_PRIOR || wp == VK_NEXT ||
            wp == VK_HOME || wp == VK_END) {
            LRESULT r = CallWindowProcA(g_oldList, hwnd, msg, wp, lp);
            list_preview_selected();
            return r;
        }
        break;
    case WM_LBUTTONDBLCLK:
        list_jump_selected();
        return 0;
    case WM_DESTROY: {
        WNDPROC old = g_oldList;
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)g_oldList);
        return CallWindowProcA(old, hwnd, msg, wp, lp);
    }
    }
    return CallWindowProcA(g_oldList, hwnd, msg, wp, lp);
}

/* watermark text shown when the edit is empty and unfocused */
static void paint_watermark(HDC hdc)
{
    RECT rc;
    HFONT of;
    GetClientRect(g_edit, &rc);
    of = (HFONT)SelectObject(hdc, g_font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    DrawTextA(hdc, " Filter Components...", -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, of);
}

static LRESULT CALLBACK editProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_GETDLGCODE: {
        LRESULT code = CallWindowProcA(g_oldEdit, hwnd, msg, wp, lp);
        return code | DLGC_WANTARROWS | DLGC_WANTCHARS;
    }
    case SL_FILTER_MSG: {
        char t[260];
        int c1;
        GetWindowTextA(hwnd, t, sizeof(t));
        if (!t[0]) {
            hide_list();
        } else {
            /* population gate: wait until the tree stops changing */
            c1 = TreeView_GetCount(g_tree);
            if (c1 != g_lastCount) {
                g_lastCount = c1;
                SetTimer(hwnd, WM_FILTER_TIMER, 250, NULL);
                return 0;
            }
            snapshot_if_needed();
            sync_edit_pos();
            ShowWindow(g_list, SW_SHOW);
            if (g_tree) ShowWindow(g_tree, SW_HIDE);
            fill_list(t);
            if ((int)SendMessageA(g_list, LB_GETCOUNT, 0, 0) > 0) {
                SendMessageA(g_list, LB_SETCURSEL, 0, 0);
                list_preview_selected();
            }
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == WM_FILTER_TIMER) {
            KillTimer(hwnd, WM_FILTER_TIMER);
            PostMessageA(hwnd, SL_FILTER_MSG, 0, 0);
            return 0;
        }
        break;
    case WM_CHAR:
    case WM_PASTE:
    case WM_CLEAR:
    case WM_CUT:
    case WM_UNDO:
    case WM_SETTEXT: {
        LRESULT r = CallWindowProcA(g_oldEdit, hwnd, msg, wp, lp);
        /* EN_CHANGE goes to the title bar; detect text changes here and
         * debounce on the edit itself */
        if (!g_rebuilding)
            SetTimer(hwnd, WM_FILTER_TIMER, 150, NULL);
        return r;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            SetWindowTextA(hwnd, "");
            SetFocus(g_tree);
            return 0;
        }
        if (wp == VK_RETURN) {
            if (g_list && IsWindowVisible(g_list)) { list_jump_selected(); return 0; }
            return 0;
        }
        if (wp == VK_DOWN && g_list && IsWindowVisible(g_list)) {
            SetFocus(g_list);
            if ((int)SendMessageA(g_list, LB_GETCURSEL, 0, 0) < 0) {
                SendMessageA(g_list, LB_SETCURSEL, 0, 0);
                list_preview_selected();
            }
            return 0;
        }
        break;
    case WM_HOTKEY:
        if (wp == HOTKEY_ID) {
            HWND top = my_GetAncestor(hwnd, GA_ROOT);
            if (top) SetForegroundWindow(top);
            SetFocus(hwnd);
            SendMessageA(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        break;
    case WM_PAINT: {
        char t[260];
        LRESULT r;
        GetWindowTextA(hwnd, t, sizeof(t));
        r = CallWindowProcA(g_oldEdit, hwnd, msg, wp, lp);
        if (!t[0] && GetFocus() != hwnd) {
            HDC hdc = GetDC(hwnd);
            paint_watermark(hdc);
            ReleaseDC(hwnd, hdc);
        }
        return r;
    }
    case WM_DESTROY:
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)g_oldEdit);
        return 0;
    }
    return CallWindowProcA(g_oldEdit, hwnd, msg, wp, lp);
}

/* The title bar hosting the edit: forward everything, but after every
 * paint of the bar re-validate our edit. Host repaints can erase a
 * raw-API child window without notifying it. */
static LRESULT CALLBACK barProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_GETDLGCODE) {
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    }
    if (msg == WM_LBUTTONDOWN) {
        if (g_edit && IsWindow(g_edit)) SetFocus(g_edit);
        return 0;
    }
    if (msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORSTATIC) {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    if (msg == WM_PAINT) {
        LRESULT r = CallWindowProcA(g_oldBar, hwnd, msg, wp, lp);
        if (g_edit && IsWindow(g_edit)) {
            HWND ch;
            for (ch = GetWindow(hwnd, GW_CHILD); ch; ch = GetWindow(ch, GW_HWNDNEXT)) {
                if (ch != g_edit && IsWindowVisible(ch)) {
                    ShowWindow(ch, SW_HIDE);
                    EnableWindow(ch, FALSE);
                }
            }
            InvalidateRect(g_edit, NULL, FALSE);
        }
        return r;
    }
    if (msg == WM_SIZE || msg == WM_WINDOWPOSCHANGED) {
        LRESULT r = CallWindowProcA(g_oldBar, hwnd, msg, wp, lp);
        sync_edit_pos();
        return r;
    }
    if (msg == WM_DESTROY) {
        WNDPROC old = g_oldBar;
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)g_oldBar);
        return CallWindowProcA(old, hwnd, msg, wp, lp);
    }
    return CallWindowProcA(g_oldBar, hwnd, msg, wp, lp);
}

/* The panel hosting the list: provide valid background brush for listbox,
 * and revalidate list on host paint / resize. */
static LRESULT CALLBACK panelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_COMMAND && HIWORD(wp) == LBN_SELCHANGE && (HWND)lp == g_list) {
        list_preview_selected();
        return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
        if (g_edit && IsWindow(g_edit)) {
            POINT pt;
            pt.x = (short)LOWORD(lp);
            pt.y = (short)HIWORD(lp);
            if (pt.y < 25) { SetFocus(g_edit); return 0; }
        }
    }
    if (msg == WM_CTLCOLORLISTBOX) {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    if (msg == WM_PAINT) {
        LRESULT r = CallWindowProcA(g_oldPanel, hwnd, msg, wp, lp);
        if (g_list && IsWindow(g_list) && IsWindowVisible(g_list))
            InvalidateRect(g_list, NULL, TRUE);
        return r;
    }
    if (msg == WM_SIZE || msg == WM_WINDOWPOSCHANGED) {
        LRESULT r = CallWindowProcA(g_oldPanel, hwnd, msg, wp, lp);
        sync_edit_pos();
        return r;
    }
    if (msg == WM_DESTROY) {
        WNDPROC old = g_oldPanel;
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)g_oldPanel);
        return CallWindowProcA(old, hwnd, msg, wp, lp);
    }
    return CallWindowProcA(g_oldPanel, hwnd, msg, wp, lp);
}

/* keep edit + list glued to the title bar / tree */
static void sync_edit_pos(void)
{
    RECT bt, rt, cr;
    int moved = 0;
    if (!g_edit || !g_tree) return;

    /* edit: fills the title bar right of the caption text */
    if (g_bar) {
        int bw, bh, ew, eh;
        GetClientRect(g_bar, &bt);
        GetWindowRect(g_tree, &rt);
        MapWindowPoints(NULL, g_bar, (POINT *)&rt, 2);
        bw = bt.right;
        bh = (g_bar == g_panel) ? rt.top : bt.bottom;
        ew = bw - 2;
        if (ew < 40) ew = 40;
        eh = (bh - 2) > EDIT_MINH ? (bh - 2) : EDIT_MINH;
        GetWindowRect(g_edit, &cr);
        MapWindowPoints(NULL, g_bar, (POINT *)&cr, 2);
        if (cr.left != 1 || cr.top != 1 ||
            cr.right - cr.left != ew || cr.bottom - cr.top != eh) {
            SetWindowPos(g_edit, HWND_TOP, 1, 1, ew, eh, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            moved = 1;
        }
    }

    /* list: matches the tree dimensions and position exactly */
    if (g_list && g_panel) {
        int ew, eh, left, top;
        GetWindowRect(g_tree, &rt);
        MapWindowPoints(NULL, g_panel, (POINT *)&rt, 2);
        left = rt.left;
        top = rt.top;
        ew = rt.right - rt.left;
        eh = rt.bottom - rt.top;
        GetWindowRect(g_list, &cr);
        MapWindowPoints(NULL, g_panel, (POINT *)&cr, 2);
        if (moved || cr.left != left || cr.top != top ||
            (cr.right - cr.left) != ew || (cr.bottom - cr.top) != eh) {
            SetWindowPos(g_list, g_tree, left, top, ew, eh, SWP_NOACTIVATE);
        }
    }
}

static LRESULT CALLBACK treeProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case SL_INSTALL_MSG:
        install_on_ui_thread(hwnd);
        return 0;
    case WM_SIZE:
    case WM_WINDOWPOSCHANGED:
        sync_edit_pos();
        break;
    case WM_DESTROY: {
        WNDPROC old = g_oldTree;
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)g_oldTree);
        return CallWindowProcA(old, hwnd, msg, wp, lp);
    }
    }
    return CallWindowProcA(g_oldTree, hwnd, msg, wp, lp);
}

/* ---- install -------------------------------------------------------- */

struct FindCtx { HWND tree; };

static BOOL CALLBACK enumTreeProc(HWND h, LPARAM lp)
{
    char cls[64];
    struct FindCtx *ctx = (struct FindCtx *)lp;
    /* Sprint-Layout's makro tree (TTreeViewx3M) registers its window class
     * as "TTreeView"; a stock VCL tree would be "SysTreeView32". */
    if (GetClassNameA(h, cls, sizeof(cls)) &&
        (strstr(cls, "TreeView") != NULL || !lstrcmpA(cls, "SysTreeView32"))) {
        ctx->tree = h;
        return FALSE;
    }
    return TRUE;
}

static BOOL CALLBACK enumTopProc(HWND h, LPARAM lp)
{
    DWORD pid = 0;
    struct FindCtx *ctx = (struct FindCtx *)lp;
    GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(h)) {
        EnumChildWindows(h, enumTreeProc, lp);
        if (ctx->tree) return FALSE;
    }
    return TRUE;
}

static int contains_wcs(const WCHAR *hay, const WCHAR *needle)
{
    int i, j, lh = 0, ln = 0;
    while (hay[lh]) lh++;
    while (needle[ln]) ln++;
    if (ln > lh) return 0;
    for (i = 0; i <= lh - ln; i++) {
        for (j = 0; j < ln; j++) {
            if (hay[i + j] != needle[j]) break;
        }
        if (j == ln) return 1;
    }
    return 0;
}

struct BarFindCtx {
    HWND tree;
    RECT treeRect;
    HWND found;
};

/* Geometric check: sits directly above tree with matching horizontal span */
static BOOL CALLBACK enumBarGeoProc(HWND h, LPARAM lp)
{
    struct BarFindCtx *ctx = (struct BarFindCtx *)lp;
    RECT r;
    if (h == ctx->tree) return TRUE;
    GetWindowRect(h, &r);
    if (iabs(r.bottom - ctx->treeRect.top) <= 5 &&
        (r.bottom - r.top) >= 10 && (r.bottom - r.top) <= 45 &&
        iabs(r.left - ctx->treeRect.left) <= 10 &&
        (r.right - r.left) >= 60) {
        ctx->found = h;
        return FALSE;
    }
    return TRUE;
}

/* Multi-language title bar text fallback */
static BOOL CALLBACK enumBarTextProc(HWND h, LPARAM lp)
{
    char txt[128];
    WCHAR wtxt[128];
    struct BarFindCtx *ctx = (struct BarFindCtx *)lp;
    RECT r;
    int match = 0;

    if (h == ctx->tree) return TRUE;

    if (GetWindowTextA(h, txt, sizeof(txt)) > 0) {
        if (strstr(txt, "Macros") ||                            /* English (case-sensitive) */
            strstr(txt, "Makros") ||                            /* German  (case-sensitive) */
            strstr(txt, "\xd0\x9c\xd0\xb0\xd0\xba\xd1\x80\xd0\xbe\xd1\x81\xd1\x8b") || /* Russian UTF-8: Макросы */
            strstr(txt, "\xcc\xe0\xea\xf0\xee\xf1\xfb") ||     /* Russian CP1251: Макросы */
            strstr(txt, "\xd4\xaa\xbc\xfe\xbf\xe2") ||         /* Chinese GBK: 元件库 */
            strstr(txt, "\xe5\x85\x83\xe4\xbb\xb6\xe5\xba\x93")) { /* Chinese UTF-8: 元件库 */
            match = 1;
        }
    }
    if (!match && GetWindowTextW(h, wtxt, sizeof(wtxt) / sizeof(WCHAR)) > 0) {
        if (contains_wcs(wtxt, L"Macros") ||                    /* English UTF-16 */
            contains_wcs(wtxt, L"Makros") ||                    /* German UTF-16 */
            contains_wcs(wtxt, L"\x041c\x0430\x043a\x0440\x043e\x0441\x044b") || /* Russian UTF-16: Макросы */
            contains_wcs(wtxt, L"\x5143\x4ef6\x5e93")) {         /* Chinese UTF-16: 元件库 */
            match = 1;
        }
    }

    if (match) {
        GetWindowRect(h, &r);
        if ((r.bottom - r.top) >= 10 && (r.bottom - r.top) <= 50 &&
            (r.right - r.left) >= 60) {
            ctx->found = h;
            return FALSE;
        }
    }
    return TRUE;
}

/* locate the component library title bar:
 * 1. Title bar text match ("Macros", "Makros", "Макросы", "元件库") - primary!
 * 2. Geometric direct sibling inside macro panel docked directly above tree - fallback 1!
 * 3. Parent container with integrated title strip directly above tree (top 15-45px) - fallback 2!
 * 4. Return NULL if not found! */
static HWND find_title_bar(HWND main, HWND tree)
{
    HWND parent, child;
    RECT rt, rp, r;
    struct BarFindCtx ctx;
    int top_strip;

    if (!tree || !IsWindow(tree)) return NULL;
    parent = GetParent(tree);
    if (!parent || !IsWindow(parent)) return NULL;
    GetWindowRect(tree, &rt);
    GetWindowRect(parent, &rp);

    /* 1. Primary: search title bar text among descendants of main */
    if (main && IsWindow(main)) {
        ZeroMemory(&ctx, sizeof(ctx));
        ctx.tree = tree;
        ctx.treeRect = rt;
        EnumChildWindows(main, enumBarTextProc, (LPARAM)&ctx);
        if (ctx.found) {
            HWND h = ctx.found;
            HWND par = GetParent(h);
            /* In Delphi VCL, the text is often inside a child TPanel (e.g. pnlCaption)
             * whose direct parent is the title bar container. Check if par is docked above tree. */
            if (par && GetParent(par) == parent) {
                GetWindowRect(par, &r);
                if (iabs(r.bottom - rt.top) <= 5 && (r.bottom - r.top) >= 10 && (r.bottom - r.top) <= 45) {
                    dbg("found title bar container by text: hwnd=%p (caption=%p)", par, h);
                    return par;
                }
            }
            if (GetParent(h) == parent) {
                GetWindowRect(h, &r);
                if (iabs(r.bottom - rt.top) <= 5 && (r.bottom - r.top) >= 10 && (r.bottom - r.top) <= 45) {
                    dbg("found title bar by text: hwnd=%p", h);
                    return h;
                }
            }
            dbg("found title bar by text hit: hwnd=%p", h);
            return h;
        }
    }

    /* 2. Fallback: Geometric direct sibling inside tree parent docked above tree */
    for (child = GetWindow(parent, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        if (child == tree) continue;
        GetWindowRect(child, &r);
        if (iabs(r.bottom - rt.top) <= 5 &&
            (r.bottom - r.top) >= 10 && (r.bottom - r.top) <= 45 &&
            iabs(r.left - rt.left) <= 10 &&
            (r.right - r.left) >= 60) {
            dbg("found title bar by sibling geometry fallback: hwnd=%p", child);
            return child;
        }
    }

    /* 3. Fallback: Parent container has integrated title strip directly above tree */
    top_strip = rt.top - rp.top;
    if (top_strip >= 12 && top_strip <= 45 && (rp.right - rp.left) >= 60) {
        dbg("found integrated title strip in parent: hwnd=%p strip_height=%d", parent, top_strip);
        return parent;
    }

    /* 4. Not found: return NULL */
    dbg("title bar not found, returning NULL");
    return NULL;
}

/* Thread-level message hook to ensure arrow keys, Enter, Esc are never
 * stolen by Delphi VCL form's DialogKey / SelectNext message loop */
static HHOOK g_hGetMsgHook = NULL;

static LRESULT CALLBACK getMsgHookProc(int nCode, WPARAM wp, LPARAM lp)
{
    if (nCode >= 0 && wp == PM_REMOVE) {
        MSG *msg = (MSG *)lp;
        if (msg->message == WM_HOTKEY && (int)msg->wParam == HOTKEY_ID) {
            if (g_edit && IsWindow(g_edit)) {
                HWND top = my_GetAncestor(g_edit, GA_ROOT);
                if (top) SetForegroundWindow(top);
                SetFocus(g_edit);
                SendMessageA(g_edit, EM_SETSEL, 0, -1);
                msg->message = WM_NULL;
                return 0;
            }
        }
        if (msg->hwnd == g_edit && g_edit && IsWindow(g_edit)) {
            if (msg->message == WM_KEYDOWN) {
                if (msg->wParam == VK_LEFT || msg->wParam == VK_RIGHT ||
                    msg->wParam == VK_HOME || msg->wParam == VK_END) {
                    CallWindowProcA(g_oldEdit, g_edit, msg->message, msg->wParam, msg->lParam);
                    msg->message = WM_NULL;
                    return 0;
                }
                if (msg->wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                    SendMessageA(g_edit, EM_SETSEL, 0, -1);
                    msg->message = WM_NULL;
                    return 0;
                }
                if (msg->wParam == VK_DOWN) {
                    if (g_list && IsWindowVisible(g_list)) {
                        SetFocus(g_list);
                        if ((int)SendMessageA(g_list, LB_GETCURSEL, 0, 0) < 0) {
                            SendMessageA(g_list, LB_SETCURSEL, 0, 0);
                            list_preview_selected();
                        }
                    }
                    msg->message = WM_NULL;
                    return 0;
                }
                if (msg->wParam == VK_UP) {
                    CallWindowProcA(g_oldEdit, g_edit, msg->message, msg->wParam, msg->lParam);
                    msg->message = WM_NULL;
                    return 0;
                }
                if (msg->wParam == VK_RETURN) {
                    if (g_list && IsWindowVisible(g_list)) {
                        list_jump_selected();
                    }
                    msg->message = WM_NULL;
                    return 0;
                }
                if (msg->wParam == VK_ESCAPE) {
                    SetWindowTextA(g_edit, "");
                    SetFocus(g_tree);
                    msg->message = WM_NULL;
                    return 0;
                }
            } else if (msg->message == WM_KEYUP) {
                if (msg->wParam == VK_LEFT || msg->wParam == VK_RIGHT ||
                    msg->wParam == VK_UP || msg->wParam == VK_DOWN ||
                    msg->wParam == VK_HOME || msg->wParam == VK_END ||
                    msg->wParam == VK_RETURN || msg->wParam == VK_ESCAPE) {
                    CallWindowProcA(g_oldEdit, g_edit, msg->message, msg->wParam, msg->lParam);
                    msg->message = WM_NULL;
                    return 0;
                }
            }
        } else if (msg->hwnd == g_list && g_list && IsWindowVisible(g_list)) {
            if (msg->message == WM_KEYDOWN) {
                if (msg->wParam == VK_LEFT || msg->wParam == VK_RIGHT) {
                    msg->message = WM_NULL;
                    return 0;
                }
                if (msg->wParam == VK_UP) {
                    int sel = (int)SendMessageA(g_list, LB_GETCURSEL, 0, 0);
                    if (sel <= 0) {
                        SetFocus(g_edit);
                        msg->message = WM_NULL;
                        return 0;
                    }
                    CallWindowProcA(g_oldList, g_list, msg->message, msg->wParam, msg->lParam);
                    list_preview_selected();
                    msg->message = WM_NULL;
                    return 0;
                }
                if (msg->wParam == VK_DOWN) {
                    CallWindowProcA(g_oldList, g_list, msg->message, msg->wParam, msg->lParam);
                    list_preview_selected();
                    msg->message = WM_NULL;
                    return 0;
                }
                if (msg->wParam == VK_PRIOR || msg->wParam == VK_NEXT ||
                    msg->wParam == VK_HOME || msg->wParam == VK_END) {
                    CallWindowProcA(g_oldList, g_list, msg->message, msg->wParam, msg->lParam);
                    list_preview_selected();
                    msg->message = WM_NULL;
                    return 0;
                }
                if (msg->wParam == VK_RETURN) {
                    list_jump_selected();
                    msg->message = WM_NULL;
                    return 0;
                }
                if (msg->wParam == VK_ESCAPE) {
                    SetWindowTextA(g_edit, "");
                    SetFocus(g_edit);
                    msg->message = WM_NULL;
                    return 0;
                }
            } else if (msg->message == WM_KEYUP) {
                if (msg->wParam == VK_UP || msg->wParam == VK_DOWN ||
                    msg->wParam == VK_PRIOR || msg->wParam == VK_NEXT ||
                    msg->wParam == VK_HOME || msg->wParam == VK_END ||
                    msg->wParam == VK_RETURN || msg->wParam == VK_ESCAPE) {
                    CallWindowProcA(g_oldList, g_list, msg->message, msg->wParam, msg->lParam);
                    msg->message = WM_NULL;
                    return 0;
                }
            }
        }
    }
    return CallNextHookEx(g_hGetMsgHook, nCode, wp, lp);
}

/* runs on the app's UI thread (invoked via SL_INSTALL_MSG in treeProc):
 * create the filter edit + result list, hook hotkey */
static void install_on_ui_thread(HWND tree)
{
    RECT bt, rt;
    HWND parent;
    int ew, eh;
    if (!IsWindow(tree)) return;
    g_tree = tree;
    parent = GetParent(tree);
    if (!parent || !IsWindow(parent)) { dbg("no parent for list"); return; }

    /* title bar that hosts the edit (the component library caption) */
    g_bar = find_title_bar(GetAncestor(tree, GA_ROOT), tree);
    dbg("title bar hwnd=%p", g_bar);
    if (!g_bar || !IsWindow(g_bar)) {
        dbg("title bar not found, aborting install");
        return;
    }

    g_font = CreateFontA(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    if (!g_font)
        g_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    GetClientRect(g_bar, &bt);
    GetWindowRect(tree, &rt);
    MapWindowPoints(NULL, g_bar, (POINT *)&rt, 2);
    /* the edit becomes the full title bar strip - a search box */
    ew = bt.right - 2;
    if (ew < 40) ew = 40;
    eh = (g_bar == parent ? rt.top : (bt.bottom - bt.top)) - 2;
    if (eh < EDIT_MINH) eh = EDIT_MINH;
    /* WS_BORDER = thin black frame, WS_CLIPSIBLINGS = clip overlapping siblings */
    g_edit = CreateWindowExA(0, "EDIT", "",
                             WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_BORDER | WS_CLIPSIBLINGS,
                             1, 1, ew, eh,
                             g_bar, NULL, NULL, NULL);
    if (g_edit) {
        HWND ch;
        /* Ensure g_edit sits on top in Z-order inside g_bar */
        SetWindowPos(g_edit, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        /* Hide and disable any existing sibling caption panel inside g_bar
         * so it cannot intercept mouse clicks or paint over the edit */
        for (ch = GetWindow(g_bar, GW_CHILD); ch; ch = GetWindow(ch, GW_HWNDNEXT)) {
            if (ch != g_edit && ch != tree) {
                ShowWindow(ch, SW_HIDE);
                EnableWindow(ch, FALSE);
            }
        }
        SendMessageA(g_edit, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
        g_oldEdit = (WNDPROC)SetWindowLongPtrA(g_edit, GWLP_WNDPROC, (LONG_PTR)editProc);
        if (g_bar != parent)
            g_oldBar = (WNDPROC)SetWindowLongPtrA(g_bar, GWLP_WNDPROC, (LONG_PTR)barProc);

        if (!g_hGetMsgHook) {
            g_hGetMsgHook = SetWindowsHookExA(WH_GETMESSAGE, getMsgHookProc, NULL, GetCurrentThreadId());
            dbg("installed getMsgHook=%p", g_hGetMsgHook);
        }
    } else {
        dbg("edit create failed");
    }

    /* result list: matches the tree dimensions and position exactly */
    GetWindowRect(tree, &rt);
    MapWindowPoints(NULL, parent, (POINT *)&rt, 2);
    ew = rt.right - rt.left;
    eh = rt.bottom - rt.top;
    g_list = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
                             WS_CHILD | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY |
                             LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT | WS_CLIPSIBLINGS,
                             rt.left, rt.top, ew, eh,
                             parent, NULL, NULL, NULL);
    if (g_list) {
        SendMessageA(g_list, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
        g_oldList = (WNDPROC)SetWindowLongPtrA(g_list, GWLP_WNDPROC, (LONG_PTR)listProc);
        g_panel = parent;
        g_oldPanel = (WNDPROC)SetWindowLongPtrA(parent, GWLP_WNDPROC, (LONG_PTR)panelProc);
    } else {
        dbg("list create failed");
    }

    if (g_edit) {
        BOOL ok = RegisterHotKey(g_edit, HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'F');
        if (!ok) {
            ok = RegisterHotKey(g_edit, HOTKEY_ID, MOD_CONTROL | MOD_ALT, 'F');
        }
        dbg("RegisterHotKey(Ctrl+Alt+F) = %d", ok);
    }
    dbg("installed edit=%p list=%p bar=%p", g_edit, g_list, g_bar);
}

/* Check whether TInfoDlg (splash window) is visible */
static BOOL CALLBACK enumSplashProc(HWND h, LPARAM lp)
{
    char cls[64];
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(h)) return TRUE;
    if (GetClassNameA(h, cls, sizeof(cls)) && !lstrcmpA(cls, "TInfoDlg")) {
        *(BOOL *)lp = TRUE;
        return FALSE;
    }
    return TRUE;
}

static BOOL is_splash_visible(void)
{
    BOOL visible = FALSE;
    EnumWindows(enumSplashProc, (LPARAM)&visible);
    return visible;
}

/* worker thread: locate the makro tree, wait until splash is dismissed
 * and tree population completes, then install on UI thread cleanly */
static DWORD WINAPI worker(LPVOID p)
{
    struct FindCtx ctx;
    int tries;
    int count = 0, lastCount = -1, stableRounds = 0;
    (void)p;

    dbg("worker thread started");

    /* 1. Wait for makro treeview to appear */
    for (tries = 0; tries < 300; tries++) {
        ZeroMemory(&ctx, sizeof(ctx));
        EnumWindows(enumTopProc, (LPARAM)&ctx);
        if (ctx.tree) break;
        Sleep(100);
    }
    if (!ctx.tree) { dbg("no treeview found after 30s"); return 0; }
    g_tree = ctx.tree;
    dbg("tree found hwnd=%p", g_tree);

    /* 2. Wait until splash window (TInfoDlg) closes */
    dbg("step 2: waiting for splash to close");
    for (tries = 0; tries < 300; tries++) {
        if (!IsWindow(g_tree)) return 0;  /* app closed rapidly */
        if (!is_splash_visible()) break;
        Sleep(100);
    }
    dbg("step 2 done: splash closed after %d tries", tries);

    /* 3. Wait until tree loading finishes (count > 0 and stable for 2 checks) */
    for (tries = 0; tries < 150; tries++) {
        if (!IsWindow(g_tree)) return 0;  /* app closed rapidly */
        Sleep(200);
        count = (int)SendMessageA(g_tree, TVM_GETCOUNT, 0, 0);
        if (count > 0) {
            if (count == lastCount) {
                stableRounds++;
                if (stableRounds >= 2) break;
            } else {
                lastCount = count;
                stableRounds = 0;
            }
        }
    }
    if (!IsWindow(g_tree)) return 0;
    dbg("startup finished, count=%d, installing", count);

    /* Subclass tree and trigger installation on UI thread */
    g_oldTree = (WNDPROC)SetWindowLongPtrA(g_tree, GWLP_WNDPROC, (LONG_PTR)treeProc);
    PostMessageA(g_tree, SL_INSTALL_MSG, 0, 0);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    (void)hInst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        dbg("DLL_PROCESS_ATTACH pid=%lu", GetCurrentProcessId());
        CreateThread(NULL, 0, worker, NULL, 0, NULL);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_hGetMsgHook) {
            UnhookWindowsHookEx(g_hGetMsgHook);
            g_hGetMsgHook = NULL;
        }
        if (g_edit) { UnregisterHotKey(g_edit, HOTKEY_ID); }
    }
    return TRUE;
}