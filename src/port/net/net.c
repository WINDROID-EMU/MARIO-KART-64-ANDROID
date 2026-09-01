#include "net/net.h"
#include "net/enet.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    NET_PACKET_PAD_INPUT     = 3
};

typedef struct {
    uint8_t  type;
    uint8_t  assigned_slot;
    uint16_t reserved;
} NetHandshakePacket;

void netInit(void) {
    if (enet_initialize() != 0) {
        NET_LOG("Falha ao inicializar a biblioteca ENet.");
        return;
    }
    NET_LOG("ENet inicializado com sucesso.");
    memset(g_RemotePads, 0, sizeof(g_RemotePads));
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
        address.host = ENET_HOST_ANY;
        address.port = (enet_uint16)g_NetServerPort;

        g_Host = enet_host_create(&address, NET_MAX_CLIENTS, 2, 0, 0);
        if (g_Host == NULL) {
            NET_LOG("Erro ao criar ENet Host na porta %u", g_NetServerPort);
        } else {
            g_NetConnected = true;
            NET_LOG("Servidor Netplay Host iniciado com sucesso na porta %u!", g_NetServerPort);
        }
    } else if (g_NetMode == NET_MODE_JOIN) {
        g_NetPlayerIndex = 1; // Padrão jogador 2 até receber ACK do host

        g_Host = enet_host_create(NULL, 1, 2, 0, 0);
        if (g_Host == NULL) {
            NET_LOG("Erro ao criar ENet Client!");
            return;
        }

        ENetAddress address;
        enet_address_set_host(&address, g_NetServerAddr);
        address.port = (enet_uint16)g_NetServerPort;

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
                NET_LOG("Novo peer conectado da porta %x:%u", event.peer->address.host, event.peer->address.port);
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
                        ENetPacket* ackPkt = enet_packet_create(&ack, sizeof(NetHandshakePacket), ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(event.peer, 1, ackPkt);
                    } else {
                        NET_LOG("Sala cheia! Desconectando peer...");
                        enet_peer_disconnect(event.peer, 0);
                    }
                } else if (g_NetMode == NET_MODE_JOIN) {
                    g_NetConnected = true;
                    NET_LOG("Conectado ao Host com sucesso!");
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
                    if (hs->type == NET_PACKET_HANDSHAKE_ACK && g_NetMode == NET_MODE_JOIN) {
                        g_NetPlayerIndex = hs->assigned_slot;
                        NET_LOG("Recebido Handshake ACK: Este dispositivo é o Jogador %d", g_NetPlayerIndex + 1);
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

JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_nativeSetNetMode(JNIEnv *env, jclass clazz, jint mode, jstring ip, jint port) {
    const char *nativeIp = ip ? (*env)->GetStringUTFChars(env, ip, 0) : "";
    netSetMode(mode, nativeIp, (uint32_t)port);
    if (ip && nativeIp) {
        (*env)->ReleaseStringUTFChars(env, ip, nativeIp);
    }
}
#endif

