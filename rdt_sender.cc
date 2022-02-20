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

#define DUP_UPPERBOUND 3

std::list <packet> window;
std::queue <packet> buffer;
std::unordered_map<int, int> dup_ack;
unsigned int seq = 1;
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

inline void SendOrBuffer(packet *pkt) {
    if (window.size() < window_size) {
        window.push_back(*pkt);
        printf("Sender send pkt(seq = %d, checksum = %d, size = %d)\n", *(unsigned int *) &pkt->data[1],
               *(unsigned short *) &pkt->data[9], pkt->data[0]);
        Sender_ToLowerLayer(pkt);
    } else {
        buffer.push(*pkt);
    }
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

//    std::cout << "msg size is: " << msg->size << std::endl;

    while (msg->size - cursor > maxpayload_size) {
        /* fill in the packet */
        pkt.data[0] = maxpayload_size;
        *(unsigned int *) (&pkt.data[1]) = seq;
        *(unsigned int *) (&pkt.data[5]) = 1;
        *(unsigned short *) (&pkt.data[9]) = checksum((unsigned short *) (msg->data + cursor), maxpayload_size);
        memcpy(pkt.data + header_size, msg->data + cursor, maxpayload_size);

        ++seq;
        SendOrBuffer(&pkt);
        /* move the cursor */
        cursor += maxpayload_size;
    }

    /* send out the last packet */
    if (msg->size > cursor) {
        /* fill in the packet */
        pkt.data[0] = msg->size - cursor;
        *(unsigned int *) (&pkt.data[1]) = seq++;
        *(unsigned int *) (&pkt.data[5]) = 1;
        *(unsigned short *) (&pkt.data[9]) = checksum((unsigned short *) (msg->data + cursor), (int) pkt.data[0]);
        memcpy(pkt.data + header_size, msg->data + cursor, pkt.data[0]);
        SendOrBuffer(&pkt);
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt) {
    unsigned int ack = *(unsigned int *) (&pkt->data[5]);
    printf("Received ack from receiver: %d, and dup_ack[%d] = %d\n", ack, ack, dup_ack[ack] + 1);
    /* Fast retransmit */
    if (++dup_ack[ack] >= DUP_UPPERBOUND) {
        auto pkt_iter = std::find_if(window.begin(), window.end(), [ack](const packet &pkt) {
            return *(unsigned int *) &pkt.data[1] == ack;
        });
        dup_ack[ack] = 0;
        Sender_ToLowerLayer(&(*pkt_iter));
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
        Sender_ToLowerLayer(&buffer.front());
        window.push_back(buffer.front());
        buffer.pop();
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout() {
}
