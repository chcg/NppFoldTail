#pragma once
// Included after the plugin's View, label() and send() declarations.
// This layer paints only: it never changes document text or syntax styles.
static std::wstring wide(const std::string& text, UINT cp) {
    if(text.empty()) return {};
    int n=MultiByteToWideChar(cp,0,text.data(),static_cast<int>(text.size()),nullptr,0);
    if(n<=0) return {};
    std::wstring result(n,L'\0');
    MultiByteToWideChar(cp,0,text.data(),static_cast<int>(text.size()),result.data(),n);
    return result;
}
static TOOLINFOW tool(View& v,LRESULT line) {
    TOOLINFOW ti{};ti.cbSize=TTTOOLINFOW_V2_SIZE;ti.hwnd=v.window;
    ti.uId=static_cast<UINT_PTR>(line+1);ti.uFlags=TTF_SUBCLASS;
    return ti;
}
static void createTooltip(View& v) {
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);
    v.tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,
        WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,CW_USEDEFAULT,CW_USEDEFAULT,
        CW_USEDEFAULT,CW_USEDEFAULT,v.window,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(v.tooltip) {
        send(v.tooltip,TTM_SETMAXTIPWIDTH,0,1000);
        send(v.tooltip,TTM_SETDELAYTIME,TTDT_INITIAL,500);
        send(v.tooltip,TTM_SETDELAYTIME,TTDT_AUTOPOP,20000);
    }
}
static void clearVisuals(View& v) {
    if(v.tooltip) {
        send(v.tooltip,TTM_POP);
        for(const auto& entry:v.visuals) {
            auto ti=tool(v,entry.first);send(v.tooltip,TTM_DELTOOLW,0,reinterpret_cast<LPARAM>(&ti));
        }
    }
    v.visuals.clear();v.pressed=-1;
}
static HFONT visualFont(HWND w,int style,HDC dc) {
    char face[256]{};
    send(w,2486,style,reinterpret_cast<LPARAM>(face)); // SCI_STYLEGETFONT
    auto name=wide(face,CP_UTF8);
    auto size=send(w,2062,style); // SCI_STYLEGETSIZEFRACTIONAL (hundredths of a point)
    if(size<=0) size=1000;
    size+=100*send(w,2374); // SCI_GETZOOM
    auto height=std::max(1,MulDiv(static_cast<int>(size),GetDeviceCaps(dc,LOGPIXELSY),7200));
    return CreateFontW(-height,0,0,0,send(w,2483,style)?FW_BOLD:FW_NORMAL,
        static_cast<DWORD>(send(w,2484,style)),FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,
        name.empty()?L"Consolas":name.c_str());
}
static bool sameRect(const RECT& a,const RECT& b) {return EqualRect(&a,&b)!=FALSE;}
static LRESULT hit(const View& v,int x,int y) {
    POINT p{x,y};
    for(const auto& entry:v.visuals) if(PtInRect(&entry.second.dots,p)) return entry.first;
    return -1;
}
static LRESULT headerCut(HWND w,LRESULT line,LRESULT last,const std::string& text,int lexer) {
    const auto pos=send(w,sci::PositionFromLine,line);
    auto cut=send(w,2136,line); // SCI_GETLINEENDPOSITION
    for(size_t j=0;j<text.size();++j) {
        if(text[j]!='{' || send(w,sci::GetStyleAt,pos+j)!=(lexer==38?5:10)) continue;
        const auto match=send(w,2353,pos+j,0); // SCI_BRACEMATCH
        const auto closeLine=match>=0 ? send(w,2166,match) : -1;
        if(closeLine>line && closeLine<=last) {cut=pos+j+1;break;}
    }
    return cut;
}
static LRESULT commentCut(HWND w,LRESULT header,LRESULT last,const std::string& text,int lexer) {
    const auto start=text.find_first_not_of(" \t");
    if(start==std::string::npos || text.compare(start,2,"/*")!=0 || text.find("*/",start+2)!=std::string::npos) return -1;
    const auto pos=send(w,sci::PositionFromLine,header);
    if(!foldtail::blockComment(lexer,static_cast<int>(send(w,sci::GetStyleAt,pos+start)))) return -1;
    for(auto line=last;line>header && line>=last-8;--line) {
        if(send(w,sci::GetLineVisible,line)) continue;
        const auto endText=readLine(w,line);
        const auto end=endText.rfind("*/");
        if(end==std::string::npos) continue;
        const auto endPos=send(w,sci::PositionFromLine,line)+end;
        if(foldtail::blockComment(lexer,static_cast<int>(send(w,sci::GetStyleAt,endPos)))) return pos+start+2;
    }
    return -1;
}
static std::wstring blockTooltip(HWND w,LRESULT header,LRESULT last,UINT cp) {
    std::vector<std::wstring> lines;
    for(auto line=header;line<=last && line<header+10;++line) lines.push_back(wide(readLine(w,line),cp));
    return foldtail::preview(lines,last>=header+10);
}
struct HtmlInfo {LRESULT cut=-1;std::string suffix;};
static HtmlInfo htmlInfo(HWND w,LRESULT header,LRESULT last,const std::string& text) {
    const auto pos=send(w,sci::PositionFromLine,header);
    const auto comment=text.find_first_not_of(" \t");
    if(comment!=std::string::npos && text.compare(comment,4,"<!--")==0 &&
        send(w,sci::GetStyleAt,pos+comment)==9) { // SCE_H_COMMENT
        if(text.find("-->",comment+4)!=std::string::npos) return {};
        for(auto line=last;line>header && line>=last-8;--line) {
            if(send(w,sci::GetLineVisible,line)) continue;
            const auto endText=readLine(w,line);
            const auto end=endText.find("-->");
            if(end==std::string::npos) continue;
            const auto endPos=send(w,sci::PositionFromLine,line)+end;
            if(send(w,sci::GetStyleAt,endPos)==9)
                return {pos+static_cast<LRESULT>(comment)+4,"-->"};
        }
        return {};
    }
    const auto opening=foldtail::htmlStart(text,[&](size_t i){return send(w,sci::GetStyleAt,pos+i);});
    if(opening.end==std::string::npos) return {};
    for(auto line=last;line>header && line>=last-8;--line) {
        if(send(w,sci::GetLineVisible,line)) continue;
        const auto lastText=readLine(w,line);
        const auto lastPos=send(w,sci::PositionFromLine,line);
        auto suffix=foldtail::htmlTail(lastText,opening.name,[&](size_t i){return send(w,sci::GetStyleAt,lastPos+i);});
        if(!suffix.empty()) return {pos+static_cast<LRESULT>(opening.end),std::move(suffix)};
    }
    return {};
}
static void update(View& v,bool includeHidden=false) {
    if(!enabled || v.busy) return;
    suppressLines(v);
    if(!includeHidden && !IsWindowVisible(v.window)) return;
    v.busy=true;
    auto w=v.window;
    const auto doc=send(w,sci::GetDocPointer);
    if(doc!=v.doc) {clearVisuals(v);v.doc=doc;}
    RECT client{};GetClientRect(w,&client);
    HDC dc=GetDC(w);
    if(!dc) {v.busy=false;return;}
    std::map<LRESULT,Visual> next;
    const auto first=send(w,sci::GetFirstVisibleLine);
    const auto count=std::min<LRESULT>(send(w,sci::LinesOnScreen)+2,2000);
    const auto total=send(w,sci::GetLineCount);
    const auto lexer=static_cast<int>(send(w,sci::GetLexer));
    const auto cp=static_cast<UINT>(send(w,2137)); // SCI_GETCODEPAGE
    if(foldtail::supported(lexer)) for(LRESULT i=0;i<count;++i) {
        const auto line=send(w,sci::DocLineFromVisible,first+i);
        if(line<0 || line>=total) break;
        if(next.count(line) || !(send(w,sci::GetFoldLevel,line)&0x2000) || send(w,sci::GetFoldExpanded,line)) continue;
        // A paint overlay cannot remove the extra display rows of a wrapped header.
        if(send(w,2235,line)>1) continue; // SCI_WRAPCOUNT
        const auto firstText=readLine(w,line);
        const auto pos=send(w,sci::PositionFromLine,line);
        const auto last=send(w,sci::GetLastChild,line,-1);
        const bool isHtml=lexer==4;
        const auto html=isHtml?htmlInfo(w,line,last,firstText):HtmlInfo{};
        if(isHtml && html.cut<0) continue;
        const auto commentPos=isHtml?-1:commentCut(w,line,last,firstText,lexer);
        const auto cut=isHtml?html.cut:commentPos>=0?commentPos:headerCut(w,line,last,firstText,lexer);
        const auto tail=commentPos>=0 || isHtml?TailInfo{}:label(w,line);
        const auto suffix=isHtml?html.suffix:commentPos>=0?std::string("*/"):tail.suffix;
        if(suffix.empty()) continue;
        Visual visual{};visual.doc=doc;
        visual.x=static_cast<int>(send(w,2164,0,cut));
        visual.y=static_cast<int>(send(w,2165,0,cut));
        visual.height=static_cast<int>(send(w,2279,line)); // SCI_TEXTHEIGHT
        visual.style=static_cast<int>(send(w,sci::GetStyleAt,cut>pos?cut-1:pos));
        visual.text=L" \u2026 "+wide(suffix,cp);
        if(commentPos>=0 || isHtml) visual.tip=blockTooltip(w,line,last,cp);
        else visual.tip=wide(foldtail::withoutEol(firstText),cp)+L"\r\n    \u22EE\r\n"+
            wide(foldtail::withoutEol(tail.raw),cp);
        visual.mask={visual.x,visual.y,client.right,visual.y+visual.height};
        // Never paint over the margin when a header is horizontally out of view.
        const auto textStart=send(w,2164,0,pos)+send(w,2398); // SCI_GETXOFFSET
        if(visual.x<textStart || visual.x<0 || visual.x>=client.right || visual.y<0 || visual.y>=client.bottom) continue;
        HFONT font=visualFont(w,visual.style,dc);
        HGDIOBJ old=font?SelectObject(dc,font):nullptr;
        SIZE space{},dots{};
        GetTextExtentPoint32W(dc,L" ",1,&space);GetTextExtentPoint32W(dc,L"\u2026",1,&dots);
        visual.dots={visual.x+space.cx,visual.y,visual.x+space.cx+dots.cx,visual.y+visual.height};
        if(old) SelectObject(dc,old);
        if(font) DeleteObject(font);
        next.emplace(line,std::move(visual));
    }
    ReleaseDC(w,dc);
    bool changed=false;
    for(auto it=v.visuals.begin();it!=v.visuals.end();) {
        if(!next.count(it->first)) {
            auto ti=tool(v,it->first);send(v.tooltip,TTM_DELTOOLW,0,reinterpret_cast<LPARAM>(&ti));
            it=v.visuals.erase(it);changed=true;
        } else ++it;
    }
    for(auto& entry:next) {
        auto old=v.visuals.find(entry.first);
        const bool added=old==v.visuals.end();
        if(!added && old->second.text==entry.second.text && old->second.tip==entry.second.tip &&
            old->second.style==entry.second.style && sameRect(old->second.mask,entry.second.mask) &&
            sameRect(old->second.dots,entry.second.dots)) continue;
        if(!added) {auto ti=tool(v,entry.first);send(v.tooltip,TTM_DELTOOLW,0,reinterpret_cast<LPARAM>(&ti));}
        auto& saved=v.visuals[entry.first];saved=std::move(entry.second);
        auto ti=tool(v,entry.first);ti.rect=saved.dots;ti.lpszText=saved.tip.data();
        send(v.tooltip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&ti));changed=true;
    }
    if(changed) InvalidateRect(w,nullptr,FALSE);
    v.busy=false;
}
static void paintVisuals(View& v) {
    if(!enabled || v.visuals.empty() || send(v.window,sci::GetDocPointer)!=v.doc) return;
    HDC dc=GetDC(v.window);if(!dc) return;
    for(const auto& entry:v.visuals) {
        const auto& visual=entry.second;
        if(send(v.window,sci::GetFoldExpanded,entry.first)) continue;
        HFONT font=visualFont(v.window,visual.style,dc);
        HGDIOBJ old=font?SelectObject(dc,font):nullptr;
        SetTextColor(dc,static_cast<COLORREF>(send(v.window,2481,visual.style)));
        SetBkColor(dc,static_cast<COLORREF>(send(v.window,2482,visual.style)));
        TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);
        auto y=visual.y+std::max<LONG>(0,(visual.height-metrics.tmHeight)/2);
        ExtTextOutW(dc,visual.x,y,ETO_OPAQUE|ETO_CLIPPED,&visual.mask,visual.text.data(),static_cast<UINT>(visual.text.size()),nullptr);
        if(old) SelectObject(dc,old);
        if(font) DeleteObject(font);
    }
    ReleaseDC(v.window,dc);
}
