# Game_Server_Portfolio
C++ real-time co-op PvE game server with Go-based service APIs

## Architecture Overview

[ Game Client ]
|
| HTTP (REST)
v
[ Go Service ]

- Auth / Ticket
- Matchmaking
- Progress Save
|
| CreateRoom
v
[ C++ Game Server ]
- TCP Session
- Tick Loop (30Hz)
- Authoritative Logic

- 게임 외적 기능은 Go REST API에서 처리
- 실시간 게임 플레이는 C++ 서버에서 Tick 기반으로 처리
- Tick 루프에서는 외부 I/O를 금지

## Tech Stack

### C++ Game Server
- C++17
- TCP (length-prefix framing)
- Tick-based update loop (30Hz)
- Server-authoritative game logic

### Go Service
- Go
- REST API
- Redis (queue / cache)
- Persistent storage (checkpoint)

## Design Principles

- Real-time communication uses **TCP only**
- No blocking I/O inside the tick loop
- All game logic is **server-authoritative**
- Service logic is separated from the game loop

## Tier1 Completion Criteria

> The client sends requests only; the server performs judgement,
> applies results, and owns the authoritative game state.

- [ ] TCP session + packet framing
- [ ] Ticket-based connection authentication
- [ ] Tick loop + event queue
- [ ] Authoritative input handling
- [ ] Snapshot broadcast
- [ ] Segment FSM
- [ ] Checkpoint save via Go service
