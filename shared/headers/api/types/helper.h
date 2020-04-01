#pragma once
struct WPlayer;
#include<string>
#include<stl\useful.h>
namespace BDX {
	using std::string;
	LBAPI bool runcmd(const string& cmd);
	LBAPI bool runcmdAs(WPlayer, const string& cmd);
	LBAPI string getIP(class NetworkIdentifier&);
	template<typename T>
	static inline void APPEND(string& r,T&& x) {
		r.append(S(std::forward<T>(x)));
		r.push_back(' ');
	}
	template<typename... T>
	static inline bool runcmdA(T&&... a) {
		string s;
		(APPEND(s,std::forward<T>(a)), ...);
		return runcmd(s);
	}
	template<typename N,typename... T>
	static inline bool runcmdAsA(N p,T&&... a) {
		string s;
		(APPEND(s, std::forward<T>(a)), ...);
		return runcmdAs(p, s);
	}
};