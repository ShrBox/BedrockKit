#include"pch.h"
#include"bdxlua.h"
namespace LEX {
    struct Dstring_view {
        char* data;
        size_t size;
        string_view move() {
            string_view rv = { data,size };
            data = nullptr;
            size = 0;
            return rv;
        }
        void push(const char* d) {
            if (data == nullptr) data = (char*)d;
            ++size;
        }
        Dstring_view() {
            data = nullptr;
            size = 0;
        }
    };
    bool _trim(string_view& stv, char tc) {
        auto start = stv.find_first_not_of(tc);
        auto end = stv.find_last_not_of(tc);
        if (start == stv.npos || end == stv.npos) return false;
        stv = stv.substr(start, end - start + 1);
        return true;
    }
    bool trim(string_view& stv) {
        return _trim(stv, ' ') && _trim(stv, '\r');
    }
    void peelVal(string_view v, string_view& rv) {
        if (v[0] == '"') {
            v.remove_prefix(1);
            v.remove_suffix(1);
        }
        rv = v;
    }
    void peelVal(string_view v, vector<string_view>& rv) {
        v.remove_suffix(1);
        v.remove_prefix(1);
        Dstring_view ds;
        bool instr = false;
        for (auto& i : v) {
            if (i == '"') {
                instr = !instr;
                continue;
            }
            if (!instr) {
                if (i == ',')
                    rv.emplace_back(ds.move());
            }
            else {
                ds.push(&i);
            }
        }
        if (ds.size)
            rv.emplace_back(ds.move());
    }
    std::pair<string_view, string_view> parseEntry(string_view rv) {
        auto eq = rv.find('=');
        if (eq == rv.npos) throw "no '=' found in ["s.append(rv)+"]";
        string_view key = rv.substr(0, eq);
        string_view val = rv.substr(eq + 1);
        if (!trim(key) || !trim(val)) throw "no key/val found in ["s.append(rv) + "]";
        return { key,val };
    }
    constexpr size_t H(string_view x) {
        return do_hash2(x);
    }
    void parse2(vector<string_view>& rval, unordered_map<size_t, string_view>& val, vector<string_view>& args) {
        for (auto i : rval) {
            auto [k, v] = parseEntry(i);
            auto KH = H(k);
            if (KH == H("args")) {
                peelVal(v, args);
            }
            else {
                string_view x;
                peelVal(v, x);
                val.emplace(KH, x);
            }
        }
    }
    void parse(const string& line, vector<string_view>& rval) {
        bool in_quote = false;
        bool in_br = false;
        Dstring_view ds;
        for (auto& i : line) {
            if (i == '"') {
                in_quote = !in_quote;
            }
            if (!in_quote && (i == '[' || i == ']')) {
                in_br = !in_br;
            }
            if (!in_quote && !in_br) {
                if (i == ',') rval.emplace_back(ds.move()); else ds.push(&i);
                continue;
            }
            ds.push(&i);
        }
        if (!in_quote && !in_br) rval.emplace_back(ds.move());
    }
    string Compile(string_view source, vector<string> const& val) {
        auto sz = source.size();
        string rv;
        rv.reserve(source.size() + 256);
        for (int i = 0; i < sz; ++i) {
            if (source[i] == '%') {
                if (source[i + 1] == '%') {
                    rv.push_back('%');
                    i = i + 1;
                    continue;
                }
                int j;
                int CV = 0;
                for (j = i + 1; j < sz; ++j) {
                    if (!isdigit(source[j])) break;
                    CV = CV * 10 + source[j] - '0';
                }
                i = j - 1;
                if (CV == 0) {
                    rv.append(GUI::getPlayerListView());
                }
                else {
                    CV--;
                    if (CV >= val.size()) {
                        throw "Compile IDX Overflow "s+S(CV)+" vs. "+S(val.size());
                    }
                    rv.append(val[CV]);
                }
            }
            else {
                rv.push_back(source[i]);
            }
        }
        rv.shrink_to_fit();
        return rv;
    }
    void parseEverything(const string& src, unordered_map<size_t, string_view>& mp, vector<string_view>& args) {
        vector<string_view> x;
        parse(src, x);
        parse2(x, mp, args);
    }
};
string ProcessLabel(string_view x) {
    string rv;
    rv.reserve(x.size());
    for (int i = 0; i < x.size(); ++i) {
        if (x[i] == '\\') {
            switch (x[i + 1]) {
            case 'n':
                rv.push_back('\n');
                i+=1;
            break;
#if 0
            case 'u':
                //i+2 i+3 i+4 i+5
            {
                uintptr_t X = 0;
                for (int j = i + 2; j <= i + 5; ++j) {
                    if (isdigit(x[j])) {
                        X *= 16;
                        X += x[j] - '0';
                    }
                    else {
                        X *= 16;
                        X += tolower(x[j]) - 'a' + 10;
                    }
                }
                //UTF16LE->UTF16BE
                printf("%p\n", X);
                rv.push_back(*(((char*)&X))); rv.push_back(*(((char*)&X)+1));
                i += 5;
            }
            break;
#endif
            default:
                rv.push_back('\\');
            }
        }
        else {
            rv.push_back(x[i]);
        }
    }
    rv.shrink_to_fit();
    return rv;
}
#include<filesystem>
string getForm(const string& name) {
    auto path = "gui/" + name;
    if (!std::filesystem::exists(path)) throw "File not exists"s;
    std::ifstream ifs(path);
    return ifs2str(ifs);
}
using LEX::H;

