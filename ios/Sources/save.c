#include "save.h"
#include <stdio.h>

// Implemented in Save.swift (@_cdecl) — the iOS Documents dir. Declared locally (not in a
// header the Swift bridging header sees) so it doesn't clash with the @_cdecl definition.
extern const char* de_documents_path(void);

static void full_path(char* buf, int bufsz, const char* name) {
    const char* dir = de_documents_path();
    snprintf(buf, bufsz, "%s/%s", dir ? dir : ".", name);
}

int de_save_bytes(const char* name, const void* data, int len) {
    char path[1024]; full_path(path, sizeof path, name);
    FILE* f = fopen(path, "wb"); if (!f) return -1;
    int n = (int)fwrite(data, 1, (size_t)len, f);
    fclose(f); return n;
}

int de_load_bytes(const char* name, void* out, int max) {
    char path[1024]; full_path(path, sizeof path, name);
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    int n = (int)fread(out, 1, (size_t)max, f);
    fclose(f); return n;
}

static int launch_count = 0;
int de_launch_count(void) { return launch_count; }

void de_record_launch(void) {
    int n = 0;
    de_load_bytes("launches.bin", &n, (int)sizeof n);   // n stays 0 if file is missing
    n++;
    de_save_bytes("launches.bin", &n, (int)sizeof n);
    launch_count = n;
}
