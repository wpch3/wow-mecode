#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
typedef uint8_t uint8; typedef uint16_t uint16; typedef uint32_t uint32; typedef uint64_t uint64;

extern std::vector<std::string> g_log;
extern std::map<std::string,std::string> g_sql;   // 记录执行过的SQL

// ---- 名字校验相关（照抄真实实现）----
enum ResponseCodes {
    CHAR_NAME_SUCCESS=87, CHAR_NAME_FAILURE=88, CHAR_NAME_NO_NAME=89,
    CHAR_NAME_TOO_SHORT=90, CHAR_NAME_TOO_LONG=91, CHAR_NAME_INVALID_CHARACTER=92,
    CHAR_NAME_MIXED_LANGUAGES=93, CHAR_NAME_PROFANE=94, CHAR_NAME_RESERVED=95,
    CHAR_NAME_THREE_CONSECUTIVE=98
};
enum LocaleConstant { LOCALE_enUS=0, LOCALE_zhCN=4, DEFAULT_LOCALE=LOCALE_enUS };
#define MAX_PLAYER_NAME 12
#define TYPEID_UNIT 3
#define TYPEID_PLAYER 4

// Util.h:155 isEastAsianCharacter 的真实实现
inline bool isEastAsianCharacter(wchar_t c){
    if (c>=0x1100&&c<=0x11F9) return true;
    if (c>=0x3041&&c<=0x30FF) return true;
    if (c>=0x3131&&c<=0x318E) return true;
    if (c>=0x31F0&&c<=0x31FF) return true;
    if (c>=0x3400&&c<=0x4DB5) return true;
    if (c>=0x4E00&&c<=0x9FC3) return true;   // 汉字
    if (c>=0xAC00&&c<=0xD7A3) return true;
    if (c>=0xFF01&&c<=0xFFEE) return true;
    return false;
}
inline bool isBasicLatinCharacter(wchar_t c){
    return (c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z');
}
inline bool isEastAsianString(std::wstring const& s){
    for(wchar_t c:s) if(!isEastAsianCharacter(c)) return false; return !s.empty(); }
inline bool isBasicLatinString(std::wstring const& s){
    for(wchar_t c:s) if(!isBasicLatinCharacter(c)) return false; return !s.empty(); }

// UTF8 -> wstring（够用即可）
inline bool Utf8toWStr(std::string const& in, std::wstring& out){
    out.clear(); size_t i=0;
    while(i<in.size()){
        unsigned char c=in[i];
        if(c<0x80){ out.push_back(c); i+=1; }
        else if((c>>5)==0x6){ if(i+1>=in.size())return false;
            out.push_back(((c&0x1F)<<6)|(in[i+1]&0x3F)); i+=2; }
        else if((c>>4)==0xE){ if(i+2>=in.size())return false;
            out.push_back(((c&0x0F)<<12)|((in[i+1]&0x3F)<<6)|(in[i+2]&0x3F)); i+=3; }
        else return false;
    }
    return true;
}
