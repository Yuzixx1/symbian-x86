# R1 DNS Resolver E32 Design v1 — read-only investigation

**State:** no NIC/stack/socket service exists yet (see `docs/r1_net_design_v1.md`); there is no DNS or socket EUSER ordinal — a socket API must be a new IPC service. The user pipeline (`tools/elf2e32.py:84,93`) emits *import-free* E32 (dllRefs=0, iImpOff=0): all user E32s statically link `euser_shim.cpp` + `runtime.c` + `ipc.c` and call executives via `exec0/2/4` (`int 0x21`). PE2 imports are only exercised by hand-built `mke32.py` images; the loader supports ≤16 ordinals bound from the genuine EUSER.DLL or veneers 1=Exit, 2=PutC, 3=CmdLen (`variant/e32load.cpp:445-466,840-868`).

## DNS message format (RFC 1035, big-endian)
Header (12B): ID u16 | Flags u16 (QR, RD=0x0100, RA, RCODE) | QDCOUNT/ANCOUNT/NSCOUNT/ARCOUNT u16.
Question: QNAME = length-prefixed labels + 0 (e.g. `3www 7example 3com 0`), QTYPE u16 (A=1), QCLASS u16 (IN=1).
RR: NAME (u16 compression pointer 0xC00n or labels) | TYPE u16 | CLASS u16 | TTL u32 | RDLENGTH u16 | RDATA (A = 4-byte IPv4). Query: ID=`random32()&0xffff`, Flags=RD, QD=1, rest 0; UDP payload to 8.8.8.8:53. Response: match ID, QR=1, RCODE=0; skip CNAME/TXT, take first A RDATA as IPv4.

## Resolver E32
`DNSSRV.E32` — standalone `CServer2` named "DnsResolverServer": `app_main` installs `CActiveScheduler`, constructs server, `Start()` (`e32base.h:368`) blocks in `ipc_receive`; EConnect builds a `CSession2`; `ServiceL` handles EResolve: `RMessage2::Read(0, TPtrC8 hostname)`, `RMessage2::Write(1, TPtr8 4-byte IPv4)`, `Complete(KErrNone/KErrNotFound)` (one terminal reply). `CActive` + async `ipc_call_async` (exec 37) issues UDP send/recv with `TRequestStatus`; `sleep_ms()`/User::After give timeout/retry; cache `{hostname→IPv4, TTL}`; server IP from `command_line()` (default 8.8.8.8). Clients use `ipc_connect`/`ipc_call_args` with TIpcArgs slots (hostname, result).

## EUSER ordinals needed
None new in practice — build import-free like PORTFILESRV. If PE2 imports are required, relevant *genuine* ordinals are 641 Exit, 604/605 WaitForRequest, 626 WaitForAnyRequest, 617 RequestComplete, 645 After, 211 RSessionBase::CreateSession, 424–428 CActiveScheduler, 208/203 CleanupStack, 651 AllocL, 564 Mem::Copy; the port's own veneers are only 1/2/3. No DNS/socket ordinal exists anywhere.

## Network stack integration
No direct NIC use: DNS is a UDP client of the planned ESOCK-like IPC service (N-03) — UdpOpen/SendTo/RecvFrom function numbers with SIpcCopyInfo descriptor slots and async completion; kernel DFC → global named chunk → user stack task (N-01/N-02). Until then, unit-test encode/parse on canned buffers; a raw-IP fallback via `chunk_create/chunk_open/chunk_base` still needs N-01. Build sketch (not applied): `dns_resolver.o` (CXX, USER_FLAGS, -Iuser) linked with start.o/runtime.o/ipc.o/euser_shim.o/malloc.o via `ld --emit-relocs -T user/link.ld`, then elf2e32.py → `DNSSRV.E32`.
