#ifndef __MCP2SIO_H__
#define __MCP2SIO_H__

#include <irx.h>

// Let's have a prototype for our export!
void hello(void);

#define mcp2sio_IMPORTS_start DECLARE_IMPORT_TABLE(mcp2sio, 1, 1)
#define mcp2sio_IMPORTS_end END_IMPORT_TABLE

#define I_hello DECLARE_IMPORT(4, hello)

#endif
