#ifndef DE_SAVE_H
#define DE_SAVE_H

// Portable C save API over the iOS sandbox. The only platform-specific piece is the
// writable Documents path (de_documents_path, implemented in Save.swift); the read/write
// here is plain stdio. Mirrors the engine's save_bytes/load_bytes shape.
int  de_save_bytes(const char* name, const void* data, int len);  // bytes written, <0 = error
int  de_load_bytes(const char* name, void* out, int max);          // bytes read, <0 = missing

// Demo of persistence: counts how many times the app has launched (a saved int).
void de_record_launch(void);
int  de_launch_count(void);
#endif
