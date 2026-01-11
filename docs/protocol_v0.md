\# Protocol v0



This document defines the wire protocol for the C++ game server (Tier1).



\## 1. Transport

\- TCP only (Tier1)

\- Message framing: length-prefix



\## 2. Endianness \& Types

\- Endianness: Little-endian

\- Integer types: uint8/16/32, int32

\- Float: float32 (IEEE 754)

\- String: uint16 length + UTF-8 bytes (Tier1: avoid if possible)



\## 3. Frame Format (Length-Prefix)



\### 3.1 Frame Layout

Each TCP frame is:



| Field | Size | Type | Notes |

|------|------|------|------|

| length | 2 bytes | uint16 | Total bytes AFTER `length` field (header+payload) |

| msg\_id | 2 bytes | uint16 | Message type |

| payload | (length-2) bytes | bytes | Message-specific |



\- `length` includes `msg\_id` + `payload`

\- Max frame size (Tier1): 4096 bytes (disconnect if exceeded)



\### 3.2 Receive Rules

\- Accumulate bytes into per-session recv buffer

\- While buffer has >= 2 bytes:

&nbsp; - Peek `length`

&nbsp; - If buffer has < (2 + length) bytes: wait for more

&nbsp; - Else: extract one full frame and dispatch



\## 4. Message IDs



| msg\_id | Name | Direction | Description |

|-------:|------|-----------|-------------|

| 1001 | C\_TicketAuthReq | C -> S | Client sends ticket on connect |

| 1002 | S\_TicketAuthRes | S -> C | Auth result (ok/fail + user\_id) |

| 2001 | C\_MoveInput | C -> S | Movement input request |

| 2002 | C\_CastSkill | C -> S | Skill cast request |

| 3001 | S\_Snapshot | S -> C | World snapshot broadcast |

| 9001 | S\_Disconnect | S -> C | Optional reason before close |



> IDs are stable. Add new messages by appending new IDs only.



\## 5. Connection \& Auth Flow (Tier1)



1\) TCP connect

2\) Client immediately sends `C\_TicketAuthReq`

3\) Server verifies ticket (via Go service) asynchronously

4\) Server replies `S\_TicketAuthRes`

\- If ok: session state -> IN\_GAME

\- If fail: send res then disconnect



\### 5.1 C\_TicketAuthReq (1001)

Payload:

| Field | Type | Notes |

|------|------|------|

| ticket\_len | uint16 | length of ticket bytes |

| ticket | bytes\[ticket\_len] | opaque token |



\### 5.2 S\_TicketAuthRes (1002)

Payload:

| Field | Type | Notes |

|------|------|------|

| ok | uint8 | 1=success, 0=fail |

| reason | uint16 | 0=none, 1=invalid, 2=expired, 3=server\_error |

| user\_id | uint64 | valid only if ok=1 |



\## 6. Input Messages (Authoritative)



\### 6.1 C\_MoveInput (2001)

Client sends intent only. Server clamps speed and validates.



Payload:

| Field | Type | Notes |

|------|------|------|

| seq | uint32 | client input sequence |

| dir\_x | int8 | -1,0,1 |

| dir\_y | int8 | -1,0,1 |

| dt\_ms | uint16 | client-side delta hint (server may ignore) |



\### 6.2 C\_CastSkill (2002)

Payload:

| Field | Type | Notes |

|------|------|------|

| seq | uint32 | input sequence |

| skill\_id | uint16 | skill type |

| target\_x | float32 | optional |

| target\_y | float32 | optional |



\## 7. Snapshot Broadcast



\### 7.1 Rules

\- Server broadcasts `S\_Snapshot` at 10Hz (every 3 ticks at 30Hz) initially

\- Snapshot is authoritative state (client renders from it)

\- Tier1: full snapshot (no delta compression)



\### 7.2 S\_Snapshot (3001)

Payload:

| Field | Type | Notes |

|------|------|------|

| server\_tick | uint32 | tick counter |

| player\_count | uint8 | |

| players | repeated | see below |

| enemy\_count | uint8 | |

| enemies | repeated | see below |

| segment\_state | uint8 | 0=IN\_SEGMENT,1=CLEAR,2=CHOICE,3=TRANSITION |



Player entry:

| Field | Type |

|------|------|

| id | uint64 |

| x | float32 |

| y | float32 |

| hp | uint16 |

| state | uint8 |



Enemy entry:

| Field | Type |

|------|------|

| id | uint32 |

| x | float32 |

| y | float32 |

| hp | uint16 |

| state | uint8 |



\## 8. Error \& Disconnect Policy

\- Unknown msg\_id: disconnect

\- Malformed frame (length too big / payload 부족): disconnect

\- Auth timeout (e.g., 5s without auth completion): disconnect

\- Max recv buffer (e.g., 64KB) exceeded: disconnect

