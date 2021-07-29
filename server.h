#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <dirent.h>
#include <limits.h>
#include <sys/epoll.h>
#include "dict.h"

/* queue up to 300 connections before starting to reject others */
#define BACKLOG (300)

/* payload compression bit indicator */
#define PL_C_BIT (3)

/* server response compression bit */
#define SR_C_BIT (2)

/* buffer size 9: one byte for the message header and 8 bytes for payload length */
#define BFR_SIZE (9)
#define PL_LEN (8)
#define SESSION_ID_SIZE (4)

/* max epoll events */
#define MAX_EVENTS (64)

/* requests types */
#define ERR_BYTE (0xf0)

#define ECHO_RQST (0x0)
#define ECHO_RSPND (0x10)

#define DIR_LIST_RQST (0x2)
#define DIR_LIST_RSPND (0x30)

#define F_SIZE_RQST (0x4)
#define F_SIZE_RSPND (0x50)
#define F_SIZE_BYTES (8)

#define RTRV_F_RQST (0x6)
#define RTRV_F_RSPND (0x70)

#define SHUTDWN_CMMND (0x8)

/*
Get_K_Bit will extract a single bit at position k from a given number n
k starts from index = 0 from the least significant bit
*/
#define Get_K_Bit(n, k)   ( (n & ( 1 << k )) >> k )

/* will set the kth bit in a number n to 1 */
#define Set_K_Bit(n, k)   ( (1 << k) | n)

/* this struct keeps all the necessary file descriptors in one place */
struct data {
    int client_fd;
    int server_fd;
    int epoll_fd;
};

struct rtrv_f_rqst {
    uint32_t session_id;
    uint64_t offset;
    uint64_t data_length;
    char* file_name;
    bool active;
};

struct rtrv_f_rqst_list {
    struct rtrv_f_rqst* list;
    int size;
};

/* handles client requests */
void request_handler(struct bit_array* dictionary, struct node* tree,
                        char* dir_path, struct data* fds, struct rtrv_f_rqst_list* rtrv_list);


#endif
