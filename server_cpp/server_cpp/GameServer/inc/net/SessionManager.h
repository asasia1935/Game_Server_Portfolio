#pragma once
#include <winsock2.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

class Session;

class SessionManager
{
public:
    using SessionId = uint64_t;

public:
    SessionManager() = default;
    ~SessionManager();

    // accept된 소켓으로 세션 생성 + 등록
    std::shared_ptr<Session> CreateAndAdd(SOCKET clientSock);

    // Session에서 onClose로 호출: 컨테이너에서 제거
    void Remove(SessionId id);

    // 서버 종료 시 전체 종료(외부 스레드에서 호출)
    void StopAll();

    size_t Count() const;

private:
    mutable std::mutex _mtx;
    std::unordered_map<SessionId, std::shared_ptr<Session>> _sessions;

    std::atomic<SessionId> _idGen{ 0 };
};