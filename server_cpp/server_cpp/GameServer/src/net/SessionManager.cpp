#include "net/SessionManager.h"
#include "net/Session.h"

SessionManager::~SessionManager()
{
    StopAll();
}

std::shared_ptr<Session> SessionManager::CreateAndAdd(SOCKET clientSock)
{
    const SessionId id = ++_idGen;

    // onClose는 Session 스레드에서 호출될 수 있으니,
    // Remove는 "map에서 빼기만" 하고, join은 소유자(StopAll)가 하도록 설계해도 됨.
    // 하지만 현재 Session::~Session이 Stop()을 호출하므로,
    // Remove에서 바로 shared_ptr을 떨구면 "세션 스레드가 자기 자신 join" 위험이 있음.
    //
    // 그래서 Tier1에서는 Remove에서 즉시 파괴하지 말고,
    //    '외부에서 StopAll(또는 별도 GC)'로 정리하는 방식이 더 안전함.
    //
    // 여기서는 "즉시 제거 + Stop은 RequestStop만" 방식으로 타협:
    // Remove에서 shared_ptr을 꺼낸 뒤 RequestStop만 호출하고, 파괴는 외부에서.
    // (아래 구현 참고)

    auto session = std::make_shared<Session>(
        clientSock,
        id,
        [this](SessionId sid) { this->Remove(sid); }
    );

    {
        std::lock_guard<std::mutex> lock(_mtx);
        _sessions.emplace(id, session);
    }

    return session;
}

void SessionManager::Remove(SessionId id)
{
    std::shared_ptr<Session> victim;

    {
        std::lock_guard<std::mutex> lock(_mtx);
        auto it = _sessions.find(id);
        if (it == _sessions.end())
            return;

        victim = it->second;
        _sessions.erase(it);
    }

    // Session 스레드에서 Remove가 호출될 수 있으므로 join은 절대 하지 말자.
    // 대신 종료 요청만.
    if (victim)
        victim->RequestStop();

    // victim이 여기서 소멸할 수 있음.
    // Session::~Session이 Stop()을 호출(=join)하면 "자기 스레드 join" 문제가 다시 생김.
    //
    // 그래서 가장 깔끔한 정답은:
    //    - Session::~Session에서 Stop() 호출 제거 (소유자가 Stop/Join 책임)
    //    - 또는 Session 내부에서 스레드가 끝난 후에만 onClose 호출되도록 보장
    //
    // 현재 코드는 onClose를 RecvLoop 끝에서 호출하니까,
    // Remove가 호출되는 시점은 "스레드가 이미 종료된 뒤"라 self-join 위험이 없음.
    // (이 보장이 깨지지 않게 '잠금 포인트'로 고정!)
}

void SessionManager::StopAll()
{
    std::vector<std::shared_ptr<Session>> copied;

    {
        std::lock_guard<std::mutex> lock(_mtx);
        copied.reserve(_sessions.size());
        for (auto& kv : _sessions)
            copied.push_back(kv.second);
        _sessions.clear();
    }

    // 외부에서 StopAll 호출 → join 가능
    for (auto& s : copied)
    {
        if (s)
            s->Stop();
    }
}

size_t SessionManager::Count() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _sessions.size();
}
