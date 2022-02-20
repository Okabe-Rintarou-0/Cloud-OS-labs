/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
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
#include <algorithm>
#include <iostream>
#include <list>

#include "rdt_struct.h"
#include "rdt_receiver.h"

int ack = 1;
std::list <packet> buffer;

/* receiver initialization, called once at the very beginning */
void Receiver_Init() {
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final() {
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
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

message *pkt2msg(packet *pkt) {
    /* 1-byte header indicating the size of the payload */
    int header_size = 11;

    /* construct a message and deliver to the upper layer */
    struct message *msg = (struct message *) malloc(sizeof(struct message));
    ASSERT(msg != NULL);

    msg->size = pkt->data[0];

    /* sanity check in case the packet is corrupted */
    if (msg->size < 0) msg->size = 0;
    if (msg->size > RDT_PKTSIZE - header_size) msg->size = RDT_PKTSIZE - header_size;

    msg->data = (char *) malloc(msg->size);
    ASSERT(msg->data != NULL);

    memcpy(msg->data, pkt->data + header_size, msg->size);

    return msg;
}

void SendToUpperLayer(packet *pkt) {
    message *msg = pkt2msg(pkt);
    Receiver_ToUpperLayer(msg);
    /* don't forget to free the space */
    if (msg->data != NULL) free(msg->data);
    free(msg);
}

void InsertIntoBuffer(packet *pkt) {
    int seq = *(unsigned int *) &pkt->data[1];
    int another_seq;
    auto iter = std::find_if(buffer.begin(), buffer.end(), [seq, &another_seq](packet &another) {
        another_seq = *(unsigned int *) &another.data[1];
        return another_seq >= seq;
    });

    if (another_seq == seq) return;

    buffer.insert(iter, *pkt);
}

inline bool PacketNotCorrupted(packet *pkt) {
    return *(unsigned short *) &pkt->data[9] == checksum((unsigned short *) &pkt->data[11], pkt->data[0]);
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */

///TODO: checksum
void Receiver_FromLowerLayer(struct packet *pkt) {
    int seq = *(unsigned int *) &pkt->data[1];
    int checksum = *(unsigned short *) &pkt->data[9];
    int size = pkt->data[0];
    printf("Receive pkt from sender(seq = %d, checksum = %d, size = %d)\n", seq, checksum, size);
    if (!PacketNotCorrupted(pkt)) {
        std::cout << "Checksum not passed!\n";
        return;
    } else {
        std::cout << "Checksum passed\n";
    }

    if (seq == ack) {
        ++ack;
        SendToUpperLayer(pkt);
    } else if (seq > ack) {
        InsertIntoBuffer(pkt);
    }

    while (!buffer.empty()) {
        packet &front = buffer.front();
        int front_seq = *(unsigned int *) &front.data[1];
        if (front_seq == ack) {
            SendToUpperLayer(&front);
            ++ack;
            buffer.pop_front();
        } else break;
    }

    packet sender_pkt;
    sender_pkt.data[0] = 0;
    *(unsigned int *) &sender_pkt.data[1] = 1;
    *(unsigned int *) &sender_pkt.data[5] = ack;

    Receiver_ToLowerLayer(&sender_pkt);
}
