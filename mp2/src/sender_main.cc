/* 
 * File:   sender_main.c
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
#include <chrono>

using namespace std;

#define MSS 2000  // maximum segment size
#define SEND_BUFFER_SIZE 300
#define MAX_SEQ_NUMBER 1000000
#define TIMEOUT 1000000
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
    int     msg_type; //0: SYN, 1: ACK, 2: FIN, 3: FINACK
    char    data[MSS];
}packet;

/* socket */
struct sockaddr_in si_other;
struct addrinfo *des_addr;
int slen, sk;
FILE *fp;
unsigned long long int bytes_to_load;
int num_bytes;

// rdt window
unsigned long long int seq_number;
int timeout = TIMEOUT, rtt = TIMEOUT;
queue<packet> send_buffer;
queue<packet> wait_ack;
queue<chrono::steady_clock::time_point> start_time_record;

// Congestion Control
double cwnd = 1.0, ssthresh = 4.0;
int dup_ack_count = 0;
enum congestion_state {SLOW_START, CONGESTION_AVOIDANCE, FAST_RECOVERY};
int cur_state = SLOW_START;
enum congestion_signal {NEW_ACK, TIME_OUT, DUP_ACK};


void establishConnection() {
    char buffer[sizeof(packet)];

    packet syn;
    syn.msg_type = SYN;
    syn.seq = rand();
    memcpy(buffer, &syn, sizeof(syn));
    if ((num_bytes = sendto(sk, buffer, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))) == -1) {
        exit(1);
    }
    fprintf(stdout, "send syn\n");

    packet syn_ack;
    if ((num_bytes = recvfrom(sk, buffer, sizeof(packet), 0, (struct sockaddr*)&si_other, (socklen_t *)&slen)) == -1) {
        exit(1);
    }
    memcpy(&syn_ack, buffer, sizeof(buffer));
    if (syn_ack.msg_type != SYN_ACK || syn_ack.ack != syn.seq + 1) {
        exit(1);
    }
    fprintf(stdout, "recv syn_ack\n");

    packet ack;
    ack.msg_type = ACK;
    ack.seq = syn.seq + 1;
    ack.ack = syn_ack.seq + 1;
    memcpy(buffer, &ack, sizeof(ack));
    if ((num_bytes = sendto(sk, buffer, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))) == -1) {
        exit(1);
    }
    fprintf(stdout, "send ack\n");
}

void closeConnection() {
    char buffer[sizeof(packet)];

    packet fin1;
    fin1.msg_type = FIN;
    fin1.seq = rand();
    memcpy(buffer, &fin1, sizeof(fin1));
    if ((num_bytes = sendto(sk, buffer, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))) == -1) {
        exit(1);
    }
    fprintf(stdout, "send fin1\n");

    packet fin_ack1;
    if ((num_bytes = recvfrom(sk, buffer, sizeof(packet), 0, (struct sockaddr*)&si_other, (socklen_t *)&slen)) == -1) {
        exit(1);
    }
    memcpy(&fin_ack1, buffer, sizeof(buffer));
    if (fin_ack1.msg_type != FIN_ACK || fin_ack1.ack != fin1.seq + 1) {
        exit(1);
    }
    fprintf(stdout, "recv fin_ack1\n");

    packet fin2;
    if ((num_bytes = recvfrom(sk, buffer, sizeof(packet), 0, (struct sockaddr*)&si_other, (socklen_t *)&slen)) == -1) {
        exit(1);
    }
    memcpy(&fin2, buffer, sizeof(buffer));
    if (fin2.msg_type != FIN) {
        exit(1);
    }
    fprintf(stdout, "recv fin2\n");

    packet fin_ack2;
    fin_ack2.msg_type = FIN_ACK;
    fin_ack2.seq = fin1.seq + 1;
    fin_ack2.ack = fin2.seq + 1;
    memcpy(buffer, &fin_ack2, sizeof(fin_ack2));
    if ((num_bytes = sendto(sk, buffer, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))) == -1) {
        exit(1);
    }
    fprintf(stdout, "send fin_ack2\n");
}

void loadSendBuffer(FILE *fp) {
    int bytes_in_pkt;
    char tmp[MSS];
    for (int i = 0; bytes_to_load != 0 && i < SEND_BUFFER_SIZE; ++i) {
        packet pkt;
        bytes_in_pkt = bytes_to_load < MSS ? bytes_to_load : MSS;
        int data_size = fread(tmp, sizeof(char), bytes_in_pkt, fp);
        if (data_size > 0) {
            pkt.data_size = data_size;
            pkt.msg_type = DATA;
            pkt.seq = seq_number;
            memcpy(pkt.data, &tmp,sizeof(char)*bytes_in_pkt);
            send_buffer.push(pkt);
            seq_number = (seq_number + 1) % MAX_SEQ_NUMBER;
        }
        bytes_to_load -= data_size;
    }
}


