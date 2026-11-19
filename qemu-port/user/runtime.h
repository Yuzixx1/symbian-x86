#ifndef QPORT_USER_RUNTIME_H
#define QPORT_USER_RUNTIME_H

/* Small freestanding port runtime, not the Symbian EUSER SDK. */
typedef unsigned short U16;
int command_line(U16* buffer, unsigned capacity);
void print(const char* text);
void print_n(const char* text, unsigned length);
void process_exit(int reason) __attribute__((noreturn));
int app_main(void);
int file_open(const char* absolute_path);
int file_read(int handle, void* buffer, unsigned capacity);
void file_close(int handle);
int file_create_write(const char* absolute_path);
int file_append(int handle, const void* data, unsigned length);
int file_commit(int handle);
int file_save(const char* absolute_path, const void* data, unsigned length);
int change_dir(const char* path);
int get_cwd(char* buffer, unsigned capacity);
void* malloc(unsigned size);
void free(void* ptr);
/* Genuine kernel chunks (RChunk executives): private or global (named) memory
   that processes can share. Handles are process-owned. */
int chunk_create(const char* name, unsigned initial_size, unsigned max_size);
int chunk_open(const char* name);
void* chunk_base(int handle);
int chunk_size(int handle);
int chunk_adjust(int handle, unsigned new_size);
/* Genuine kernel executives (no server): sleep, yield, time, random. */
void sleep_us(int microseconds);
void sleep_ms(int milliseconds);
void thread_yield(void);
void thread_park(void);    /* block until signalled (holds resources) */
int time_now(void);       /* seconds since 2000-01-01, like the RTC */
unsigned random32(void);  /* Math::Random */

/* ------------------------------------------------------------------ wait/exit
 * Genuine executive numbers (build/export/exec_enum.h). The logon calls are
 * SLOW_EXEC3: ECX=handle, EDX=TRequestStatus*, EBX=rendezvous. A logon status
 * completes with the target's exit reason; the getters give type/reason while
 * our handle keeps the DProcess alive. */
#define QE_EXEC_PROCESS_EXIT_TYPE       24u
#define QE_EXEC_PROCESS_EXIT_REASON     25u
#define QE_EXEC_TIMER_CANCEL            52u
#define QE_EXEC_TIMER_AFTER             53u
#define QE_EXEC_REQUEST_SIGNAL          58u
#define QE_EXEC_HANDLE_CLOSE           106u
#define QE_EXEC_THREAD_OPEN_BY_ID      113u
#define QE_EXEC_PROCESS_OPEN_BY_ID     114u
#define QE_EXEC_THREAD_KILL            115u
#define QE_EXEC_THREAD_LOGON           116u
#define QE_EXEC_THREAD_LOGON_CANCEL    117u
#define QE_EXEC_PROCESS_KILL           122u
#define QE_EXEC_PROCESS_LOGON          123u
#define QE_EXEC_PROCESS_LOGON_CANCEL   124u
#define QE_EXEC_TIMER_CREATE           133u
#define QE_OWNER_PROCESS               0
#define QE_KREQUEST_PENDING            ((int)0x80000001u)
#define QE_KERR_TIMED_OUT              (-33)

int  process_open_by_id(unsigned aId);
void process_logon(int aHandle, volatile int* aStatus);
int  process_logon_cancel(int aHandle, volatile int* aStatus);
void thread_logon(int aHandle, volatile int* aStatus);
int  thread_logon_cancel(int aHandle, volatile int* aStatus);
/* Returns the logon status (target exit reason), or a negative Symbian error.
 * A negative timeout waits forever; otherwise poll every ~20 ms. */
int  process_wait_exit(unsigned aPid, int* aExitReason, int* aExitType,
                       int aTimeoutMs);

/* Transitional console input (PortCon lease); a real console server
   replaces this. See con_acquire contract before use. */
int con_acquire(void);    /* exclusive input lease; KErrInUse if held */
void con_release(void);
int con_tryread(void);    /* char 0..255, or negative (empty = KErrNotReady) */
int read_line(char* buffer, unsigned capacity, int timeout_ms);

#endif
