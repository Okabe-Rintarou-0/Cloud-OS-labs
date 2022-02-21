/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <queue>
#include <iostream>
#include <list>
#include <algorithm>
#include <unordered_map>

#include "rdt_struct.h"
#include "rdt_sender.h"

//#define DEBUG
#define DUP_UPPERBOUND 3
#define TIMEOUT 0.3

struct TimerChainBlock {
    TimerChainBlock(unsigned int seq, double expire_time) : seq(seq), expire_time(expire_time) {}

    unsigned int seq;
    double expire_time;
};

std::queue <TimerChainBlock> timer_chain;

std::list <packet> window;
std::queue <packet> buffer;
std::unordered_map<int, int> dup_ack;
unsigned int seq = 0;
unsigned int current_ack = 1;
unsigned int window_size = 8;

/* sender initialization, called once at the very beginning */
void Sender_Init() {
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final() {
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

inline unsigned short checksum(unsigned short *data, int size) {
    long long sum = 0;
    while (size > 1) {
        sum += *data++;
        size -= 2;
    }

    if (size > 0) {
        char left_over[2] = {0};
        left_over[0] = *(char *) data;
        sum += *(unsigned short *) left_over;
    }

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}

inline void SendToLower(packet *pkt) {
#ifdef DEBUG
    printf("Sender send pkt(seq = %d, checksum = %d, size = %d)\n", *(unsigned int *) &pkt->data[1],
           *(unsigned short *) &pkt->data[9], pkt->data[0]);
#endif
    if (timer_chain.empty()) {
        ASSERT(!Sender_isTimerSet());
        Sender_StartTimer(TIMEOUT);
    }

    timer_chain.push(TimerChainBlock(*(unsigned int *) &pkt->data[1], GetSimulationTime() + TIMEOUT));

    Sender_ToLowerLayer(pkt);
}

inline void SendOrBuffer(packet *pkt) {
    unsigned int seq = *(unsigned int *) &pkt->data[1];
    if (window.size() < window_size) {
        window.push_back(*pkt);
#ifdef DEBUG
        printf("Sender send pkt now(seq = %d)\n", seq);
#endif
        SendToLower(pkt);
    } else {
#ifdef DEBUG
        printf("Sender put pkt into buffer(seq = %d)\n", seq);
#endif
        buffer.push(*pkt);
    }
}

void FillPacket(packet *pkt, int size, int seq, int ack, char *data) {
    constexpr static int header_size = 11;
    memset(pkt, 0, sizeof(packet));
    pkt->data[0] = size;
    *(unsigned int *) (&pkt->data[1]) = seq;
    *(unsigned int *) (&pkt->data[5]) = 1;
    memcpy(pkt->data + header_size, data, size);
    *(unsigned short *) (&pkt->data[9]) = checksum((unsigned short *) pkt, size + header_size);
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg) {
    /*
     * struct is as follow
     * |<- data length(1 byte) ->||<- seq(4 bytes) ->||<- ack(4 bytes) ->||<- checksum(2 bytes) ->|
     * */

    int header_size = 11;

    /* maximum payload size */
    int maxpayload_size = RDT_PKTSIZE - header_size;

    /* split the message if it is too big */

    /* reuse the same packet data structure */
    packet pkt;

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;

    while (msg->size - cursor > maxpayload_size) {
        /* fill in the packet */
        FillPacket(&pkt, maxpayload_size, ++seq, 1, msg->data + cursor);
        SendOrBuffer(&pkt);
        /* move the cursor */
        cursor += maxpayload_size;
    }

    /* send out the last packet */
    if (msg->size > cursor) {
        /* fill in the packet */
        FillPacket(&pkt, msg->size - cursor, ++seq, 1, msg->data + cursor);
        SendOrBuffer(&pkt);
    }
}

void Retransmit(unsigned int seq) {
#ifdef DEBUG
    printf("Retransmit to seq = %d\n", seq);
#endif
    auto pkt_iter = std::find_if(window.begin(), window.end(), [seq](const packet &pkt) {
        return *(unsigned int *) &pkt.data[1] == seq;
    });
    if (pkt_iter != window.end())
        SendToLower(&(*pkt_iter));
}

inline bool PacketNotCorrupted(packet *pkt) {
    constexpr static int header_size = 11;
    unsigned int size = pkt->data[0];
    unsigned int ack = *(unsigned int *) &pkt->data[5];
    if (size < 0 || size > RDT_PKTSIZE || ack > seq + 1)
        return false;
    int pkt_checksum = *(unsigned short *) &pkt->data[9];
    /* Set the checksum to zero first, then calculate the checksum */
    *(unsigned short *) &pkt->data[9] = 0;
    int real_checksum = checksum((unsigned short *) pkt, pkt->data[0] + header_size);
#ifdef DEBUG
    printf("pkt_checksum = %d and real_checksum = %d\n", pkt_checksum, real_checksum);
#endif
    return pkt_checksum == real_checksum;
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt) {
    if (!PacketNotCorrupted(pkt)) {
#ifdef DEBUG
        printf("Sender receive corrupted pkt\n");
#endif
        return;
    }

    unsigned int ack = *(unsigned int *) (&pkt->data[5]);
#ifdef DEBUG
    printf("Received ack from receiver: %d, and dup_ack[%d] = %d\n", ack, ack, dup_ack[ack] + 1);
#endif
    /* Fast retransmit */
    if (++dup_ack[ack] >= DUP_UPPERBOUND) {
        dup_ack[ack] = 0;
        Retransmit(ack);
    }

    /* Move the window */
    if (ack > current_ack) {
        while (!window.empty()) {
            packet &front = window.front();
            unsigned int front_seq = *(unsigned int *) &front.data[1];
            if (front_seq < ack) {
                window.pop_front();
            } else break;
        }
        current_ack = ack;
    }

    while (window.size() < window_size && !buffer.empty()) {
        packet &front = buffer.front();
        SendToLower(&front);
        window.push_back(front);
        buffer.pop();
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout() {
    ASSERT(!timer_chain.empty());
    TimerChainBlock &front = timer_chain.front();
#ifdef DEBUG
    printf("Timeout(seq = %d, current_ack = %d)\n", front.seq, current_ack);
#endif
    if (front.seq >= current_ack) {
        Retransmit(front.seq);
    }
    timer_chain.pop();
    if (!timer_chain.empty()) {
        double next_expire_time = timer_chain.front().expire_time;
        double internal = next_expire_time - GetSimulationTime();
        Sender_StartTimer(internal);
    }
}
