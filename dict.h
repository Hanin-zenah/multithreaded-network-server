#ifndef DICT_H
#define DICT_H

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

#define BIT_ARRAY_LENGTH (32)
#define DICT_PATH "compression.dict"
#define DICT_SEGMENTS (256)
#define MAX_CAP (1024)

#define Get_K_Bit(n, k)   ( (n & ( 1 << k )) >> k )


/* individual bits */
struct bit {
    uint8_t bit: 1;
};

/*
    bit array to hold the bits for each segment of the dictionary
    size (int) is the number of bits in the bit_values array
*/
struct bit_array {
    struct bit bit_values[BIT_ARRAY_LENGTH];
    int size;
};

/*
    fp (FILE*) is the file pointer to the binary file (dictionary)
    byte (unsigned char) is the current extracted byte from the file
    left (unsigned char) is the number of bits left from the extracted byte
    that haven't been read (dealt with) yet
*/
struct dict_file {
    FILE* fp;
    unsigned char byte;
    unsigned char left;
};


/* this struct will be used for adding individual bits to the payload */
struct dyn_bit_array {
    uint8_t* array;
    int capacity;
    int size;
};


/*
    A node in the binary tree where
    value (uint8_t) is the value of the node
    left (struct node*) is the left child of the node
    right (struct node*) is the right child of the node
*/
struct node {
    int value;
    struct node* left;
    struct node* right;
};

unsigned get_number(struct bit_array array);

struct dict_file* dict_open(char* path);

int dict_close(struct dict_file* dict_ptr);

int byte_get_next_bit(struct dict_file * dict_ptr);

unsigned get_n_bits( struct dict_file *dict_ptr, unsigned bitcount);

void read_dict_file(struct bit_array* dict);

/* creates a new node for the binary tree */
struct node* new_node(int value);

/*
    This function will construct a binary tree that will be traversed when we need to
    decode a compressed payload, starting from the root, every right edge is labelled
    0 and every left edge is 1
*/
struct node* construct_tree(struct bit_array* dict);

/* adds a single bit to bit array */
void add_bit(struct dyn_bit_array* arr, uint8_t bit);

/* returns the number of padding 0-bits needed for the nearest byte boundary
of a given n_bits */
uint8_t get_n_padding(int n_bits);

/* given a dictionary of bit codes, compress a payload and return the compressed payload */
uint8_t* compress(struct bit_array* dictionary, uint64_t* payload_len,
                    uint8_t* payload);

/* given a binary tree, and a compressed payload with its length, decompress the payload */
uint8_t* decompress(struct node* tree, uint8_t* compressed_payload,
                uint64_t* cmprsd_pl_length);


/* frees every single node in the constructed tree */
void free_tree(struct node* root);


#endif
