#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#include <map>
#include <string>
#include "tail.h"
#include "version.h"

// Minimal, public Notepad++ plugin ABI. No SDK binaries required.
struct NppData { HWND npp, main, second; };
struct ShortcutKey { bool ctrl, alt, shift; UCHAR key; };
struct FuncItem {
    wchar_t name[64]; void (__cdecl *command)(); int id;
    bool checked; ShortcutKey* shortcut;
};
namespace sci {
enum : UINT {
    GetStyleAt=2010, GetLine=2153, GetLineCount=2154, PositionFromLine=2167,
    LineLength=2350, GetLexer=4002, GetLastChild=2224, GetLineVisible=2228,
    GetFoldExpanded=2230, ToggleFold=2231, GetFoldLevel=2223,
    GetFirstVisibleLine=2152, LinesOnScreen=2370, DocLineFromVisible=2221,
    ToggleFoldShowText=2700, FoldDisplayTextSetStyle=2701,
    FoldDisplayTextGetStyle=2707, GetDocPointer=2357
};
}
static NppData npp{};
static bool enabled = true;
struct Visual {
    RECT mask{}, dots{};
    int x{}, y{}, height{}, style{};
    std::wstring text, tip;
    LRESULT doc{};
};
struct View {
    HWND window{};
    UINT_PTR timer{};
    LRESULT doc{};
    int originalStyle{};
    bool busy{};
    WPARAM originalFoldFlags=16; // Notepad++ default: line after contracted fold.
    bool coloursCaptured{};
    bool colourSet[2]{};
    LRESULT colours[2]{};
    std::map<LRESULT, Visual> visuals;
    HWND tooltip{};
    LRESULT pressed=-1;
};
static View views[2];
static LRESULT send(HWND w, UINT m, WPARAM a=0, LPARAM b=0) {
    return SendMessage(w,m,a,b);
}
static void suppressLines(View& v) {
    for(int i=0;i<2;++i) {
        auto element=80+i; // SC_ELEMENT_FOLD_LINE / SC_ELEMENT_HIDDEN_LINE.
        bool set=send(v.window,2756,element)!=0; // SCI_GETELEMENTISSET
        auto colour=send(v.window,2754,element); // SCI_GETELEMENTCOLOUR
        if(!v.coloursCaptured) { v.colourSet[i]=set; v.colours[i]=colour; }
        if(!set || colour!=0) send(v.window,2753,element,0); // transparent RGBA
    }
    v.coloursCaptured=true;
}
static void restoreLines(View& v) {
    if(!v.coloursCaptured) return;
    for(int i=0;i<2;++i) {
        if(v.colourSet[i]) send(v.window,2753,80+i,v.colours[i]);
        else send(v.window,2755,80+i); // SCI_RESETELEMENTCOLOUR
    }
    v.coloursCaptured=false;
}
static std::string readLine(HWND w, LRESULT line) {
    auto n = send(w,sci::LineLength,line);
    if (n <= 0 || n > 8192) return "";
    std::string s(static_cast<size_t>(n)+1,'\0');
    auto copied=send(w,sci::GetLine,line,reinterpret_cast<LPARAM>(s.data()));
    s.resize(static_cast<size_t>(std::max<LRESULT>(0,copied)));
    return s;
}
struct TailInfo { std::string suffix, raw; };
static TailInfo label(HWND w, LRESULT header) {
    int lexer=static_cast<int>(send(w,sci::GetLexer));
    if (!foldtail::supported(lexer)) return {};
    auto last=send(w,sci::GetLastChild,header,-1);
    // Stay strictly inside the hidden range: do not duplicate visible closing lines.
    for (auto line=last; line>header && line>=last-8; --line) {
        if (send(w,sci::GetLineVisible,line)) return {};
        auto s=readLine(w,line);
        auto pos=send(w,sci::PositionFromLine,line);
        bool code=false;
        for(size_t i=0;i<s.size();++i)
            if (!std::isspace(static_cast<unsigned char>(s[i])) &&
                !foldtail::comment(lexer,static_cast<int>(send(w,sci::GetStyleAt,pos+i))))
                code=true;
        if (!code) continue;
        auto result=foldtail::tail(s,lexer,[&](size_t i){
            return static_cast<int>(send(w,sci::GetStyleAt,pos+i));
        });
        return {result,s};
    }
    return {};
}
#include "visual.h"
static LRESULT CALLBACK subclass(HWND w,UINT msg,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR ref) {
    auto& v=*reinterpret_cast<View*>(ref);
    if(msg==WM_PAINT) {
        auto result=DefSubclassProc(w,msg,a,b);
        try { update(v); paintVisuals(v); } catch (...) { v.busy=false; }
        return result;
    }
    if(enabled && (msg==WM_LBUTTONDOWN || msg==WM_LBUTTONUP)) {
        try { update(v); } catch (...) { v.busy=false; }
        auto line=hit(v,GET_X_LPARAM(b),GET_Y_LPARAM(b));
        if(msg==WM_LBUTTONDOWN && line>=0) {
            v.pressed=line; SetFocus(w); SetCapture(w); return 0;
        }
        if(msg==WM_LBUTTONUP && v.pressed>=0) {
            auto pressed=v.pressed;v.pressed=-1;
            if(GetCapture()==w) ReleaseCapture();
            if(line==pressed && send(w,sci::GetDocPointer)==v.doc && !send(w,sci::GetFoldExpanded,line)) {
                send(w,sci::ToggleFold,line);
                send(v.tooltip,TTM_POP);
                clearVisuals(v);
                InvalidateRect(w,nullptr,FALSE);
            }
            return 0;
        }
    }
    if(msg==WM_CAPTURECHANGED) v.pressed=-1;
    if(msg==WM_SETCURSOR && enabled) {
        POINT p{};GetCursorPos(&p);ScreenToClient(w,&p);
        if(hit(v,p.x,p.y)>=0) {SetCursor(LoadCursor(nullptr,IDC_HAND));return TRUE;}
    }
    if(msg==2233) { // SCI_SETFOLDFLAGS: remember host updates, suppress fold lines.
        v.originalFoldFlags=a;
        if(enabled) a=0;
    }
    if(msg==WM_TIMER && a==v.timer) {
        // Never let allocation failures escape a Windows callback.
        try { update(v); } catch (...) { v.busy=false; send(w,WM_SETREDRAW,TRUE); }
        return 0;
    }
    if(msg==WM_NCDESTROY) {
        clearVisuals(v); if(v.tooltip) DestroyWindow(v.tooltip); v.tooltip=nullptr;
        KillTimer(w,v.timer); RemoveWindowSubclass(w,subclass,id); v.window=nullptr;
    }
    return DefSubclassProc(w,msg,a,b);
}
static void start() {
    HWND handles[]={npp.main,npp.second};
    for(int i=0;i<2;++i) {
        auto& v=views[i];
        if(v.window || !IsWindow(handles[i])) continue;
        v.window=handles[i];
        v.originalStyle=static_cast<int>(send(v.window,sci::FoldDisplayTextGetStyle));
        if(SetWindowSubclass(v.window,subclass,0x46544149,reinterpret_cast<DWORD_PTR>(&v))) {
            v.timer=SetTimer(v.window,0x46544149,200,nullptr);
            send(v.window,sci::FoldDisplayTextSetStyle,0);
            send(v.window,2233,v.originalFoldFlags);
            suppressLines(v);
            createTooltip(v);
        }
    }
}
static void toggle();
static void about() {
    MessageBoxW(npp.npp,L"NppFoldTail " NPPFOLDTAIL_VERSION_W L" (x64)\n\nCompact folding previews for Notepad++ Plugin.",L"NppFoldTail",MB_OK);
}
static FuncItem commands[]={
    {L"Enabled",toggle,0,true,nullptr},
    {L"About NppFoldTail",about,0,false,nullptr}
};
static void toggle() {
    enabled=!enabled;
    CheckMenuItem(GetMenu(npp.npp),commands[0].id,MF_BYCOMMAND|(enabled?MF_CHECKED:MF_UNCHECKED));
    for(auto& v:views) if(IsWindow(v.window)) {
        clearVisuals(v);
        send(v.window,sci::FoldDisplayTextSetStyle,enabled?0:v.originalStyle);
        send(v.window,2233,v.originalFoldFlags);
        if(enabled) suppressLines(v); else restoreLines(v);
        InvalidateRect(v.window,nullptr,FALSE);
    }
}
extern "C" {
__declspec(dllexport) BOOL isUnicode() { return TRUE; }
__declspec(dllexport) const wchar_t* getName() { return L"NppFoldTail"; }
__declspec(dllexport) void setInfo(NppData data) { npp=data; }
__declspec(dllexport) FuncItem* getFuncsArray(int* n) { *n=2; return commands; }
// Only the common notification header is read; full SCNotification is not needed.
__declspec(dllexport) void beNotified(NMHDR* notification) {
    if(!notification) return;
    if(notification->code==1001) start(); // NPPN_READY
    if(notification->code==1009) { // NPPN_SHUTDOWN
        enabled=false;
        for(auto& v:views) if(IsWindow(v.window)) {
            clearVisuals(v); if(v.tooltip) DestroyWindow(v.tooltip);v.tooltip=nullptr;
            KillTimer(v.window,v.timer);
            RemoveWindowSubclass(v.window,subclass,0x46544149);
        }
    }
}
__declspec(dllexport) LRESULT messageProc(UINT,WPARAM,LPARAM) { return TRUE; }
}
