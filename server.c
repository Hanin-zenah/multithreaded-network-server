#include "server.h"

void add_bit(struct dyn_bit_array* arr, uint8_t bit) {
    if(arr -> size >= arr -> capacity) {
        arr -> capacity += 32;
        arr -> array = realloc(arr -> array, sizeof(uint8_t) * arr -> capacity);
    }
    arr -> array[arr -> size] = (uint8_t) bit;
    arr -> size += 1;
}

uint8_t get_n_padding(int n_bits) {
    uint8_t n_padding = n_bits;
    while (n_padding > 8) {
        if(n_padding % 8 == 0) {
            return 0;
        }
        n_padding -= 8;
    }
    if (n_padding <= 8) {
        n_padding = 8 - n_padding;
    }
    return n_padding;

}

void swap_endianness(uint8_t* array, int length) {
    int end = length - 1;
    for (int i = 0; i < length/2; i++) {
        uint8_t temp = array[i];
        array[i]   = array[end];
        array[end] = temp;
        end--;
    }
}

void request_handler(struct bit_array* dictionary, struct node* tree, char* dir_path,
                            struct data* fds, struct rtrv_f_rqst_list* rtrv_list) {
    uint8_t buffer[BFR_SIZE];
    uint8_t error_buffer[BFR_SIZE];
    memset(buffer, 0x00, BFR_SIZE);
    memset(error_buffer, 0x00, BFR_SIZE);
    error_buffer[0] = ERR_BYTE;
    /* receive the message header and the payload length info from client */
    if(recv(fds -> client_fd, buffer, sizeof(buffer), 0) <= 0) {
        /* close connection */
        close(fds -> client_fd);
        return;
    }
    /* first four bits of the message header is the request_type */
    unsigned char type_field = buffer[0] >> 4;

    /* get the compression indicators */
    unsigned char compressed = Get_K_Bit(buffer[0], PL_C_BIT);
    unsigned char r_compression = Get_K_Bit(buffer[0], SR_C_BIT);

    /* get the payload length */
    uint8_t payload_len[PL_LEN];
    memset(payload_len, 0, PL_LEN);

    /* swap endianness */
    for(int i = 1; i < BFR_SIZE; i++) {
        payload_len[PL_LEN-i] = buffer[i];
    }

    /* get decimal representation */
    uint64_t pl_len = *((uint64_t*) payload_len);

    if(type_field == ECHO_RQST) {
        buffer[0] = ECHO_RSPND;
        uint8_t* payload = malloc(sizeof(uint8_t)* pl_len);
        recv(fds -> client_fd, payload, pl_len, 0);

        if(compressed) {
            /* set compression bit and send back same payload length and payload */
            buffer[0] = Set_K_Bit(buffer[0], PL_C_BIT);
            send(fds -> client_fd, buffer, sizeof(buffer), 0);
            send(fds -> client_fd, payload, pl_len, 0);
        }
        else if(!compressed && r_compression) {
            buffer[0] = Set_K_Bit(buffer[0], PL_C_BIT);
            /* compress payload */
            uint8_t* bytes_send = compress(dictionary, &pl_len, payload);

            /* get the hexadecimal representation of the compressed payload length
                and copy it back to the buffer to send it to client */
            uint8_t cmprs_payload_len[PL_LEN];
            memset(cmprs_payload_len, 0, PL_LEN);
            memcpy(cmprs_payload_len, &pl_len, sizeof(pl_len));
            /* swap endianness */
            for(int i = 0; i < PL_LEN; i++) {
                buffer[PL_LEN - i] = cmprs_payload_len[i];
            }
            send(fds -> client_fd, buffer, sizeof(buffer), 0);

            /* now send back the compressed payload and the padding byte */
            send(fds -> client_fd, bytes_send, pl_len, 0);
            free(bytes_send);
        }
        else {
            /* send back buffer, payload length and payload as is */
            send(fds -> client_fd, buffer, sizeof(buffer), 0);
            send(fds -> client_fd, payload, pl_len, 0);
        }
        free(payload);
    }
    else if(type_field == DIR_LIST_RQST) {
        buffer[0] = DIR_LIST_RSPND;
        DIR* target = NULL;
        struct dirent *dit;
        /* open the target directory */
        if((target = opendir(dir_path)) == NULL) {
            perror("Target directory open failed");
            free(fds);
            free(dir_path);
            free_tree(tree);
            for(int i = 0; i < rtrv_list -> size; i++) {
                free(rtrv_list -> list[i].file_name);
            }
            free(rtrv_list -> list);
            free(rtrv_list);
            exit(1);
        }
        uint64_t payload_length = 0;
        uint8_t* payload = NULL;
        while((dit = readdir(target)) != NULL) {
            /*
               traverse directory for regular files
               get the number of bytes for each regular file name + null character,
               realloc payload accordingly
            */
            if(dit -> d_type == DT_REG) {
                payload = realloc(payload, sizeof(uint8_t) * (strlen(dit -> d_name) + 1 + payload_length));
                /* add names of files to payload byte by byte */
                for(int ch = 0; ch <= strlen(dit -> d_name); ch++) {
                    payload[payload_length] = dit -> d_name[ch];
                    payload_length += 1;
                }
            }
        }
        closedir(target);

        /* if no regular files were found return a null byte payload */
        if(payload_length == 0) {
            send(fds -> client_fd, buffer, sizeof(buffer), 0);
            uint8_t null_payload[1] = {0x00};
            send(fds -> client_fd, null_payload, 1, 0);
            return;
        }
        if(r_compression) {
            buffer[0] = Set_K_Bit(buffer[0], PL_C_BIT);
            uint8_t* bytes_send = compress(dictionary, &payload_length, payload);

            /* get the hexadecimal representation of the compressed payload length
                and copy it back to the buffer to send it to client */
            uint8_t cmprs_payload_len[PL_LEN];
            memset(cmprs_payload_len, 0, PL_LEN);
            memcpy(cmprs_payload_len, &payload_length, sizeof(payload_length));
            /* swap endianness */
            for(int i = 0; i < PL_LEN; i++) {
                buffer[PL_LEN - i] = cmprs_payload_len[i];
            }
            send(fds -> client_fd, buffer, sizeof(buffer), 0);

            /* now send back the compressed payload and the padding byte */
            send(fds -> client_fd, bytes_send, payload_length, 0);
            free(bytes_send);
        }
        else {
            uint8_t payload_len[PL_LEN];
            memset(payload_len, 0, PL_LEN);
            memcpy(payload_len, &payload_length, sizeof(payload_length));
            /* swap endiannes */
            for(int i = 0; i < PL_LEN; i++) {
                buffer[PL_LEN - i] = payload_len[i];
            }
            /* send back the appropriate payload length and payload */
            send(fds -> client_fd, buffer, sizeof(buffer), 0);
            send(fds -> client_fd, payload, payload_length, 0);
        }
    }
    else if(type_field == F_SIZE_RQST) {
        buffer[0] = F_SIZE_RSPND;
        /* get the file name */
        char file_name[PATH_MAX];
        recv(fds -> client_fd, file_name, pl_len, 0);
        /* concatenate file name with the path of the target directory given
            example: TargetDirectory/this/file/path
        */
        char file_path[PATH_MAX];
        strcpy(file_path, dir_path);
        strcat(file_path, "/");
        strcat(file_path, file_name);

        struct stat stats;
        if (stat(file_path, &stats) != 0) {
            /* error finding file -> return error **as error functionality** */
            send(fds -> client_fd, error_buffer, sizeof(error_buffer), 0);
            /* close the connection */
            close(fds -> client_fd);
            return;
        }
        uint64_t f_size = stats.st_size;
        uint8_t file_size[F_SIZE_BYTES];
        memcpy(file_size, &f_size, F_SIZE_BYTES);
        swap_endianness(file_size, F_SIZE_BYTES);

        if(r_compression) {
            buffer[0] = Set_K_Bit(buffer[0], PL_C_BIT);

            uint64_t payload_length = PL_LEN;
            uint8_t* bytes_send = compress(dictionary, &payload_length, file_size);

            /* get the hexadecimal representation of the compressed payload length
                and copy it back to the buffer to send it to client */
            uint8_t cmprs_payload_len[PL_LEN];
            memset(cmprs_payload_len, 0, PL_LEN);
            memcpy(cmprs_payload_len, &payload_length, sizeof(payload_length));
            /* swap endianness */
            for(int i = 0; i < PL_LEN; i++) {
                buffer[PL_LEN - i] = cmprs_payload_len[i];
            }
            send(fds -> client_fd, buffer, sizeof(buffer), 0);

            /* now send back the compressed payload and the padding byte */
            send(fds -> client_fd, bytes_send, payload_length, 0);
            free(bytes_send);
        }
        else {
            uint8_t payload_len[PL_LEN];
            memset(payload_len, 0, PL_LEN);
            uint64_t length = PL_LEN;
            memcpy(payload_len, &length, sizeof(length));
            /* swap endianness */
            for(int i = 0; i < PL_LEN; i++) {
                buffer[PL_LEN - i] = payload_len[i];
            }
            send(fds -> client_fd, buffer, sizeof(buffer), 0);
            send(fds -> client_fd, file_size, sizeof(file_size), 0);
        }

    }
    else if(type_field == RTRV_F_RQST) {
        buffer[0] = RTRV_F_RSPND;

        uint8_t session_id[SESSION_ID_SIZE];
        uint8_t offset[PL_LEN];
        uint8_t data_length[PL_LEN];

        uint8_t* payload = malloc(sizeof(uint8_t) * pl_len);
        recv(fds -> client_fd, payload, pl_len, 0);

        if(compressed) {
            uint8_t* decompressed_payload = decompress(tree, payload, &pl_len);
            payload = realloc(payload, sizeof(uint8_t) * pl_len);
            memset(payload, 0x00, pl_len);
            memcpy(payload, decompressed_payload, pl_len);
            free(decompressed_payload);
        }
        /* get session id, offset, data_length, target file name */
        memcpy(session_id, payload, SESSION_ID_SIZE);
        memcpy(offset, payload + SESSION_ID_SIZE, PL_LEN);
        memcpy(data_length, payload + SESSION_ID_SIZE + PL_LEN, PL_LEN);

        int target_length = pl_len - SESSION_ID_SIZE - PL_LEN - PL_LEN;
        char* target_file = malloc(target_length);
        memset(target_file, 0, target_length);
        memcpy(target_file, payload + SESSION_ID_SIZE + PL_LEN + PL_LEN, target_length);

        /* get the whole file path */
        char file_path[PATH_MAX];
        strcpy(file_path, dir_path);
        strcat(file_path, "/");
        strcat(file_path, target_file);
        free(target_file);
        swap_endianness(offset, PL_LEN);
        swap_endianness(data_length, PL_LEN);

        uint32_t sess_id_val = *(uint32_t*) session_id;
        uint64_t off_val = *(uint64_t*) offset;
        uint64_t range_val = *(uint64_t*) data_length;

        /* swap endianness of data length and offset back to original */
        swap_endianness(offset, PL_LEN);
        swap_endianness(data_length, PL_LEN);

        /* send file data in one go **change later to multiplexing** */
        FILE* file = fopen(file_path, "rb");
        if(!file) {
            /* error finding file -> return error **as error functionality** */
            send(fds -> client_fd, error_buffer, sizeof(error_buffer), 0);
            /* close the connection */
            close(fds -> client_fd);
            free(payload);
            return;
        }
        int fd = fileno(file);
        struct stat stats;
        fstat(fd, &stats);
        int f_size = stats.st_size;
        if((off_val + range_val) > f_size) {
            /* invalid data range */
            send(fds -> client_fd, error_buffer, sizeof(error_buffer), 0);
            /* close the connection */
            close(fds -> client_fd);
            free(payload);
            return;
        }

        bool found = false;
        int list_size = rtrv_list -> size;
        for(int i = 0; i < list_size; i++) {
            /*
               check if another connection has made the same
               request with the same session id
            */
            if(rtrv_list -> list[i].session_id == sess_id_val) {
                /* check if an ongoing transfer is happening */
                bool active = rtrv_list -> list[i].active;
                if(active) {
                    /* check if it has the same file name and same data range */
                    if(strcmp(rtrv_list -> list[i].file_name, file_path) == 0 &&
                        rtrv_list -> list[i].data_length == range_val) {
                            /* send specified message respond with an empty payload */

                            /* MULTIPLEX HERE */
                            memset(buffer, 0, BFR_SIZE);
                            buffer[0] = RTRV_F_RSPND;
                            send(fds -> client_fd, buffer, sizeof(buffer), 0);
                            free(payload);
                            return;
                    }

                    /* different file name, or same file name with different byte range */
                    else {
                        /* another connection is performing the same work
                            send error response */
                        send(fds -> client_fd, error_buffer, sizeof(error_buffer), 0);
                        close(fds -> client_fd);
                        free(payload);
                        return;
                    }

                }
                /* request is not active, session id is being reused */
                else {
                    if(strcmp(rtrv_list -> list[i].file_name, file_path) == 0 &&
                        rtrv_list -> list[i].data_length == range_val) {
                            send(fds -> client_fd, error_buffer, sizeof(error_buffer), 0);
                            close(fds -> client_fd);
                            free(payload);
                            return;
                    }
                    else {
                        int size = rtrv_list -> size;
                        rtrv_list -> list = realloc(rtrv_list -> list, sizeof(struct rtrv_f_rqst) * (size + 1));
                        rtrv_list -> list[size].session_id = sess_id_val;
                        rtrv_list -> list[size].offset = off_val;
                        rtrv_list -> list[size].data_length = range_val;
                        rtrv_list -> list[size].file_name = strdup(file_path);
                        rtrv_list -> list[size].active = true;

                        rtrv_list -> size += 1;
                    }

                }
                found = true;
                break;
            }
        }
        /* if no other connection has made the same request with the same session id
            or if this is the first ever retrieve file request */
        if(!found) {
            /* add request to the list */
            int size = rtrv_list -> size;
            rtrv_list -> list = realloc(rtrv_list -> list, sizeof(struct rtrv_f_rqst) * (size + 1));
            rtrv_list -> list[size].session_id = sess_id_val;
            rtrv_list -> list[size].offset = off_val;
            rtrv_list -> list[size].data_length = range_val;
            rtrv_list -> list[size].file_name = strdup(file_path);
            rtrv_list -> list[size].active = true;

            rtrv_list -> size += 1;
        }

        /* read file from given offset */
        char* content = malloc(range_val);
        fseek(file, off_val, SEEK_SET);
        fread(content, range_val, 1, file);
        fclose(file);

        /* concatenate content read with the payload */
        int bytes = SESSION_ID_SIZE + (PL_LEN * 2);
        payload = realloc(payload, sizeof(uint8_t) *(bytes + range_val));
        memset(payload + bytes, 0x00, range_val);
        memcpy(payload + bytes, content, range_val);
        pl_len = bytes + range_val;
        free(content);

        if(r_compression) {
            /* compress payload */
            buffer[0] = Set_K_Bit(buffer[0], PL_C_BIT);
            uint8_t* bytes_send = compress(dictionary, &pl_len, payload);

            /* get the hexadecimal representation of the compressed payload length
                and copy it back to the buffer to send it to client */
            uint8_t cmprs_payload_len[PL_LEN];
            memset(cmprs_payload_len, 0, PL_LEN);
            memcpy(cmprs_payload_len, &pl_len, sizeof(pl_len));
            /* swap endianness */
            for(int i = 0; i < PL_LEN; i++) {
                buffer[PL_LEN - i] = cmprs_payload_len[i];
            }
            send(fds -> client_fd, buffer, sizeof(buffer), 0);

            /* now send back the compressed payload and the padding byte */
            send(fds -> client_fd, bytes_send, pl_len, 0);
            rtrv_list -> list[list_size].active = false;
            free(bytes_send);
        }
        else {
            /* send payload as required */
            uint8_t payload_len[PL_LEN];
            memset(payload_len, 0, PL_LEN);
            memcpy(payload_len, &pl_len, sizeof(pl_len));

            /* swap endianness */
            for(int i = 0; i < PL_LEN; i++) {
                buffer[PL_LEN - i] = payload_len[i];
            }
            /* send back the appropriate payload length and payload and update request status */
            send(fds -> client_fd, buffer, sizeof(buffer), 0);
            send(fds -> client_fd, payload, pl_len, 0);
            rtrv_list -> list[list_size].active = false;

        }
        free(payload);
    }
    else if(type_field == SHUTDWN_CMMND) {
        /* shutdown server and exit */
        shutdown(fds -> server_fd, SHUT_RDWR);
        free(fds);
        free(dir_path);
        free_tree(tree);
        for(int i = 0; i < rtrv_list -> size; i++) {
            free(rtrv_list -> list[i].file_name);
        }
        free(rtrv_list -> list);
        free(rtrv_list);
        exit(0);
    }
    else {
        /* Error */
        send(fds -> client_fd, error_buffer, sizeof(error_buffer), 0);
        close(fds -> client_fd);
        return;
    }
};


