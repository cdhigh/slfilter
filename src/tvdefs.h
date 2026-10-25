/* tvdefs.h - Cross-compiler Win32 compatibility definitions
 */
#ifndef TVDEFS_H
#define TVDEFS_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifndef HTREEITEM
typedef HANDLE HTREEITEM;
#endif

/* TreeView messages */
#ifndef TVM_INSERTITEMA
#define TVM_INSERTITEMA      0x1100
#define TVM_DELETEITEM       0x1101
#define TVM_EXPAND           0x1102
#define TVM_GETITEMRECT      0x1104
#define TVM_GETCOUNT         0x1105
#define TVM_GETNEXTITEM      0x110A
#define TVM_SELECTITEM       0x110B
#define TVM_GETITEMA         0x110C
#define TVM_GETITEMW         0x110D
#define TVM_ENSUREVISIBLE    0x1113

#define TVIF_TEXT          0x0001
#define TVIF_IMAGE         0x0002
#define TVIF_PARAM         0x0004
#define TVIF_STATE         0x0008
#define TVIF_SELECTEDIMAGE 0x0020

#define TVIS_EXPANDED      0x0020

#define TVGN_ROOT            0
#define TVGN_NEXT            1
#define TVGN_PARENT          3
#define TVGN_CHILD           4
#define TVGN_CARET           9

#define TVI_ROOT   ((HTREEITEM)(LONG_PTR)0xFFFF0000)
#define TVI_LAST   ((HTREEITEM)(LONG_PTR)0xFFFF0002)

#define TVE_EXPAND  0x0001
#endif

/* ListBox definitions */
#ifndef LB_ADDSTRING
#define LB_ADDSTRING         0x0180
#endif
#ifndef LB_RESETCONTENT
#define LB_RESETCONTENT      0x0184
#endif
#ifndef LB_GETCURSEL
#define LB_GETCURSEL         0x0188
#endif
#ifndef LB_GETTEXT
#define LB_GETTEXT           0x0189
#endif
#ifndef LB_GETCOUNT
#define LB_GETCOUNT          0x018B
#endif
#ifndef LB_ERR
#define LB_ERR               (-1)
#endif
#ifndef LBS_NOINTEGRALHEIGHT
#define LBS_NOINTEGRALHEIGHT 0x0100L
#endif

/* Dialog codes */
#ifndef DLGC_WANTARROWS
#define DLGC_WANTARROWS      0x0001
#endif
#ifndef DLGC_WANTTAB
#define DLGC_WANTTAB         0x0002
#endif
#ifndef DLGC_WANTALLKEYS
#define DLGC_WANTALLKEYS     0x0004
#endif
#ifndef DLGC_WANTCHARS
#define DLGC_WANTCHARS       0x0080
#endif

/* Compatibility for older headers (VC6 / early MinGW) */
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL        0x020A
#endif

#ifndef GA_ROOT
#define GA_PARENT            1
#define GA_ROOT              2
#define GA_ROOTOWNER         3
#endif

static HWND my_GetAncestor(HWND h, UINT flags)
{
    typedef HWND (WINAPI *PFN_GA)(HWND, UINT);
    static PFN_GA pfn = NULL;
    if (!pfn) pfn = (PFN_GA)GetProcAddress(GetModuleHandleA("user32.dll"), "GetAncestor");
    if (pfn) return pfn(h, flags);
    while (h && GetParent(h)) h = GetParent(h);
    return h;
}
#define GetAncestor my_GetAncestor

#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY    5
#endif

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT         0x4000
#endif

#if defined(__TINYC__)
int WINAPI CompareStringA(LCID Locale, DWORD dwCmpFlags, LPCSTR lpString1, int cchCount1, LPCSTR lpString2, int cchCount2);
#endif

#ifndef GWLP_WNDPROC
#define GWLP_WNDPROC         (-4)
#endif
#ifndef SetWindowLongPtrA
#define SetWindowLongPtrA(h, n, v) ((WNDPROC)(LONG_PTR)SetWindowLongA((h), (n), (LONG)(LONG_PTR)(v)))
#endif
#ifndef GetWindowLongPtrA
#define GetWindowLongPtrA(h, n)    ((LONG_PTR)GetWindowLongA((h), (n)))
#endif

#ifndef TVITEMA
typedef struct _TV_ITEMA {
    UINT      mask;
    HTREEITEM hItem;
    UINT      state;
    UINT      stateMask;
    LPSTR     pszText;
    int       cchTextMax;
    int       iImage;
    int       iSelectedImage;
    int       cChildren;
    LPARAM    lParam;
} TVITEMA, *LPTVITEMA;
#endif

#ifndef TreeView_GetItem
static HTREEITEM TV_GetItem(HWND tv, HTREEITEM h, TVITEMA *ti)
{ return (HTREEITEM)SendMessageA(tv, TVM_GETITEMA, 0, (LPARAM)ti) ? h : NULL; }

#define TreeView_GetItem(tv, ti)       TV_GetItem(tv, (ti)->hItem, ti)
#define TreeView_GetRoot(tv)           ((HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_ROOT, 0))
#define TreeView_GetChild(tv, h)       ((HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)h))
#define TreeView_GetNextSibling(tv, h) ((HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)h))
#define TreeView_GetSelection(tv)      ((HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_CARET, 0))
#define TreeView_GetParent(tv, h)      ((HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_PARENT, (LPARAM)h))
#define TreeView_Select(tv, h, f)      ((BOOL)SendMessageA(tv, TVM_SELECTITEM, f, (LPARAM)h))
#define TreeView_Expand(tv, h, f)      ((BOOL)SendMessageA(tv, TVM_EXPAND, f, (LPARAM)h))
#define TreeView_GetCount(tv)          ((UINT)SendMessageA(tv, TVM_GETCOUNT, 0, 0))
#define TreeView_EnsureVisible(tv, h)  ((BOOL)SendMessageA(tv, TVM_ENSUREVISIBLE, 0, (LPARAM)h))
#endif

#endif /* TVDEFS_H */
