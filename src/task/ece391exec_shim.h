#ifndef _ECE391EXEC_SHIM_H
#define _ECE391EXEC_SHIM_H

#define ECE391_PAGEADDR 0x08000000
#define ECE391_MAPADDR 0x48000

// Arguments goes on top of the page because why not. Userspace has no API to remap us
#define ECE391_ARGSADDR ECE391_PAGEADDR

#endif
