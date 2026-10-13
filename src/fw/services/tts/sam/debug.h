#ifndef DEBUG_H
#define DEBUG_H

// Firmware build: no diagnostics, as in the C64 build
#define debug 0
#define printf(...) ((void)0)
#define PrintPhonemes(...) ((void)0)
#define PrintOutput(...) ((void)0)
#define PrintRule(...) ((void)0)

#endif
