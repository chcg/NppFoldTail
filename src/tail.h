#pragma once
#include <string>
#include <cctype>
#include <vector>

namespace foldtail {
// Lexilla lexer IDs and comment style IDs (SciLexer.h).
inline bool supported(int lexer) {
    return lexer == 3 || lexer == 35 || lexer == 38 || lexer == 6 || lexer == 4;
}
inline bool comment(int lexer, int style) {
    if (lexer == 3 || lexer == 35)
        return style == 1 || style == 2 || style == 3 || style == 15 || style == 17 || style == 18;
    if (lexer == 38) return style == 9;
    if (lexer == 6) return style == 2 || style == 3;
    return false;
}
inline bool blockComment(int lexer,int style) {
    return ((lexer==3 || lexer==35) && (style==1 || style==3 || style==17 || style==18)) ||
        (lexer==38 && style==9);
}
struct HtmlStart { size_t end=std::string::npos; std::string name; };
inline bool htmlNameChar(unsigned char c) {return std::isalnum(c) || c=='-' || c==':' || c=='_';}
inline std::string lowerTag(std::string text) {
    for(auto& c:text) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
template<class StyleAt> HtmlStart htmlStart(const std::string& text,StyleAt styleAt) {
    const auto start=text.find_first_not_of(" \t");
    if(start==std::string::npos || start+1>=text.size() || text[start]!='<' ||
        !std::isalpha(static_cast<unsigned char>(text[start+1])) ||
        (styleAt(start)!=1 && styleAt(start)!=2)) return {};
    auto nameEnd=start+1;
    while(nameEnd<text.size() && htmlNameChar(static_cast<unsigned char>(text[nameEnd]))) ++nameEnd;
    char quote=0;
    for(auto i=nameEnd;i<text.size();++i) {
        const auto c=text[i];
        if(quote) {if(c==quote) quote=0;continue;}
        if(c=='\'' || c=='"') {quote=c;continue;}
        if(c=='>') {
            auto previous=i;
            while(previous>nameEnd && std::isspace(static_cast<unsigned char>(text[previous-1]))) --previous;
            if(previous>nameEnd && text[previous-1]=='/') return {};
            return {i+1,lowerTag(text.substr(start+1,nameEnd-start-1))};
        }
    }
    return {};
}
template<class StyleAt> std::string htmlTail(const std::string& text,const std::string& name,StyleAt styleAt) {
    for(size_t start=0;(start=text.find("</",start))!=std::string::npos;++start) {
        if(styleAt(start)!=1 && styleAt(start)!=2) continue;
        auto end=start+2;
        while(end<text.size() && htmlNameChar(static_cast<unsigned char>(text[end]))) ++end;
        if(lowerTag(text.substr(start+2,end-start-2))!=name) continue;
        auto close=end;
        while(close<text.size() && std::isspace(static_cast<unsigned char>(text[close]))) ++close;
        if(close>=text.size() || text[close]!='>') continue;
        const auto last=text.find_last_not_of(" \t\r\n");
        return text.substr(start,last-start+1);
    }
    return {};
}
template<class StyleAt>
std::string tail(const std::string& text, int lexer, StyleAt styleAt) {
    if (!supported(lexer)) return "";
    std::string out;
    size_t brace=std::string::npos;
    for (size_t i = 0; i < text.size(); ++i) {
        const auto style=styleAt(i);
        if (comment(lexer, style)) continue;
        // Only syntax operators count: braces inside strings are not block endings.
        const bool op=style==(lexer==38 ? 5 : 10);
        if (brace==std::string::npos && text[i]=='}' && op) brace=out.size();
        out += text[i];
    }
    const auto start = brace!=std::string::npos ? brace : out.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = out.find_last_not_of(" \t\r\n");
    out = out.substr(start, end - start + 1);
    // A closing brace keeps its entire non-comment suffix, including arguments.
    if (out.size() > 256) return "";
    if (brace==std::string::npos && out.find_first_not_of("] );,\t")!=std::string::npos) return "";
    if (out.find_first_of("}])") == std::string::npos) return "";
    return out;
}
inline std::string display(const std::string& suffix) {
    return suffix.empty() ? "" : "\xE2\x80\xA6 " + suffix;
}
inline std::string withoutEol(std::string text) {
    while(!text.empty() && (text.back()=='\r' || text.back()=='\n')) text.pop_back();
    return text;
}
inline std::wstring preview(const std::vector<std::wstring>& lines,bool more=false) {
    std::wstring result;
    const auto count=lines.size()>10?10:lines.size();
    for(size_t i=0;i<count;++i) {
        auto line=lines[i];
        while(!line.empty() && (line.back()==L'\r' || line.back()==L'\n')) line.pop_back();
        if(i) result+=L"\r\n";
        result+=line;
    }
    if(more || lines.size()>10) result+=L"\r\n    \u22EE";
    return result;
}
}
