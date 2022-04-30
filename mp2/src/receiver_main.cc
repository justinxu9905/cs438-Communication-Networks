/* 
 * File:   receiver_main.c
 * Author: 
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <queue>


#define MSS 2000  // maximum segment size
#define RECV_BUFFER_SIZE 300
#define MAX_SEQ_NUMBER 1000000
#define DATA 0
#define SYN 1
#define SYN_ACK 2
#define ACK 3
#define FIN 4
#define FIN_ACK 5

typedef struct{
    int 	data_size;
    int 	seq;
    int     ack;
    int     msg_type;
    char    data[MSS];
}packet;

// socket
struct sockaddr_in si_me, si_other;
int slen, sk;
int num_bytes;

// rdt
unsigned long long int seq_number, next_seq, next_ack_idx; // next seq expected to receive, next expected packet index to ack
packet recv_buffer[RECV_BUFFER_SIZE];
bool pending_ack[RECV_BUFFER_SIZE];

// file
FILE *fp;

void diep(char *s) {
    perror(s);
    exit(1);
}


void connectionHandler() {
    char buffer[sizeof(packet)];

    packet syn;
    if ((num_bytes = recvfrom(sk, buffer, sizeof(packet), 0, (struct sockaddr*)&si_other, (socklen_t *)&slen)) == -1) {
        exit(1);
    }
    memcpy(&syn, buffer, sizeof(buffer));
    if (syn.msg_type != SYN) {
        exit(1);
    }
    fprintf(stdout, "recv syn\n");

    packet syn_ack;
    syn_ack.msg_type = SYN_ACK;
    syn_ack.seq = rand();
    syn_ack.ack = syn.seq + 1;
    memcpy(buffer, &syn_ack, sizeof(packet));
    if ((num_bytes = sendto(sk, buffer, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))) == -1) {
        exit(1);
    }
    fprintf(stdout, "send syn_ack\n");

    packet ack;
    if ((num_bytes = recvfrom(sk, buffer, sizeof(packet), 0, (struct sockaddr*)&si_other, (socklen_t *)&slen)) == -1) {
        exit(1);
    }
    memcpy(&ack, buffer, sizeof(buffer));
    if (ack.msg_type != ACK || ack.ack != syn_ack.seq + 1) {
        exit(1);
    }
    fprintf(stdout, "recev ack\n");
}


void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    /* Open the file */
    fp = fopen(destinationFile, "ab");
    if (fp == NULL) {
        printf("Could not open file to write.");
        exit(1);
    }

    /* Create UDP socket */
    slen = sizeof(si_other);


    if ((sk = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(sk, (struct sockaddr*) &si_me, sizeof(si_me)) == -1)
        diep("bind");

    /* Handle connection request */
    connectionHandler();

    /* Now receive data and send acknowledgements */
    char pkt_to_send[sizeof(packet)];
    char pkt_to_recv[sizeof(packet)];
    while (true) {
        if ((num_bytes = recvfrom(sk, pkt_to_recv, sizeof(packet), 0, (struct sockaddr*)&si_other, (socklen_t *)&slen)) == -1) {
            exit(1);
        }
        packet pkt, ack;
        memcpy(&pkt, pkt_to_recv, sizeof(pkt_to_recv));
        if (pkt.msg_type == DATA) {
            fprintf(stdout, "recv seq# %d\n", pkt.seq);
            if (pkt.seq == next_seq) {
                recv_buffer[next_ack_idx] = pkt;
                pending_ack[next_ack_idx] = true;

                while (pending_ack[next_ack_idx]) {
                    fwrite(recv_buffer[next_ack_idx].data, sizeof(char), recv_buffer[next_ack_idx].data_size, fp);
                    pending_ack[next_ack_idx] = false;
                    next_ack_idx = (next_ack_idx + 1) % RECV_BUFFER_SIZE;
                    next_seq += 1;
                }
            } else if (pkt.seq > next_seq) {
                recv_buffer[(next_ack_idx + pkt.seq - next_seq) % RECV_BUFFER_SIZE] = pkt;
                pending_ack[(next_ack_idx + pkt.seq - next_seq) % RECV_BUFFER_SIZE] = true;
            }
            ack.msg_type = ACK;
            ack.seq = seq_number;
            ack.ack = next_seq;
            seq_number = (seq_number + 1) % MAX_SEQ_NUMBER;
            memcpy(pkt_to_send, &ack, sizeof(packet));
            if ((num_bytes = sendto(sk, pkt_to_send, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))) == -1) {
                exit(1);
            }
            fprintf(stdout, "send ack# %d\n", ack.ack);
        } else if (pkt.msg_type == FIN) {
            packet fin_ack1, fin2, fin_ack2;
            fprintf(stdout, "recv fin1\n");

            fin_ack1.msg_type = FIN_ACK;
            fin_ack1.seq = rand();
            fin_ack1.ack = pkt.seq + 1;
            memcpy(pkt_to_send, &fin_ack1, sizeof(packet));
            if ((num_bytes = sendto(sk, pkt_to_send, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))) == -1) {
                exit(1);
            }
            fprintf(stdout, "send fin_ack1\n");

            fin2.msg_type = FIN;
            fin2.seq = fin_ack1.seq + 1;
            memcpy(pkt_to_send, &fin2, sizeof(packet));
            if ((num_bytes = sendto(sk, pkt_to_send, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))) == -1) {
                exit(1);
            }
            fprintf(stdout, "send fin2\n");

            if ((num_bytes = recvfrom(sk, pkt_to_recv, sizeof(packet), 0, (struct sockaddr*)&si_other, (socklen_t *)&slen)) == -1) {
                exit(1);
            }
            memcpy(&fin_ack2, pkt_to_recv, sizeof(pkt_to_recv));
            if (fin_ack2.msg_type != FIN_ACK || fin_ack2.ack != fin2.seq + 1) {
                exit(1);
            }
            fprintf(stdout, "recv fin_ack2\n");
            break;
        }
    }


    /* Close connection */
    close(sk);
    printf("%s received.", destinationFile);
    return;
}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}
