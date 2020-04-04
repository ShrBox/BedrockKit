#pragma once
#include<lbpch.h>
#include<variant>
#include<api/types/types.h>
#include<I18N.h>
#include<debug/WatchDog.h>
using std::string, std::function,std::variant;
enum class EvPrio:int
{
	LOW = 0,
	MEDUIM = 1,
	HIGH = 2
};
LIGHTBASE_API int newListenerID();
template<typename T>
struct LInfo {
	int id=-1;
};
class ICancellableEvent {
	bool cancelled;
public:
	bool isCancelled() {
		return cancelled;
	}
	operator bool() {
		return !isCancelled();
	}
	void setCancelled(bool c = true) {
		cancelled = c;
	}
	ICancellableEvent() :cancelled(false) {}
};
template<class T>
struct CallBackStorage {
	function<void(T&)> data;
	LInfo<T> id;
#ifdef TRACING_ENABLED
	string note;
#endif
	operator bool() {
		return id.id!=-1;
	}
	void operator()(T& arg) {
		data(arg);
	}
	CallBackStorage() {
		id.id = -1;
	}
#ifdef TRACING_ENABLED
	CallBackStorage(function<void(T&)>&& fun, LInfo<T> lf,string&& note_) :data(std::forward< function<void(T&)>>(fun)), id(lf),note(std::forward<string>(note_)) {
	}
#else
	CallBackStorage(function<void(T&)>&& fun,LInfo<T> lf):data(std::forward< function<void(T&)>>(fun)),id(lf) {
	}
#endif
};
#ifdef LIGHTBASE_EXPORTS
static inline void logError(const char* e,const char* T) {
	char ebuf[1024];
	snprintf(ebuf, 1024, I18N::EVENT_EXCEPTION_S.c_str(), e, T);
	LOG.l<LOGLVL::Error>(ebuf);
}
#else
static inline void logError(const char* e, const char* T) {
	printf(I18N::EVENT_EXCEPTION_S.c_str(), e, T);
}
#endif
#include<debug/WatchDog.h>
template <class T>
class EventCaller {
	LIGHTBASE_API static std::list<CallBackStorage<T>> listener;
public:
	template<typename... P>
	static auto _call(P&&... args) {
		//printf("call event %s\n", typeid(T).name());
		
		T ev(std::forward<P>(args)...);
		try {
			for (auto& i : EventCaller<T>::listener) {
				if (i.id.id == -1)
					continue;
				WATCH_ME(string("call event ") + typeid(T).name() + "\n at " + i.note);
					i(ev);
				if (ev.isAborted()) break;
			}
		}
		catch (std::exception e) {
			logError(e.what(), typeid(T).name());
		}
		catch (string e) {
			logError(e.c_str(), typeid(T).name());
		}
		catch (...) {
			logError("unk error", typeid(T).name());
		}
		if constexpr (std::is_base_of<ICancellableEvent, T>())
			return !ev.isCancelled();
		else
			return;
	}
	static void _removeall() {
		listener.clear();
	}
	template<typename... TP>
	static LInfo<T> _reg(function<void(T&)>&& cb, EvPrio prio,TP&&... args) {
		LInfo<T> lf;
		lf.id = newListenerID();
		if (prio == EvPrio::HIGH) {
			listener.emplace_front(std::forward< function<void(T&)>>(cb),lf,std::forward<TP>(args)...);
			return lf;
		}
		if (prio == EvPrio::LOW) {
			listener.emplace_back(std::forward< function<void(T&)>>(cb), lf, std::forward<TP>(args)...);
			return lf;
		}
		for (auto it = listener.begin(); it != listener.end(); ++it) {
			if (!(*it)) {
				//flag
				listener.emplace(it, std::forward<function<void(T&)>>(cb), lf, std::forward<TP>(args)...);
				return lf;
			}
		}
		printf("[Event] Failed to register %s ,Dont use PRIO_MEDIUM when server started!!!\n",typeid(T).name());
		return { -1 };
	}
	static auto _remove(LInfo<T> lf) {
		return listener.remove_if([lf](auto& elem)->bool {return elem.id.id == lf.id; });
	}
	static void _cleanup() {
		listener.remove_if([](auto& elem)->bool {return !elem; });
	}
};

class IAbortableEvent {
	bool aborted;
public:
	void setAborted(bool c = true) {
		aborted = c;
	}
	bool isAborted() {
		return aborted;
	}
	IAbortableEvent() :aborted(false) {}
};
class ServerPlayer;
class IPlayerEvent {
	WPlayer sp;
public:
	WPlayer getPlayer() {
		return sp;
	}
	IPlayerEvent(ServerPlayer& s) :sp(s) {}
};
class IActorEvent {
	WActor sp;

public:
	WActor getActor() {
		return sp;
	}
	IActorEvent(Actor& s) : sp(s) {}
};
class IMobEvent {
	WMob sp;

public:
	WMob getMob() {
		return sp;
	}
	IMobEvent(Mob& s) : sp(s) {}
};
template<typename T>
class IEventBase :public EventCaller<T>, public IAbortableEvent {

};
template<class T>
class IGenericEvent :public IEventBase<T>, public ICancellableEvent {};
template<class T>
class IGenericPlayerEvent :public IGenericEvent<T>, public IPlayerEvent {
public:
	IGenericPlayerEvent(ServerPlayer& sp) :IPlayerEvent(sp) {}
};

template<class T>
class IGenericActorEvent : public IGenericEvent<T>, public IActorEvent {
public:
	IGenericActorEvent(Actor& sp) : IActorEvent(sp) {}
};

template<class T>
class INotifyEvent :public IEventBase<T> {};
template<class T>
class INotifyPlayerEvent :public INotifyEvent<T>, public IPlayerEvent {
public:
	INotifyPlayerEvent(ServerPlayer& sp) :IPlayerEvent(sp) {}
};



template<class T>
void removeListener(LInfo<T> lf) {
	T::_remove(lf);
}
#ifdef TRACING_ENABLED
template<class T>
LInfo<T> __addListener(string&& note,function<void(T&)>&& fn, EvPrio prio = EvPrio::MEDUIM) {
	return T::_reg(std::forward<function<void(T&)>>(fn), prio,std::forward<string>(note));
}
template<typename T>
auto __addListener(string&& note,T&& fn, EvPrio prio = EvPrio::MEDUIM) {
	return __addListener(std::forward<string>(note),function(std::forward<T>(fn)), prio);
}
struct addListener_caller {
	string note;
	addListener_caller(string&& n) {
		note = std::forward<string>(n);
	}
	template<typename... T>
	auto operator()(T&&... a) {
		return __addListener(std::move(note), std::forward<T>(a)...);
	}
};
#define addListener addListener_caller(std::to_string(__LINE__)+" :: "+__FILE__)
#else
template<class T>
LInfo<T> addListener(function<void(T&)>&& fn, EvPrio prio = EvPrio::MEDUIM) {
	return T::_reg(std::forward<function<void(T&)>>(fn), prio);
}
template<typename T>
LInfo<T> addListener(T&& fn, EvPrio prio = EvPrio::MEDUIM) {
	return addListener(function(std::forward<T>(fn)), prio);
}
#endif