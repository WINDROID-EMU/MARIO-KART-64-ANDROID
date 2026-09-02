#include "net/net.h"
#include "net/enet.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern uint16_t gRandomSeed16;
extern int32_t  gGamestate;
extern int32_t  gMenuSelection;
extern int8_t   gSubMenuSelection;
extern int8_t   gMainMenuSelection;
extern int8_t   gPlayerSelectMenuSelection;
extern int32_t  gModeSelection;
extern int32_t  gCCSelection;
extern int32_t  gPlayerCountSelection1;
extern int16_t  gCurrentCourseId;
extern int8_t   gCupSelection;
extern int8_t   gCourseIndexInCup;
extern int8_t   gCharacterSelections[];
extern int8_t   gCharacterGridSelections[];
extern bool     gCharacterGridIsSelected[];

#if defined(ANDROID) || defined(__ANDROID__)
#include <android/log.h>
#define NET_LOG(...) __android_log_print(ANDROID_LOG_INFO, "MK64_NET", __VA_ARGS__)
#else
#define NET_LOG(...) printf("[MK64_NET] " __VA_ARGS__); printf("\n")
#endif

int32_t  g_NetMode = NET_MODE_NONE;
int32_t  g_NetPlayerIndex = 0;
uint32_t g_NetServerPort = NET_DEFAULT_PORT;
char     g_NetServerAddr[256] = "127.0.0.1";
bool     g_NetConnected = false;
uint32_t g_NetConnectedPlayers = 1; // P1 local por padrão

static ENetHost*   g_Host = NULL;
static ENetPeer*   g_ServerPeer = NULL;
static ENetPeer*   g_ClientPeers[NET_MAX_CLIENTS] = { NULL };
static uint32_t    g_FrameSeq = 0;

// Armazenamento de estado dos 4 controles recebidos da rede
typedef struct {
    uint16_t button;
    int8_t   stick_x;
    int8_t   stick_y;
    int8_t   right_stick_x;
    int8_t   right_stick_y;
    uint32_t last_seq;
} NetPlayerPadState;

static NetPlayerPadState g_RemotePads[NET_MAX_CLIENTS];

enum NetPacketType {
    NET_PACKET_HANDSHAKE_REQ = 1,
    NET_PACKET_HANDSHAKE_ACK = 2,
    NET_PACKET_PAD_INPUT     = 3,
    NET_PACKET_GAME_STATE    = 4,
    NET_PACKET_PLAYER_SYNC   = 5,
    NET_PACKET_ACTORS_SYNC   = 6,
    NET_PACKET_RACE_SYNC     = 7
};

typedef struct {
    uint8_t  type;
    uint8_t  assigned_slot;
    uint16_t random_seed;
    uint32_t connected_players;
} NetHandshakePacket;

static NetGameStatePacket  g_LastGameState = { 0 };
static bool                g_HasNewGameState = false;
static NetPlayerSyncPacket g_LastPlayerSync[NET_MAX_RACERS] = { 0 };
static bool                g_HasNewPlayerSync[NET_MAX_RACERS] = { false };
static NetActorsSyncPacket g_LastActorsSync = { 0 };
static bool                g_HasNewActorsSync = false;
static NetRaceSyncPacket   g_LastRaceSync = { 0 };
static bool                g_HasNewRaceSync = false;

void netInit(void) {
    if (enet_initialize() != 0) {
        NET_LOG("Falha ao inicializar a biblioteca ENet.");
        return;
    }
    NET_LOG("ENet inicializado com sucesso.");
    memset(g_RemotePads, 0, sizeof(g_RemotePads));
    memset(&g_LastGameState, 0, sizeof(g_LastGameState));
    memset(g_LastPlayerSync, 0, sizeof(g_LastPlayerSync));
    g_HasNewGameState = false;
    for (int i = 0; i < NET_MAX_RACERS; i++) g_HasNewPlayerSync[i] = false;
}