void sendPackets(int socket) {
    char pkt_to_send[sizeof(packet)];
    int num_pkt = wait_ack.size() + send_buffer.size() <= cwnd ? send_buffer.size() : cwnd - wait_ack.size();
    for (int i = 0; i < num_pkt; i++) {
        memcpy(pkt_to_send, &send_buffer.front(), sizeof(packet));
        if ((num_bytes = sendto(socket, pkt_to_send, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))) == -1) {
            exit(1);
        }
        start_time_record.push(chrono::steady_clock::now());
        fprintf(stdout, "send seq# %d\n", send_buffer.front().seq);
        wait_ack.push(send_buffer.front());
        send_buffer.pop();
    }
    loadSendBuffer(fp);
}


void setTimeout(int socket) {
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeout;

    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt failed\n");
    }
}


void congestionControl(int signal) {
    switch (signal) {
        case NEW_ACK:
            if (cur_state == SLOW_START) {
                cwnd = cwnd * 2 <= ssthresh ? cwnd * 2 : ssthresh;
                dup_ack_count = 0;
                if (cwnd >= ssthresh) {
                    cur_state = CONGESTION_AVOIDANCE;
                }
            } else if (cur_state == CONGESTION_AVOIDANCE) {
                cwnd += 1.0 / cwnd;
                dup_ack_count = 0;
            } else if (cur_state == FAST_RECOVERY) {
                cwnd = ssthresh;
                dup_ack_count = 0;
                cur_state = CONGESTION_AVOIDANCE;
            }
            break;
        case TIME_OUT:
            ssthresh = cwnd / 2.0;
            cwnd = 1;
            dup_ack_count = 0;
            cur_state = SLOW_START;
            break;
        case DUP_ACK:
            if (cur_state == FAST_RECOVERY) {
                cwnd += 1;
            } else {
                dup_ack_count += 1;
                if (dup_ack_count >= 3) {
                    ssthresh = cwnd / 2.0;
                    cwnd += 3;
                    cur_state = FAST_RECOVERY;
                }
            }
            break;
        default:
            break;
    }
    fprintf(stdout, "%s -> state: %s, cwnd: %lf, ssthresh: %lf\n", signal == NEW_ACK ? "new_ack" : (signal == DUP_ACK ? "dup_ack" : "time_out"), cur_state == SLOW_START ? "slow_start" : (cur_state == CONGESTION_AVOIDANCE ? "ca" : "fr"), cwnd, ssthresh);
}


void diep(char *s) {
    perror(s);
    exit(1);
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    /* Open the file */
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    /* Determine how many bytes to transfer */
    bytes_to_load = bytesToTransfer;

    /* Create UDP socket */

    slen = sizeof (si_other);
    if ((sk = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    setTimeout(sk);

    /* Establish TCP connection */
    establishConnection();

    /* Send data and receive acknowledgements on s*/
    loadSendBuffer(fp);
    sendPackets(sk);
    char pkt_to_send[sizeof(packet)], pkt_to_recv[sizeof(packet)];
    chrono::steady_clock::time_point start_time, end_time;
    int sample_rtt, dev_rtt = 0;
    while (!send_buffer.empty() || !wait_ack.empty()) {
        if ((num_bytes = recvfrom(sk, pkt_to_recv, sizeof(packet), 0, (struct sockaddr*)&si_other, (socklen_t *)&slen)) == -1) { // timeout
            fprintf(stdout, "timeout\n");
            if (cwnd >= ssthresh) {
                congestionControl(TIME_OUT);
            }
            memcpy(pkt_to_send, &wait_ack.front(), sizeof(packet));
            if ((num_bytes = sendto(sk, pkt_to_send, sizeof(packet), 0, (struct sockaddr*)&si_other, slen)) == -1) {
                exit(1);
            }
        } else { // ack received
            end_time = chrono::steady_clock::now();
            packet ack;
            memcpy(&ack, pkt_to_recv, sizeof(packet));
            fprintf(stdout, "recv ack# %d\n", ack.ack);
            if (ack.msg_type == ACK) {
                if (ack.ack == wait_ack.front().seq) { // receive duplicate ack
                    congestionControl(DUP_ACK);
                    if (cur_state == FAST_RECOVERY) { // if fast recovery, retransmit
                        memcpy(pkt_to_send, &wait_ack.front(), sizeof(packet));
                        if ((num_bytes = sendto(sk, pkt_to_send, sizeof(packet), 0, (struct sockaddr*)&si_other, slen)) == -1) {
                            exit(1);
                        }
                    }
                } else if (ack.ack > wait_ack.front().seq) { // receive cumulated ack
                    while (!wait_ack.empty() && ack.ack > wait_ack.front().seq) {
                        congestionControl(NEW_ACK);
                        wait_ack.pop();
                        start_time = start_time_record.front();
                        start_time_record.pop();
                    }
                    sample_rtt = chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
                    if (sample_rtt > 5 * rtt) {
                        continue;
                    }
                    rtt = 0.875 * rtt + 0.125 * sample_rtt;
                    //dev_rtt = (3 * dev_rtt + (sample_rtt - rtt >= 0 ? sample_rtt - rtt : rtt - sample_rtt)) / 4;
                    timeout = rtt;
                    fprintf(stdout, "current timeout: %d us\n", timeout);
                    setTimeout(sk);
                }
                sendPackets(sk);
            }
        }
    }

    /* Close TCP connection */
    closeConnection();

    fprintf(stdout, "Closing the socket\n");
    close(sk);
    return;

}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);


    fprintf(stdout, "start rt\n");
    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}

