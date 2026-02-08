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
    std::lock_guard<std::mutex> lock(_mtx);
    auto it = _sessions.find(id);
    if (it == _sessions.end())
        return;

    // map에서 빼고 zombies로 이동 (즉시 소멸 방지)
    _zombies.emplace_back(std::move(it->second));
    _sessions.erase(it);

    // 여기서 RequestStop/Stop/join 아무것도 하지 말기
    // (이미 RecvLoop 끝에서 호출되므로 종료 상태일 확률이 높고,
    // 무엇보다 "세션 스레드에서 실행될 수" 있어서 위험)
}

void SessionManager::StopAll()
{
    std::vector<std::shared_ptr<Session>> local;

    {
        std::lock_guard<std::mutex> lock(_mtx);
        for (auto& kv : _sessions)
            local.push_back(kv.second);
        _sessions.clear();

        // zombies도 같이 꺼내기
        for (auto& z : _zombies)
            local.push_back(z);
        _zombies.clear();
    }

    for (auto& s : local)
        s->Stop();
}

size_t SessionManager::Count() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _sessions.size();
}

void SessionManager::ReapClosed()
{
    std::vector<std::shared_ptr<Session>> local;

    {
        std::lock_guard<std::mutex> lock(_mtx);
        local.swap(_zombies);
    }

    for (auto& s : local)
        s->Stop();  // 외부 스레드에서 join
}
