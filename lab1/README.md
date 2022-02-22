# Lab 1 - Reliable Data Transport Protocol

### Packet Format

The packet format of my reliable data transport protocol is as follow.

![image-20220222192908517](README.assets/image-20220222192908517.png)

The first 1 byte stands for the length of the data; The following 4 bytes is the sequence number; Then the acknowledge number(4 bytes) and the checksum(2 bytes). Finally, the data can occupy 0 ~ 117 bits(because the size of the header is 11 bits while the max size of a packet is 128 bits). The overall format is partly imitating TCP protocol, but is simpler than it. 

Here is table indicating each field of a packet in form of code(Assume we have a packet called pkt).

![image-20220222195104523](README.assets/image-20220222195104523.png)

### Parameters

![image-20220222201351444](README.assets/image-20220222201351444.png)

### Checksum

Checksum is an important mechanism to determine whether a packet has been corrupted. I have adopted the traditional checksum function implemented in TCP protocol. The codes concerned are as follows.

```c++
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
```

The checksum function will return a value whose type is unsigned short.

Notice that to calculate the checksum of a packet, we must set the checksum field of the packet first.

Here is the function that checks whether the packet has been corrupted.

```C++
/**
 * @brief This function is to check whether the packet received has been corrupted, based on checksum.
 * @param pkt packet received from the receiver
 * @return if the packet is not corrupted, return true, else return false.
 */
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
    return pkt_checksum == real_checksum;
}
```

Since checksum is not omnipotent(In some cases it will not be able to detect the error), so the function also checks whether the ack and data length field of the packet are validate.

### Sliding Window with Buffer

In my rdt, I use a sliding window to increase the band width of data transmit. The default size is 8. I've tried to implement ***AIMD(Additive Increase Multiplicative Decrease)*** mechanism in TCP protocol: The initial window size is 2. Then the window size will slow start to 32. After that the window size will addictively increase(increases by 1 each time). Once the sender receives duplicate ack, the window size will be divided by 2. If a timeout occurs,  the window size will be set back to 2, and begins a new slow start. (This mechanism can be started or stopped through a macro called **AIMD**)

This mechanism doesn't make much better performance than the version of a constant window size.