void netShutdown(void) {
    if (g_Host != NULL) {
        if (g_ServerPeer != NULL) {
            enet_peer_disconnect_now(g_ServerPeer, 0);
            g_ServerPeer = NULL;
        }
        for (int i = 0; i < NET_MAX_CLIENTS; i++) {
            if (g_ClientPeers[i] != NULL) {
                enet_peer_disconnect_now(g_ClientPeers[i], 0);
                g_ClientPeers[i] = NULL;
            }
        }
        enet_host_destroy(g_Host);
        g_Host = NULL;
    }
    enet_deinitialize();
    g_NetConnected = false;
    NET_LOG("ENet encerrado.");
}

void netSetMode(int32_t mode, const char* addr, uint32_t port) {
    netInit();

    // Fecha sockets anteriores se houver
    if (g_Host != NULL) {
        if (g_ServerPeer != NULL) {
            enet_peer_disconnect_now(g_ServerPeer, 0);
            g_ServerPeer = NULL;
        }
        for (int i = 0; i < NET_MAX_CLIENTS; i++) {
            if (g_ClientPeers[i] != NULL) {
                enet_peer_disconnect_now(g_ClientPeers[i], 0);
                g_ClientPeers[i] = NULL;
            }
        }
        enet_host_destroy(g_Host);
        g_Host = NULL;
    }

    g_NetMode = mode;
    if (port > 0) {
        g_NetServerPort = port;
    }
    if (addr && addr[0]) {
        strncpy(g_NetServerAddr, addr, sizeof(g_NetServerAddr) - 1);
        g_NetServerAddr[sizeof(g_NetServerAddr) - 1] = '\0';
    }

    NET_LOG("netSetMode: mode=%d, addr=%s, port=%u", g_NetMode, g_NetServerAddr, g_NetServerPort);

    if (g_NetMode == NET_MODE_HOST) {
        g_NetPlayerIndex = 0; // Host é sempre o Jogador 1 (slot 0)
        g_NetConnectedPlayers = 1;

        ENetAddress address;
        enet_address_set_ip(&address, "0.0.0.0");
        address.port = (uint16_t)g_NetServerPort;

        g_Host = enet_host_create(&address, NET_MAX_CLIENTS, 2, 0, 0, 0);
        if (g_Host == NULL) {
            NET_LOG("Erro ao criar ENet Host na porta %u", g_NetServerPort);
        } else {
            g_NetConnected = true;
            NET_LOG("Servidor Netplay Host iniciado com sucesso na porta %u!", g_NetServerPort);
        }
    } else if (g_NetMode == NET_MODE_JOIN) {
        g_NetPlayerIndex = 1; // Padrão jogador 2 até receber ACK do host
        g_NetConnected = false;

        g_Host = enet_host_create(NULL, 1, 2, 0, 0, 0);
        if (g_Host == NULL) {
            NET_LOG("Erro ao criar ENet Client!");
            return;
        }

        ENetAddress address;
        if (enet_address_set_hostname(&address, g_NetServerAddr) != 0) {
            enet_address_set_ip(&address, g_NetServerAddr);
        }
        address.port = (uint16_t)g_NetServerPort;

        NET_LOG("Conectando ao Host %s:%u...", g_NetServerAddr, g_NetServerPort);
        g_ServerPeer = enet_host_connect(g_Host, &address, 2, 0);
        if (g_ServerPeer == NULL) {
            NET_LOG("Nenhum peer disponível para iniciar conexão.");
        }
    }
}