void LUA_NOTIFY_CB2(const string& name, const string& pname) {
    if (name.size() == 0) return;
    lua_getglobal(L, "EXCEPTION");
    auto EHIDX = lua_gettop(L);
    if (lua_getglobal(L, name.c_str()) == 0) {
        printf("[LUA] guicb2 %s not found\n", name.c_str());
        return;
    }
    lua_pushlstring(L, pname.data(), pname.size());
    LuaFly(L).pCall(name.c_str(), 1, 0, EHIDX);
}
void LUA_NOTIFY_SIMPLE(const string& name, const string& pname, int idx, const string& text) {
    lua_getglobal(L, "EXCEPTION");
    auto EHIDX = lua_gettop(L);
    if (lua_getglobal(L, name.c_str()) == 0) {
        printf("[LUA] guicb %s not found\n", name.c_str());
        return;
    }
    lua_pushlstring(L, pname.data(), pname.size());
    lua_pushinteger(L, idx);
    lua_pushlstring(L, text.data(), text.size());
    LuaFly(L).pCall(name.c_str(), 3, 0, EHIDX);
}
void sendSimp(WPlayer w, std::stringstream& ss, string cb,string cb2,string && tit,string && cont) {
    using namespace GUI;
    shared_ptr<SimpleForm> sf= make_shared<SimpleForm>();
    sf->title = std::forward<string>(tit);
    sf->content = std::forward<string>(cont);
    string line;
    vector<string_view> args;
    while (std::getline(ss, line)) {
        unordered_map<size_t, string_view> mp;
        LEX::parseEverything(line,mp,args);//dont need args
        auto it = mp.find(H("img"));
        if (it != mp.end())
            sf->buttons.emplace_back(ProcessLabel(mp[H("text")]), string{ it->second });
        else
            sf->buttons.emplace_back(ProcessLabel(mp[H("text")]));
    }
    sendForm(w, SimpleFormBinder{ sf,[cb,cb2](WPlayer w, SimpleFormBinder::DType dt) {
        if (dt.set) {
            auto [idx, val] = dt.val();
            //call idx,val(s)
            LUA_NOTIFY_SIMPLE(cb, w.getName(), idx, val);
        }
        else {
            //call cb2
            LUA_NOTIFY_CB2(cb2,w.getName());
        }
      } });
}
void sendFull(WPlayer w, std::stringstream& ss, string cb, string cb2, string&& tit) {
    using namespace GUI;
    shared_ptr<FullForm> sf = make_shared<FullForm>();
    sf->title = std::forward<string>(tit);
    string line;
    while (std::getline(ss, line)) {
        if (line.size() <= 3) continue;
        unordered_map<size_t, string_view> mp;
        vector<string_view> args;
        try {
            LEX::parseEverything(line, mp, args);
        }
        catch (string e) {
            throw "error " + e + " in line [" + line + "]";
        }
        auto it = mp.find(H("type"));
        if (it == mp.end()) throw "must specify a type at ["s + line;
        //GUIDropdown; GUIInput; GUILabel; GUISlider;GUIToggle
        switch (H(it->second)) {
        case H("dropdown"): {
            vector<string> _args;
            for (auto X : args) {
                _args.emplace_back(X);
            }
            sf->widgets.emplace_back(GUIDropdown(string{ mp[H("text")] }, std::move(_args)));
            break;
        }
        case H("input"): {
            sf->widgets.emplace_back(GUIInput(string{ mp[H("text")] }, string{ mp[H("placeholder")] }));
            break;
        }
        case H("label"): {
            sf->widgets.emplace_back(GUILabel(ProcessLabel(mp[H("text")])));
            break;
        }
        case H("slider"): {
            int miN=atoi(mp[H("min")]), maX= atoi(mp[H("max")]), def= atoi(mp[H("def")]);
            sf->widgets.emplace_back(GUISlider(string{ mp[H("text")] },miN,maX,1,def));
            break;
        }
        case H("toggle"): {
            sf->widgets.emplace_back(GUIToggle(string{ mp[H("text")] }, atoi(mp[H("def")])));
            break;
        }
        default: {
            throw "invlaid type "s.append(it->second)+ "at [" + line;;
        }
        }
    }
    sendForm(w, FullFormBinder(sf, [cb, cb2](WPlayer w, FullFormBinder::DType d) {
        if (d.set) {
            auto [dat, ext] = d.val();
            //push name,array(dat),array_str(ext
            lua_getglobal(L, "EXCEPTION");
            auto EHIDX = lua_gettop(L);
            if (lua_getglobal(L, cb.c_str()) == 0) {
                printf("[LUA] guicb %s not found\n", cb.c_str());
                return;
            }
            lua_pushlstring(L, w.getName().data(), w.getName().size());
            {
                lua_checkstack(L, ext.size()+dat.size() + 20);
                lua_newtable(L);
                int idx = 0;
                for (auto i : dat) {
                    //lua_pushstring(L, i.getName().c_str());
                    if (i.index() == 0) {
                        //string
                        lua_pushlstring(L, std::get<string>(i).data(), std::get<string>(i).size());
                    }
                    else {
                        //int
                        lua_pushinteger(L, std::get<int>(i));
                    }
                    lua_rawseti(L, -2, ++idx);
                }
            }
            {
                lua_checkstack(L, ext.size()+dat.size() + 20);
                lua_newtable(L);
                int idx = 0;
                for (auto i : ext) {
                    //lua_pushstring(L, i.getName().c_str());
                    lua_pushlstring(L, (i).data(), (i).size());
                    lua_rawseti(L, -2, ++idx);
                }
            }
            LuaFly(L).pCall(cb.c_str(), 3, 0, EHIDX);
        }
        else {
            LUA_NOTIFY_CB2(cb2, w.getName());
        }
        }));
}
void glang_send(WPlayer wp,const string& payload) {
    std::stringstream ss{ payload };
    string line;
    std::getline(ss, line);
    unordered_map<size_t, string_view> mp;
    vector<string_view> args;
        LEX::parseEverything(line, mp, args);
    //step 3 sendForm
    auto& tp = mp[H("type")];
    auto luacb = mp[H("cb")];
    auto luacb2 = mp[H("cb2")];
    auto content = mp[H("content")];
    auto title = mp[H("title")];
    if (tp == "full") {
        sendFull(wp, ss, string{ luacb }, string{ luacb2 }, string{ title });
    }
    else {
        if (tp == "simple") {
            sendSimp(wp, ss, string{ luacb }, string{ luacb2 }, string{ title }, ProcessLabel(content));
        }
        else {
            throw "form type can only be (full,simple)"s;
            return;
        }
    }
}
int lua_bind_GUI(lua_State* L) {
    //string(pname) string(filename) args...
    auto n = lua_gettop(L);
    if (n < 2) {
        luaL_error(L, "too less args");
        return 0;
    }
    size_t Lpname, Lfile;
    auto ppname=luaL_tolstring(L, 1, &Lpname);
    auto pfile=luaL_tolstring(L, 2, &Lfile);
    auto p=LocateS<WLevel>()->getPlayer({ ppname,Lpname });
    if (!p.set) {
        luaL_error(L, "player offline");
        return 0;
    }
    auto wp=p.val();
    string sv;
    try {
        sv = getForm({ pfile,Lfile });
    }
    catch (...) {
        luaL_error(L, "cant find form");
        return 0;
    } //step 1 load form
    vector<string> X;
    try {
        LuaFly fly{ L };
        for (int i = 3; i <= n; ++i) {
            xstring x;
            fly.readx(x, i);
            X.emplace_back(std::move(x));
        } //step 2 lua->vector
    }
    catch (string e) {
        luaL_error(L, e.c_str());
        return 0;
    }
    lua_settop(L, 0);
    try {
         auto compiled=LEX::Compile(sv, X);
         glang_send(wp, compiled);
    }
    catch (string e) {
        luaL_error(L, e.c_str());
        return 0;
    }
    catch (...) {
        return 0;
    }
    return 0;
}