#ifndef __MCP2SIO_H__
#define __MCP2SIO_H__

#include "stdint.h"
#include <irx.h>

enum MCP_COMMAND {
  MCP_PING = 1,
  MCP_SET_GAMEID,
  MCP_CARD_CHANNEL_CHANGE,
  MCP_CARD_SLOT_CHANGE,
  MCP_GET_ACCESS_MODE,
  MCP_SET_ACCESS_MODE,
  MCP_READ_BLOCK_MULTI,
  MCP_READ_BLOCK_SINGLE,
  MCP_WRITE_BLOCK_MULTI,
  MCP_WRITE_BLOCK_SINGLE
};

// Let's have a prototype for our export!
int sendCommandWithBuffer(uint8_t command, uint8_t *data, int size);

#define mcp2sio_IMPORTS_start DECLARE_IMPORT_TABLE(mcp2sio, 1, 1)
#define mcp2sio_IMPORTS_end END_IMPORT_TABLE

#define I_sendCommandWithBuffer DECLARE_IMPORT(4, sendCommandWithBuffer)

#endif