void netSendLocalInput(uint8_t playerIdx, uint16_t button, int8_t stick_x, int8_t stick_y, int8_t right_stick_x, int8_t right_stick_y) {
    // Atualiza estado local imediatamente no buffer de pads
    if (playerIdx < NET_MAX_CLIENTS) {
        g_RemotePads[playerIdx].button = button;
        g_RemotePads[playerIdx].stick_x = stick_x;
        g_RemotePads[playerIdx].stick_y = stick_y;
        g_RemotePads[playerIdx].right_stick_x = right_stick_x;
        g_RemotePads[playerIdx].right_stick_y = right_stick_y;
    }

    if (g_NetMode == NET_MODE_NONE || g_Host == NULL) {
        return;
    }

    NetPadInputPacket pkt;
    pkt.button = button;
    pkt.stick_x = stick_x;
    pkt.stick_y = stick_y;
    pkt.right_stick_x = right_stick_x;
    pkt.right_stick_y = right_stick_y;
    pkt.player_idx = (uint8_t)g_NetPlayerIndex;
    pkt.frame_seq = ++g_FrameSeq;

    ENetPacket* packet = enet_packet_create(&pkt, sizeof(NetPadInputPacket), ENET_PACKET_FLAG_UNSEQUENCED);

    if (g_NetMode == NET_MODE_HOST) {
        // Host faz broadcast para todos os clientes
        enet_host_broadcast(g_Host, 0, packet);
    } else if (g_NetMode == NET_MODE_JOIN && g_ServerPeer != NULL && g_NetConnected) {
        // Client envia seu input para o Host
        enet_peer_send(g_ServerPeer, 0, packet);
    }
}

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
    uint16_t randomSeed,
    uint8_t gotoMode
) {
    if (g_NetMode != NET_MODE_HOST || g_Host == NULL) {
        return;
    }

    NetGameStatePacket pkt;
    pkt.type = NET_PACKET_GAME_STATE;
    pkt.gamestate = gamestate;
    pkt.menuSelection = menuSelection;
    pkt.subMenuSelection = subMenuSelection;
    pkt.mainMenuSelection = mainMenuSelection;
    pkt.playerSelectMenuSelection = playerSelectMenuSelection;
    pkt.modeSelection = modeSelection;
    pkt.ccSelection = ccSelection;
    pkt.playerCountSelection = playerCount;
    pkt.courseId = courseId;
    pkt.cupSelection = cupSelection;
    pkt.courseIndexInCup = courseIndexInCup;
    if (charSelections) {
        for (int i = 0; i < 4; i++) pkt.characterSelections[i] = charSelections[i];
    } else {
        memset(pkt.characterSelections, 0, sizeof(pkt.characterSelections));
    }
    if (charGridSelections) {
        for (int i = 0; i < 4; i++) pkt.characterGridSelections[i] = charGridSelections[i];
    } else {
        memset(pkt.characterGridSelections, 0, sizeof(pkt.characterGridSelections));
    }
    if (charGridIsSelected) {
        for (int i = 0; i < 4; i++) pkt.characterGridIsSelected[i] = charGridIsSelected[i];
    } else {
        memset(pkt.characterGridIsSelected, 0, sizeof(pkt.characterGridIsSelected));
    }
    pkt.randomSeed = randomSeed;
    pkt.gotoMode = gotoMode;

    ENetPacket* enetPkt = enet_packet_create(&pkt, sizeof(NetGameStatePacket), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(g_Host, 1, enetPkt);
}

void netSendPlayerSync(
    uint8_t playerIdx,
    const float pos[3],
    const float velocity[3],
    const int16_t rotation[3],
    uint32_t effects,
    int32_t triggers,
    int16_t currentItem,
    uint16_t kartGraphics,
    uint8_t rank,
    uint8_t lap,
    float lapCompletion
) {
    if (g_NetMode == NET_MODE_NONE || g_Host == NULL) {
        return;
    }

    NetPlayerSyncPacket pkt;
    pkt.type = NET_PACKET_PLAYER_SYNC;
    pkt.player_idx = playerIdx;
    if (pos) { pkt.pos[0] = pos[0]; pkt.pos[1] = pos[1]; pkt.pos[2] = pos[2]; }
    if (velocity) { pkt.velocity[0] = velocity[0]; pkt.velocity[1] = velocity[1]; pkt.velocity[2] = velocity[2]; }
    if (rotation) { pkt.rotation[0] = rotation[0]; pkt.rotation[1] = rotation[1]; pkt.rotation[2] = rotation[2]; }
    pkt.effects = effects;
    pkt.triggers = triggers;
    pkt.currentItem = currentItem;
    pkt.kartGraphics = kartGraphics;
    pkt.rank = rank;
    pkt.lap = lap;
    pkt.lapCompletion = lapCompletion;

    if (playerIdx < NET_MAX_RACERS) {
        g_LastPlayerSync[playerIdx] = pkt;
    }

    ENetPacket* enetPkt = enet_packet_create(&pkt, sizeof(NetPlayerSyncPacket), ENET_PACKET_FLAG_UNSEQUENCED);
    if (g_NetMode == NET_MODE_HOST) {
        enet_host_broadcast(g_Host, 0, enetPkt);
    } else if (g_NetMode == NET_MODE_JOIN && g_ServerPeer != NULL) {
        enet_peer_send(g_ServerPeer, 0, enetPkt);
    }
}

void netSendActorsSync(const NetActorsSyncPacket* packet) {
    if (g_NetMode != NET_MODE_HOST || g_Host == NULL || packet == NULL) {
        return;
    }
    ENetPacket* enetPkt = enet_packet_create(packet, sizeof(NetActorsSyncPacket), ENET_PACKET_FLAG_UNSEQUENCED);
    enet_host_broadcast(g_Host, 0, enetPkt);
}

void netSendRaceSync(
    const uint8_t laps[4],
    const uint8_t alsoLaps[4],
    const uint8_t raceComplete[4],
    uint8_t raceEnded,
    uint8_t winnerIndex
) {
    if (g_NetMode == NET_MODE_NONE || g_Host == NULL) {
        return;
    }
    NetRaceSyncPacket pkt;
    pkt.type = NET_PACKET_RACE_SYNC;
    for (int i = 0; i < 4; i++) {
        pkt.lapCount[i] = laps ? laps[i] : 0;
        pkt.alsoLapCount[i] = alsoLaps ? alsoLaps[i] : 0;
        pkt.raceComplete[i] = raceComplete ? raceComplete[i] : 0;
    }
    pkt.raceEnded = raceEnded;
    pkt.winnerIndex = winnerIndex;

    ENetPacket* enetPkt = enet_packet_create(&pkt, sizeof(NetRaceSyncPacket), ENET_PACKET_FLAG_RELIABLE);
    if (g_NetMode == NET_MODE_HOST) {
        enet_host_broadcast(g_Host, 1, enetPkt);
    } else if (g_NetMode == NET_MODE_JOIN && g_ServerPeer != NULL) {
        enet_peer_send(g_ServerPeer, 1, enetPkt);
    }
}

bool netPopGameState(NetGameStatePacket* out) {
    if (g_HasNewGameState && out != NULL) {
        *out = g_LastGameState;
        g_HasNewGameState = false;
        return true;
    }
    return false;
}

bool netPopPlayerSync(int playerIdx, NetPlayerSyncPacket* out) {
    if (playerIdx >= 0 && playerIdx < NET_MAX_RACERS && g_HasNewPlayerSync[playerIdx] && out != NULL) {
        *out = g_LastPlayerSync[playerIdx];
        g_HasNewPlayerSync[playerIdx] = false;
        return true;
    }
    return false;
}

bool netPopActorsSync(NetActorsSyncPacket* out) {
    if (g_HasNewActorsSync && out != NULL) {
        *out = g_LastActorsSync;
        g_HasNewActorsSync = false;
        return true;
    }
    return false;
}

bool netPopRaceSync(NetRaceSyncPacket* out) {
    if (g_HasNewRaceSync && out != NULL) {
        *out = g_LastRaceSync;
        g_HasNewRaceSync = false;
        return true;
    }
    return false;
}

void netGetPadData(int playerIdx, uint16_t* button, int8_t* stick_x, int8_t* stick_y, int8_t* right_stick_x, int8_t* right_stick_y) {
    if (playerIdx >= 0 && playerIdx < NET_MAX_CLIENTS) {
        *button = g_RemotePads[playerIdx].button;
        *stick_x = g_RemotePads[playerIdx].stick_x;
        *stick_y = g_RemotePads[playerIdx].stick_y;
        *right_stick_x = g_RemotePads[playerIdx].right_stick_x;
        *right_stick_y = g_RemotePads[playerIdx].right_stick_y;
    }
}

void netUpdate(void) {
    if (g_NetMode == NET_MODE_NONE || g_Host == NULL) {
        return;
    }

    ENetEvent event;
    while (enet_host_service(g_Host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                char peerIp[128] = { 0 };
                enet_address_get_ip(&event.peer->address, peerIp, sizeof(peerIp));
                NET_LOG("Novo peer conectado: %s:%u", peerIp, event.peer->address.port);
                if (g_NetMode == NET_MODE_HOST) {
                    // Atribui próximo slot de jogador livre (1, 2 ou 3)
                    int assigned = -1;
                    for (int i = 1; i < NET_MAX_CLIENTS; i++) {
                        if (g_ClientPeers[i] == NULL) {
                            g_ClientPeers[i] = event.peer;
                            event.peer->data = (void*)(intptr_t)i;
                            assigned = i;
                            g_NetConnectedPlayers |= (1 << i);
                            break;
                        }
                    }
                    if (assigned != -1) {
                        NET_LOG("Jogador atribuído ao Slot %d", assigned + 1);
                        NetHandshakePacket ack;
                        ack.type = NET_PACKET_HANDSHAKE_ACK;
                        ack.assigned_slot = (uint8_t)assigned;
                        ack.random_seed = gRandomSeed16;
                        ack.connected_players = g_NetConnectedPlayers;
                        ENetPacket* ackPkt = enet_packet_create(&ack, sizeof(NetHandshakePacket), ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(event.peer, 1, ackPkt);
                    } else {
                        NET_LOG("Sala cheia! Desconectando peer...");
                        enet_peer_disconnect(event.peer, 0);
                    }
                } else if (g_NetMode == NET_MODE_JOIN) {
                    g_NetConnected = true;
                    NET_LOG("Conectado ao Host com sucesso! Solicitando handshake...");
                    NetHandshakePacket req;
                    req.type = NET_PACKET_HANDSHAKE_REQ;
                    req.assigned_slot = 0;
                    req.random_seed = 0;
                    req.connected_players = 0;
                    ENetPacket* reqPkt = enet_packet_create(&req, sizeof(NetHandshakePacket), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 1, reqPkt);
                }
                break;
            }

            case ENET_EVENT_TYPE_RECEIVE: {
                if (event.packet->dataLength == sizeof(NetPadInputPacket)) {
                    NetPadInputPacket* inPkt = (NetPadInputPacket*)event.packet->data;
                    uint8_t pIdx = inPkt->player_idx;
                    if (pIdx < NET_MAX_CLIENTS) {
                        g_RemotePads[pIdx].button = inPkt->button;
                        g_RemotePads[pIdx].stick_x = inPkt->stick_x;
                        g_RemotePads[pIdx].stick_y = inPkt->stick_y;
                        g_RemotePads[pIdx].right_stick_x = inPkt->right_stick_x;
                        g_RemotePads[pIdx].right_stick_y = inPkt->right_stick_y;
                        g_RemotePads[pIdx].last_seq = inPkt->frame_seq;

                        // Se for o Host recebendo de um cliente, retransmite para os outros clientes
                        if (g_NetMode == NET_MODE_HOST) {
                            ENetPacket* fwdPkt = enet_packet_create(inPkt, sizeof(NetPadInputPacket), ENET_PACKET_FLAG_UNSEQUENCED);
                            for (int i = 1; i < NET_MAX_CLIENTS; i++) {
                                if (g_ClientPeers[i] != NULL && g_ClientPeers[i] != event.peer) {
                                    enet_peer_send(g_ClientPeers[i], 0, fwdPkt);
                                }
                            }
                        }
                    }
                } else if (event.packet->dataLength == sizeof(NetHandshakePacket)) {
                    NetHandshakePacket* hs = (NetHandshakePacket*)event.packet->data;
                    if (hs->type == NET_PACKET_HANDSHAKE_REQ && g_NetMode == NET_MODE_HOST) {
                        int slot = (int)(intptr_t)event.peer->data;
                        if (slot >= 1 && slot < NET_MAX_CLIENTS) {
                            NetHandshakePacket ack;
                            ack.type = NET_PACKET_HANDSHAKE_ACK;
                            ack.assigned_slot = (uint8_t)slot;
                            ack.random_seed = gRandomSeed16;
                            ack.connected_players = g_NetConnectedPlayers;
                            ENetPacket* ackPkt = enet_packet_create(&ack, sizeof(NetHandshakePacket), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(event.peer, 1, ackPkt);

                            // Sincroniza imediatamente o estado completo do jogo na conexão
                            NetGameStatePacket initGs;
                            initGs.type = NET_PACKET_GAME_STATE;
                            initGs.gamestate = (uint8_t)gGamestate;
                            initGs.menuSelection = (uint8_t)gMenuSelection;
                            initGs.subMenuSelection = (uint8_t)gSubMenuSelection;
                            initGs.mainMenuSelection = (uint8_t)gMainMenuSelection;
                            initGs.playerSelectMenuSelection = (uint8_t)gPlayerSelectMenuSelection;
                            initGs.modeSelection = (uint8_t)gModeSelection;
                            initGs.ccSelection = (uint8_t)gCCSelection;
                            initGs.playerCountSelection = (uint8_t)gPlayerCountSelection1;
                            initGs.courseId = gCurrentCourseId;
                            initGs.cupSelection = (uint8_t)gCupSelection;
                            initGs.courseIndexInCup = (uint8_t)gCourseIndexInCup;
                            for (int c = 0; c < 4; c++) {
                                initGs.characterSelections[c] = (uint8_t)gCharacterSelections[c];
                                initGs.characterGridSelections[c] = (uint8_t)gCharacterGridSelections[c];
                                initGs.characterGridIsSelected[c] = (uint8_t)gCharacterGridIsSelected[c];
                            }
                            initGs.randomSeed = gRandomSeed16;
                            ENetPacket* initGsPkt = enet_packet_create(&initGs, sizeof(NetGameStatePacket), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(event.peer, 1, initGsPkt);
                            NET_LOG("Sincronização inicial enviada para o Jogador %d na conexão.", slot + 1);
                        }
                    } else if (hs->type == NET_PACKET_HANDSHAKE_ACK && g_NetMode == NET_MODE_JOIN) {
                        g_NetPlayerIndex = hs->assigned_slot;
                        g_NetConnectedPlayers = hs->connected_players;
                        gRandomSeed16 = hs->random_seed;
                        NET_LOG("Recebido Handshake ACK: Este dispositivo é o Jogador %d (RNG Seed: %u)", g_NetPlayerIndex + 1, (unsigned)gRandomSeed16);
                    }
                } else if (event.packet->dataLength == sizeof(NetGameStatePacket)) {
                    NetGameStatePacket* gs = (NetGameStatePacket*)event.packet->data;
                    if (gs->type == NET_PACKET_GAME_STATE) {
                        g_LastGameState = *gs;
                        g_HasNewGameState = true;

                        // Se Host recebeu (ou reenviou), retransmite aos outros clientes
                        if (g_NetMode == NET_MODE_HOST) {
                            ENetPacket* fwdPkt = enet_packet_create(gs, sizeof(NetGameStatePacket), ENET_PACKET_FLAG_RELIABLE);
                            for (int i = 1; i < NET_MAX_CLIENTS; i++) {
                                if (g_ClientPeers[i] != NULL && g_ClientPeers[i] != event.peer) {
                                    enet_peer_send(g_ClientPeers[i], 1, fwdPkt);
                                }
                            }
                        }
                    }
                } else if (event.packet->dataLength == sizeof(NetPlayerSyncPacket)) {
                    NetPlayerSyncPacket* ps = (NetPlayerSyncPacket*)event.packet->data;
                    if (ps->type == NET_PACKET_PLAYER_SYNC && ps->player_idx < NET_MAX_RACERS) {
                        g_LastPlayerSync[ps->player_idx] = *ps;
                        g_HasNewPlayerSync[ps->player_idx] = true;

                        // Se Host recebeu de um cliente, retransmite aos outros clientes
                        if (g_NetMode == NET_MODE_HOST) {
                            ENetPacket* fwdPkt = enet_packet_create(ps, sizeof(NetPlayerSyncPacket), ENET_PACKET_FLAG_UNSEQUENCED);
                            for (int i = 1; i < NET_MAX_CLIENTS; i++) {
                                if (g_ClientPeers[i] != NULL && g_ClientPeers[i] != event.peer) {
                                    enet_peer_send(g_ClientPeers[i], 0, fwdPkt);
                                }
                            }
                        }
                    }
                } else if (event.packet->dataLength == sizeof(NetActorsSyncPacket)) {
                    NetActorsSyncPacket* as = (NetActorsSyncPacket*)event.packet->data;
                    if (as->type == NET_PACKET_ACTORS_SYNC) {
                        g_LastActorsSync = *as;
                        g_HasNewActorsSync = true;
                    }
                } else if (event.packet->dataLength == sizeof(NetRaceSyncPacket)) {
                    NetRaceSyncPacket* rs = (NetRaceSyncPacket*)event.packet->data;
                    if (rs->type == NET_PACKET_RACE_SYNC) {
                        g_LastRaceSync = *rs;
                        g_HasNewRaceSync = true;

                        // Se Host recebeu de um cliente, retransmite aos outros clientes
                        if (g_NetMode == NET_MODE_HOST) {
                            ENetPacket* fwdPkt = enet_packet_create(rs, sizeof(NetRaceSyncPacket), ENET_PACKET_FLAG_RELIABLE);
                            for (int i = 1; i < NET_MAX_CLIENTS; i++) {
                                if (g_ClientPeers[i] != NULL && g_ClientPeers[i] != event.peer) {
                                    enet_peer_send(g_ClientPeers[i], 1, fwdPkt);
                                }
                            }
                        }
                    }
                }
                enet_packet_destroy(event.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT: {
                NET_LOG("Peer desconectado.");
                if (g_NetMode == NET_MODE_HOST) {
                    int slot = (int)(intptr_t)event.peer->data;
                    if (slot >= 1 && slot < NET_MAX_CLIENTS) {
                        g_ClientPeers[slot] = NULL;
                        g_NetConnectedPlayers &= ~(1 << slot);
                        memset(&g_RemotePads[slot], 0, sizeof(NetPlayerPadState));
                        NET_LOG("Jogador %d desconectou da sala.", slot + 1);
                    }
                } else if (g_NetMode == NET_MODE_JOIN) {
                    g_NetConnected = false;
                    NET_LOG("Desconectado do servidor Host.");
                }
                event.peer->data = NULL;
                break;
            }

            case ENET_EVENT_TYPE_NONE:
                break;
        }
    }
}

#if defined(ANDROID) || defined(__ANDROID__)
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_nativeSetNetMode(JNIEnv *env, jclass clazz, jint mode, jstring ip, jint port) {
    const char *nativeIp = "";
    if (ip != NULL) {
        nativeIp = (*env)->GetStringUTFChars(env, ip, NULL);
    }
    netSetMode(mode, nativeIp, (uint32_t)port);
    if (ip != NULL && nativeIp != NULL && nativeIp[0] != '\0') {
        (*env)->ReleaseStringUTFChars(env, ip, nativeIp);
    }
}

#ifdef __cplusplus
}
#endif
#endif

