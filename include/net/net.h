#ifndef _NET_H
#define _NET_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NET_DEFAULT_PORT 27100
#define NET_MAX_CLIENTS  4

enum NetMode {
    NET_MODE_NONE = 0,
    NET_MODE_HOST = 1,
    NET_MODE_JOIN = 2
};

// Estrutura de input enviada pela rede para cada controle
typedef struct {
    uint16_t button;
    int8_t   stick_x;
    int8_t   stick_y;
    int8_t   right_stick_x;
    int8_t   right_stick_y;
    uint8_t  player_idx; // 0..3
    uint32_t frame_seq;
} NetPadInputPacket;

// Variáveis globais de estado de rede
extern int32_t  g_NetMode;
extern int32_t  g_NetPlayerIndex; // 0 = Host (P1), 1 = Client 1 (P2), etc.
extern uint32_t g_NetServerPort;
extern char     g_NetServerAddr[256];
extern bool     g_NetConnected;
extern uint32_t g_NetConnectedPlayers; // máscara de bits de jogadores conectados

// Funções públicas
void netInit(void);
void netShutdown(void);
void netSetMode(int32_t mode, const char* addr, uint32_t port);
void netUpdate(void);
void netSendLocalInput(uint8_t playerIdx, uint16_t button, int8_t stick_x, int8_t stick_y, int8_t right_stick_x, int8_t right_stick_y);
void netGetPadData(int playerIdx, uint16_t* button, int8_t* stick_x, int8_t* stick_y, int8_t* right_stick_x, int8_t* right_stick_y);

#ifdef __cplusplus
}
#endif

#endif // _NET_H
