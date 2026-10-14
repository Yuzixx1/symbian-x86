#ifndef QPORT_IPC_H
#define QPORT_IPC_H

/* ABI from SymbianSource e32cmn.h: RMessage2 and TIpcArgs.
   This is a freestanding bridge to the real kernel IPC, not EUSER C++ ABI. */
struct IpcMessage {
    unsigned handle;
    int function, args[4], spare1;
    void* session;
    int flags, spare3;
};
int ipc_server_create(const char* name);
int ipc_receive(int server, struct IpcMessage* message);
void ipc_complete(struct IpcMessage* message, int result);
void ipc_accept(struct IpcMessage* message, void* cookie);
int ipc_connect(const char* name);
int ipc_call(int session, int function, int argument);
struct IpcArgs { unsigned args[4], flags; };
struct IpcConst8 { unsigned type_length; const void* pointer; };
struct IpcBuffer8 { unsigned type_length, maximum; void* pointer; };
int ipc_call_args(int session, int function, const struct IpcArgs* args);
int ipc_read8(struct IpcMessage* message, int slot, void* buffer, int capacity, int offset);
int ipc_write8(struct IpcMessage* message, int slot, const void* buffer, int length, int offset);
struct IpcConst16 { unsigned type_length; const void* pointer; };
struct IpcBuffer16 { unsigned type_length, maximum; void* pointer; };
int ipc_read16(struct IpcMessage* message, int slot, void* buffer, int capacity, int offset);
int ipc_write16(struct IpcMessage* message, int slot, const void* buffer, int length, int offset);
int ipc_length(struct IpcMessage* message, int slot);
void ipc_receive_async(int server, struct IpcMessage* message, volatile int* status);
void ipc_receive_cancel(int server);
int ipc_wait(volatile int* status);
void ipc_close(int handle);
#endif
