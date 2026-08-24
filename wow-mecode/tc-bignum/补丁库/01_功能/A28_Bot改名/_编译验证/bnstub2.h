#pragma once
#include "bnstub.h"

// ObjectMgr.cpp:8633 isValidString 的真实逻辑（strictMask=0 分支）
inline bool isValidString(std::wstring const& w, uint32 strictMask){
    if(strictMask==0){
        if(isBasicLatinString(w)) return true;
        if(isEastAsianString(w)) return true;
        return false;                       // 混用 -> false
    }
    if(strictMask&0x1) return isBasicLatinString(w);
    return false;
}
extern uint32 g_strictMask;

struct ObjectMgrStub {
    std::vector<std::string> reserved;
    // ObjectMgr.cpp:8669 的真实实现
    static ResponseCodes CheckPlayerName(std::string const& name, LocaleConstant, bool create=false){
        std::wstring w;
        if(!Utf8toWStr(name,w)) return CHAR_NAME_INVALID_CHARACTER;
        if(w.size()>MAX_PLAYER_NAME) return CHAR_NAME_TOO_LONG;
        if(w.size()<2) return CHAR_NAME_TOO_SHORT;
        if(!isValidString(w,g_strictMask)) return CHAR_NAME_MIXED_LANGUAGES;
        for(size_t i=2;i<w.size();++i)
            if(w[i]==w[i-1]&&w[i]==w[i-2]) return CHAR_NAME_THREE_CONSECUTIVE;
        (void)create;
        return CHAR_NAME_SUCCESS;
    }
    bool IsReservedName(std::string const& n) const {
        for(auto&r:reserved) if(r==n) return true; return false; }
};
extern ObjectMgrStub _omgr;
#define sObjectMgr (&_omgr)

// normalizePlayerName：首字母大写其余小写（对中文无影响）
inline bool normalizePlayerName(std::string& name){
    if(name.empty()) return false;
    std::wstring w; if(!Utf8toWStr(name,w)) return false;
    if(w.empty()) return false;
    // 只对纯ASCII做大小写规范
    bool ascii=true; for(char c:name) if((unsigned char)c>=0x80) ascii=false;
    if(ascii){
        for(size_t i=0;i<name.size();++i)
            name[i]= (i==0)? toupper(name[i]) : tolower(name[i]);
    }
    return true;
}
