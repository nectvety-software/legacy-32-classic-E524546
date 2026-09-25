#pragma once
// Portable directory creation for the POSIX host shims.
//
// `mkdir(path, mode)` is a POSIX signature. MinGW's <io.h> instead declares the
// single-argument `int mkdir(const char *)`, so the two-argument form fails to
// compile there ("too many arguments to function"), and it fails *hard* because
// the host tests build with -Werror. Windows also has no meaningful mode bits
// for a directory, so the mode is simply dropped.
//
// Both tools/qeapp_host and tools/storage_host include this file.
#ifdef _WIN32
#  include <direct.h>
inline int host_mkdir(const char *path, int /*mode*/) { return _mkdir(path); }
#else
#  include <sys/stat.h>
inline int host_mkdir(const char *path, int mode) { return ::mkdir(path, mode); }
#endif
