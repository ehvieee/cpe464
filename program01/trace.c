#include <pcap/pcap.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "checksum.h"

// function declarations
void ethernet(const uint8_t *packet);
void arp(const uint8_t *arpbuf);
void ip(const uint8_t *ipbuf);
void icmp(const uint8_t *icmpbuf);
void tcp(const uint8_t *tcpbuf, uint16_t TCP_len, uint8_t *pseudobuf);
void pseudo(uint8_t *pseudobuf, const uint8_t *ipbuf, uint16_t TCP_len);
void tcp_chksum(uint8_t *pseudobuf, const uint8_t *tcpbuf, uint16_t TCP_len);
void tcp_flags(uint8_t flag_byte);
void udp(const uint8_t *udpbuf);

// main function
int main (int argc, char *argv[]) {
    // pcap error buffer
    char errbuf[PCAP_ERRBUF_SIZE];
    // pcap header before packet
    struct pcap_pkthdr *pcap_header;
    // actual packet
    const uint8_t *packet;
    // packet count
    uint32_t packet_n = 1;

    // take only a single argument
    if (argc != 2) {
        fprintf(stderr, "Enter a single PCAP file to be read.\n");
        return 1;
    }

    // open pcap file for reading
    pcap_t *pcap_file = pcap_open_offline(argv[1], errbuf);
    
    // determine if pcap file is valid
    if (pcap_file == NULL) {
        fprintf(stderr, "Error opening file.\n");
        return 1;
    }

    // parse each packet in pcap file
    while (pcap_next_ex(pcap_file, &pcap_header, &packet) == 1) {
        // get packet number and len
        printf("\n");
        printf("Packet number: %d  Packet Len: %d\n\n", packet_n, (uint32_t) pcap_header->len);

        // process packet
        ethernet(packet);

        // increase packet count
        packet_n++;
    }

    pcap_close(pcap_file);
    return 0;
}

// function for processing ethernet header
void ethernet(const uint8_t *ethbuf) {
    char *destmac;
    char *srcmac;
    char *type;
    uint16_t type_code;

    // start ethernet header section
    printf("\tEthernet Header\n");

    // format and print destination mac
    destmac = ether_ntoa((const struct ether_addr*) &ethbuf[0]);
    printf("\t\tDest MAC: %s\n", destmac);

    // format and print source mac
    srcmac = ether_ntoa((const struct ether_addr*) &ethbuf[6]);
    printf("\t\tSource MAC: %s\n", srcmac);

    // get both bytes of type in big endian
    type_code = ntohs(*(uint16_t *) &ethbuf[12]);
    // determine and print type
    switch (type_code) {
        case (0x0800):
            type = "IP";
            break;
        
        case (0x0806):
            type = "ARP";
            break;

        default:
            type = "Unknown";
    }
    printf("\t\tType: %s\n\n", type);

    // process next header depending on type
    switch (type_code) {
        case (0x0800):
            ip(ethbuf + 14);
            break;
        
        case (0x0806):
            arp(ethbuf + 14);
            break;

        default:
            return;
    }
}

// function for processing arp header
void arp(const uint8_t *arpbuf) {
    char *sendmac;
    struct in_addr sendip_struct;
    char *sendip;
    char *targmac;
    struct in_addr targip_struct;
    char *targip;
    char *opcode;
    uint16_t opcode_code;

    printf("\tARP header\n");

    // get both bytes of opcode in big endian
    opcode_code = ntohs(*(uint16_t *) &arpbuf[6]);
    // determine and print opcode
    switch (opcode_code) {
        case (0x0001):
            opcode = "Request";
            break;
        
        case (0x0002):
            opcode = "Reply";
            break;

        default:
            opcode = "Unknown";
    }
    printf("\t\tOpcode: %s\n", opcode);

    // format and print sender mac
    sendmac = ether_ntoa((const struct ether_addr*) &arpbuf[8]);
    printf("\t\tSender MAC: %s\n", sendmac);
    
    // format and print sender ip
    memcpy(&sendip_struct, &arpbuf[14], 4);
    sendip = inet_ntoa(sendip_struct);
    printf("\t\tSender IP: %s\n", sendip);

    // format and print target mac
    targmac = ether_ntoa((const struct ether_addr*) &arpbuf[18]);
    printf("\t\tTarget MAC: %s\n", targmac);

    // format and print target ip
    memcpy(&targip_struct, &arpbuf[24], 4);
    targip = inet_ntoa(targip_struct);
    printf("\t\tTarget IP: %s\n", targip);
}