int main(int argc, char** argv) {
    /* if no configuration file given can't execute program */
    if(argc < 2) {
        return 1;
    }

    /* open and read dictionary file into dictionary array to access it later,
        construct the binary tree of bit codes for decompressing */
    struct bit_array dictionary[DICT_SEGMENTS];
    read_dict_file(dictionary);
    struct node* tree = construct_tree(dictionary);

    /* open and read from config file */
    FILE* config_file;
    if((config_file = fopen(argv[1], "rb")) == NULL) {
        perror("Provided config file is invalid");
        exit(1);
    }

    /* move file cursor to the end of the file and get its size using ftell() */
    fseek(config_file, 0 , SEEK_END);
    long file_size = ftell(config_file);
    if(file_size == -1) {
        perror("ftell error");
        exit(1);
    }
    /* set cursor back to the beginning of the file */
    fseek(config_file, 0 , SEEK_SET);
    uint32_t ip_add = 0;
    uint16_t port = 0;
    int ip_port_length = (sizeof(uint32_t) + sizeof(uint16_t));
    int dir_length = file_size - ip_port_length;
    char* dir_path = malloc(dir_length + 1);

    /* read ip address and port number from file */
    fread(&ip_add, sizeof(ip_add), 1, config_file);
    fread(&port, sizeof(port), 1, config_file);

    /* read directory file path */
    fseek(config_file, ip_port_length, SEEK_SET);
    fread(dir_path, 1, dir_length, config_file);
    fclose(config_file);

    /* NULL terminate the target directory path string */
    dir_path[dir_length] = 0x00;

    /* ready to start connection */
    struct data* fds = malloc(sizeof(struct data));
    int server_fd = -1;
    int client_fd = -1;
    int option = 1;

    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        exit(1);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
	address.sin_addr.s_addr = ip_add;
	address.sin_port = port;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option, sizeof(option));

    if(bind(server_fd, (struct sockaddr*) &address, sizeof(address))) {
		perror("Bind failed");
		exit(1);
	}
    if(listen(server_fd, BACKLOG) < 0) {
        perror("Listen failed");
        exit(1);
    }

    /*
       create a retrieve file request list to monitor if any other connection
       has requested the same file retrieval, session_id, etc
    */
    struct rtrv_f_rqst_list* rtrv_list = malloc(sizeof(struct rtrv_f_rqst_list));
    rtrv_list -> size = 0;
    rtrv_list -> list = NULL;

    /* initialize epoll to monitor multiple file descriptors (clients) */
    int epoll_fd = -1;
    epoll_fd = epoll_create1(0);
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);
    fds -> server_fd = server_fd;
    fds -> epoll_fd = epoll_fd;

    struct epoll_event epoll_events[MAX_EVENTS];
    for(;;) {
        /* block until at least one event occurs, no time limit */
        int n_events = epoll_wait(epoll_fd, epoll_events, MAX_EVENTS, -1);

        /* handle each event */
        for(int i = 0; i < n_events; i++) {
            /* handle errors and client socket shutdown */
            if(epoll_events[i].events == EPOLLERR ||
                epoll_events[i].events == EPOLLHUP ||
                epoll_events[i].events != EPOLLIN) {
                    /* remove that server fd from the monitored list
                        and close the file descriptor */
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, epoll_events[i].data.fd, &event);
                    close(epoll_events[i].data.fd);
                    break;
            }
            /* new incoming connection -> accept client socket fd and
                add it to the monitored epoll list */
            if(server_fd == epoll_events[i].data.fd) {
                uint32_t addrlen = sizeof(address);
                if((client_fd = accept(server_fd, (struct sockaddr*) &address, &addrlen)) < 0 ) {
                    perror("Accept failed");
                    exit(1);
                }
                event.data.fd = client_fd;
                event.events = EPOLLIN;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
            }
            /* handle active connection sockets */
            else {
                fds -> client_fd = epoll_events[i].data.fd;
                /* receive and handle client requests */
                request_handler(dictionary, tree, dir_path, fds, rtrv_list);
            }
        }
    }
    close(server_fd);
    return 0;
}
