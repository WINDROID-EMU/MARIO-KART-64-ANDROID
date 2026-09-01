#ifndef _NET_H
#define _NET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

// Estrutura de sincronização do estado do jogo (Menu, Pistas, Personagens, Grid de Seleção)
typedef struct {
    uint8_t  type; // 4 = NET_PACKET_GAME_STATE
    uint8_t  gamestate;
    uint8_t  menuSelection;
    uint8_t  subMenuSelection;
    uint8_t  mainMenuSelection;
    uint8_t  playerSelectMenuSelection;
    uint8_t  modeSelection;
    uint8_t  ccSelection;
    uint8_t  playerCountSelection;
    int16_t  courseId;
    uint8_t  cupSelection;
    uint8_t  courseIndexInCup;
    uint8_t  characterSelections[4];
    uint8_t  characterGridSelections[4];
    uint8_t  characterGridIsSelected[4];
    uint16_t randomSeed;
} NetGameStatePacket;

// Estrutura de sincronização de física e efeitos dos karts (Posição, Velocidade, Efeitos, Dano, Itens)
typedef struct {
    uint8_t  type; // 5 = NET_PACKET_PLAYER_SYNC
    uint8_t  player_idx;
    float    pos[3];
    float    velocity[3];
    int16_t  rotation[3];
    uint32_t effects;
    int32_t  triggers;
    int16_t  currentItem;
    uint16_t kartGraphics;
} NetPlayerSyncPacket;

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

void netSendGameState(
    uint8_t gamestate,
    uint8_t menuSelection,
    uint8_t subMenuSelection,
    uint8_t mainMenuSelection,
    uint8_t playerSelectMenuSelection,
    uint8_t modeSelection,
    uint8_t ccSelection,
    uint8_t playerCount,
    int16_t courseId,
    uint8_t cupSelection,
    uint8_t courseIndexInCup,
    const uint8_t charSelections[4],
    const uint8_t charGridSelections[4],
    const uint8_t charGridIsSelected[4],
    uint16_t randomSeed
);
void netSendPlayerSync(uint8_t playerIdx, const float pos[3], const float velocity[3], const int16_t rotation[3], uint32_t effects, int32_t triggers, int16_t currentItem, uint16_t kartGraphics);
bool netPopGameState(NetGameStatePacket* out);
bool netPopPlayerSync(int playerIdx, NetPlayerSyncPacket* out);

#ifdef __cplusplus
}
#endif

#endif // _NET_H
