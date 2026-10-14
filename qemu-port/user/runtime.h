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
/* Genuine kernel executives (no server): sleep, yield, time, random. */
void sleep_us(int microseconds);
void sleep_ms(int milliseconds);
void thread_yield(void);
int time_now(void);       /* seconds since 2000-01-01, like the RTC */
unsigned random32(void);  /* Math::Random */
/* Transitional console input (PortCon lease); a real console server
   replaces this. See con_acquire contract before use. */
int con_acquire(void);    /* exclusive input lease; KErrInUse if held */
void con_release(void);
int con_tryread(void);    /* char 0..255, or negative (empty = KErrNotReady) */
int read_line(char* buffer, unsigned capacity, int timeout_ms);

#endif