// function for processing IP headers / packets
void ip(const uint8_t *ipbuf) {
    uint16_t PDU_len;
    uint8_t IHL;
    uint8_t TTL;
    uint8_t protocol_code;
    char *protocol;
    char *checksum;
    struct in_addr sendip_struct;
    char *sendip;
    struct in_addr destip_struct;
    char *destip;

    printf("\tIP Header\n");

    // get and print IP PDU Len
    PDU_len = ntohs(*(uint16_t *) &ipbuf[2]);
    printf("\t\tIP PDU Len: %d\n", PDU_len);

    // get and print IHL (lower nibble of first byte gives IHL in words)
    IHL = (ipbuf[0] & 0x0F) * 4;
    printf("\t\tHeader Len (bytes): %d\n", IHL);

    // get and print TTL
    TTL = (ipbuf[8]);
    printf("\t\tTTL: %d\n", TTL);

    // determine and print protocol
    protocol_code = (ipbuf[9]);
    switch(protocol_code) {
        case 1:
            protocol = "ICMP";
            break;

        case 6:
            protocol = "TCP";
            break;

        case 17:
            protocol = "UDP";
            break;

        default:
            protocol = "Unknown";
    }
    printf("\t\tProtocol: %s\n", protocol);

    // checksum takes IP header and IHL, returns != 0 when error detected
    if (in_cksum((unsigned short*) (&ipbuf[0]), IHL) == 0) {
        checksum = "Correct";
    }
    else {
        checksum = "Incorrect";
    }
    printf("\t\tChecksum: %s (0x%02x%02x)\n", checksum, ipbuf[10], ipbuf[11]);

    // format and print sender ip
    memcpy(&sendip_struct, &ipbuf[12], 4);
    sendip = inet_ntoa(sendip_struct);
    printf("\t\tSender IP: %s\n", sendip);

    // format and print dest ip
    memcpy(&destip_struct, &ipbuf[16], 4);
    destip = inet_ntoa(destip_struct);
    printf("\t\tDest IP: %s\n", destip);

    // process protocols depending on code
    uint8_t pseudobuf[(PDU_len - IHL) + 12];
    switch (protocol_code) {
        // ICMP
        case (1):
            // pass icmpbuf (address of ipbuf + IHL) to icmp function
            icmp(ipbuf + IHL);
            break;

        // TCP
        case (6):
            // create / format pseudo header into pseudobuf
            pseudo(pseudobuf, ipbuf, PDU_len - IHL);
            // pass tcpbuf (address of ipbuf + IHL), TCP_len (PDU_len - IHL), and pseudobuf to tcp function
            tcp(ipbuf + IHL, PDU_len - IHL, pseudobuf);
            break;

        // UDP
        case (17):
            // pass udpbuf (address of ipbuf + IHL) to udp function
            udp(ipbuf + IHL);
            break;

        default:
            return;
    }
}

// function for processing ICMP packets
void icmp(const uint8_t *icmpbuf) {
    uint8_t type_code;

    printf("\n\tICMP Header\n");

    // get type from first byte of ICMP packet and print type
    type_code = icmpbuf[0];
    switch (type_code) {
        case (0x08):
            printf("\t\tType: Request\n");
            break;

        case (0x00):
            printf("\t\tType: Reply\n");
            break;

        // bad checksum case (Type: 109)
        default:
            printf("\t\tType: 109\n");
    }
}

// function for processing TCP packets
void tcp(const uint8_t *tcpbuf, uint16_t TCP_len, uint8_t *pseudobuf) {
    uint16_t srcport;
    uint16_t destport;
    uint32_t seqnum;
    uint32_t acknum;
    uint8_t offset;
    uint8_t flag_byte;
    uint16_t winsize;

    printf("\n\tTCP Header\n");
    
    // print segment length (calculated with IP PDU len - IHL)
    printf("\t\tSegment Length: %d\n", TCP_len);

    // format and print source port
    srcport = ntohs(*(uint16_t *) &tcpbuf[0]);
    if (srcport == 80) {
        printf("\t\tSource Port:  HTTP\n");
    }
    else {
        printf("\t\tSource Port:  %d\n", srcport);
    }

    // format and print dest port
    destport = ntohs(*(uint16_t *) &tcpbuf[2]);
    if (destport == 80) {
        printf("\t\tDest Port:  HTTP\n");
    }
    else {
        printf("\t\tDest Port:  %d\n", destport);
    }

    // format and print sequence number
    seqnum = ntohl(*(uint32_t *) &tcpbuf[4]);
    printf("\t\tSequence Number: %u\n", seqnum);

    // format and print ack number
    acknum = ntohl(*(uint32_t *) &tcpbuf[8]);
    printf("\t\tACK Number: %u\n", acknum);

    // format and print data offset in bytes (only shifting case)
    offset = (tcpbuf[12] >> 4) * 4;
    printf("\t\tData Offset (bytes): %d\n", offset);

    // process flags
    flag_byte = tcpbuf[13];
    tcp_flags(flag_byte);

    // format and print window size
    winsize = ntohs(*(uint16_t *) &tcpbuf[14]);
    printf("\t\tWindow Size: %u\n", winsize);

    // checksum function
    tcp_chksum(pseudobuf, tcpbuf, TCP_len);
}

// modifies pseudobuf input to create pseudo header
void pseudo(uint8_t *pseudobuf, const uint8_t *ipbuf, uint16_t TCP_len) {
    uint16_t len_hton;

    // copy srcip
    memcpy(&pseudobuf[0], &ipbuf[12], 4);

    // copy destip
    memcpy(&pseudobuf[4], &ipbuf[16], 4);

    // set reserved byte to 0
    pseudobuf[8] = 0;

    // copy protocol byte
    memcpy(&pseudobuf[9], &ipbuf[9], 1);

    // copy TCP len in network order
    len_hton = htons(TCP_len);
    memcpy(&pseudobuf[10], &len_hton, 2);
}

// function for TCP checksum process
void tcp_chksum(uint8_t *pseudobuf, const uint8_t *tcpbuf, uint16_t TCP_len) {
    char *checksum;
    // create buffer with size TCP_len + 12 bytes (pseudo header)
    uint8_t buf[TCP_len + 12];

    // copy pseudobuf to buf
    memcpy(&buf[0], &pseudobuf[0], 12);

    // copy TCP header to buf after pseudo header
    memcpy(&buf[12], &tcpbuf[0], TCP_len);

    // printf checksum status and value
    if (in_cksum((unsigned short*) (&buf[0]), TCP_len + 12) == 0) {
        checksum = "Correct";
    }
    else {
        checksum = "Incorrect";
    }
    printf("\t\tChecksum: %s (0x%02x%02x)\n", checksum, tcpbuf[16], tcpbuf[17]);
}

// function for processing TCP flags
void tcp_flags(uint8_t flag_byte) {
    char *syn;
    char *rst;
    char *fin;
    char *ack;
    uint8_t syn_bit;
    uint8_t rst_bit;
    uint8_t fin_bit;
    uint8_t ack_bit;

    // grab respective flag bits from single byte
    syn_bit = flag_byte & 0x02;
    rst_bit = flag_byte & 0x04;
    fin_bit = flag_byte & 0x01;
    ack_bit = flag_byte & 0x10;

    // output yes or no depending on if bit is high or low
    if (syn_bit) {
        syn = "Yes";
    }
    else {
        syn = "No";
    }

    if (rst_bit) {
        rst = "Yes";
    }
    else {
        rst = "No";
    }

    if (fin_bit) {
        fin = "Yes";
    }
    else {
        fin = "No";
    }

    if (ack_bit) {
        ack = "Yes";
    }
    else {
        ack = "No";
    }

    printf("\t\tSYN Flag: %s\n", syn);
    printf("\t\tRST Flag: %s\n", rst);
    printf("\t\tFIN Flag: %s\n", fin);
    printf("\t\tACK Flag: %s\n", ack);
}

// function for processing UDP packets
void udp(const uint8_t *udpbuf) {
    uint16_t srcport;
    uint16_t destport;

    printf("\n\tUDP Header\n");

    // get source port in host order and print
    srcport = ntohs(*(uint16_t *) &udpbuf[0]);
    if (srcport == 53) {
        printf("\t\tSource Port:  DNS\n");
    }
    else {
        printf("\t\tSource Port:  %u\n", srcport);
    }

    // get destination port in host order and print
    destport = ntohs(*(uint16_t *) &udpbuf[2]);
    if (destport == 53) {
        printf("\t\tDest Port:  DNS\n");
    }
    else {
        printf("\t\tDest Port:  %u\n", destport);
    }
}