#include "dict.h"

struct dict_file* dict_open(char* path) {
    struct dict_file* dict_ptr = malloc(sizeof(struct dict_file));
    dict_ptr -> fp = fopen(path, "rb");
    dict_ptr -> byte = 0;
    dict_ptr -> left = 0;
    return dict_ptr;
}

int dict_close(struct dict_file* dict_ptr) {
    int ret;
    ret = fclose(dict_ptr -> fp);
    free(dict_ptr);
    return ret;
}

int byte_get_next_bit(struct dict_file* dict_ptr) {
    int cur_byte;
    if (dict_ptr -> left == 0) {
        cur_byte = fgetc(dict_ptr -> fp);
        if (cur_byte < 0) {
            return EOF;
        }
        dict_ptr -> byte = cur_byte;
        dict_ptr -> left = 8;
    }
    dict_ptr -> left -= 1;
    int cur_bit = Get_K_Bit(dict_ptr -> byte, dict_ptr -> left);
    return cur_bit;
}

unsigned get_n_bits(struct dict_file* dict_ptr, unsigned bit_count) {
    unsigned val = 0 ;
    while(bit_count--) {
        int cur_bit;
        cur_bit = byte_get_next_bit(dict_ptr);
        if (cur_bit < 0) {
            return EOF;
        }
        val <<= 1;
        val |= cur_bit;
    }
    return val;
}

void read_dict_file(struct bit_array* dict) {
    struct dict_file* dict_ptr;
    int cur_bit;
    unsigned iseg;
    unsigned ibit;
    unsigned n_bits;
    /* we are guaranteed the path will exist */
    dict_ptr = dict_open(DICT_PATH);
    for(iseg = 0; iseg < DICT_SEGMENTS; iseg++) {
        /* get the length of the dictionary segment in bits */
        n_bits = get_n_bits(dict_ptr, 8);
        dict[iseg].size = n_bits;
        for(ibit = 0; ibit < n_bits; ibit++) {
            cur_bit = byte_get_next_bit(dict_ptr);
            if (cur_bit < 0) {
                break;
            }
            dict[iseg].bit_values[ibit].bit = (uint8_t) cur_bit;
        }
    }
    dict_close(dict_ptr);
}

struct node* new_node(int value) {
    struct node* node = malloc(sizeof(struct node));
    node -> value = value;
    node -> right = NULL;
    node -> left = NULL;
    return node;
}

struct node* construct_tree(struct bit_array* dict) {
    struct node* root = new_node(-10);
    struct node* cursor = root;
    for(int iseg = 0; iseg < DICT_SEGMENTS; iseg++) {
        /* for every segment create a path in the tree with its binary
            code starting from the root */
        cursor = root;
        for(int ibit = 0; ibit < dict[iseg].size; ibit++) {
            if(dict[iseg].bit_values[ibit].bit == 0) {
                /* if current bit is 0 -> traverse the right subtree (ie take the right edge) */
                if(cursor -> right == NULL) {
                    struct node* new = new_node(-10);
                    cursor -> right = new;
                }
                cursor = cursor -> right;
            }
            else {
                /* go left */
                if(cursor -> left == NULL) {
                    struct node* new = new_node(-10);
                    cursor -> left = new;
                }
                cursor = cursor -> left;
            }
        }
        /* assign the value to leaf */
        cursor -> value = iseg;
    }
    return root;
}

uint8_t* compress(struct bit_array* dictionary, uint64_t* payload_len,
                    uint8_t* payload) {

    struct dyn_bit_array* cmprsd_payload_bits = malloc(sizeof(struct dyn_bit_array));
    cmprsd_payload_bits -> array = malloc(sizeof(uint8_t) * MAX_CAP);
    cmprsd_payload_bits -> size = 0;
    cmprsd_payload_bits -> capacity = MAX_CAP;

    for(int byte = 0; byte < *payload_len; byte++) {
        uint8_t current_byte = payload[byte];
        for(int ibit = 0; ibit < dictionary[current_byte].size; ibit++) {
            add_bit(cmprsd_payload_bits, dictionary[current_byte].bit_values[ibit].bit);
        }
    }
    uint8_t n_paddings = get_n_padding(cmprsd_payload_bits -> size);
    /* add the padding 0-bits required to make the payload aligned to the
        next byte boundary */
    int i = 0;
    while(i < n_paddings) {
        add_bit(cmprsd_payload_bits, 0);
        i++;
    }
    /* compressed payload length is the total number of bits encoded/8 (number of bytes)
        + one byte for the number of padding bits added  */
    uint64_t c_pl_len = (cmprsd_payload_bits -> size / 8);
    c_pl_len++;

    /* now send back the compressed payload and the padding byte */
    int length = cmprsd_payload_bits -> size;
    uint8_t* aux = malloc(sizeof(uint8_t) * length);
    /* reverse endianness to get the correct values */
    for(int i = 0; i < length; i++) {
        aux[length - 1 - i] = cmprsd_payload_bits -> array[i];
    }

    /* get the hex representation of each byte and send it to client */
    uint64_t size = c_pl_len;
    uint8_t* bytes_send = malloc(sizeof(uint8_t) * size);
    int j;
    size--;
    for(int i = 0; i < length; i = j){
        uint8_t a = 0;
        for(j = i; j < (i + 8); ++j){
            a |= aux[j] << (j-i);
        }
        bytes_send[size - 1] = a;
        size--;
    }

    bytes_send[c_pl_len - 1] = (uint8_t) n_paddings;
    *payload_len = c_pl_len;

    free(cmprsd_payload_bits -> array);
    free(cmprsd_payload_bits);
    free(aux);
    return bytes_send;
}

uint8_t* decompress(struct node* root, uint8_t* compressed_payload,
                uint64_t* cmprsd_pl_length) {

    struct node* cursor = root;

    /* get the number of bits to read
     * ((total payload length * 8) - number of padding bits)
     */
    uint64_t cmprsd_bytes = *cmprsd_pl_length - 1;
    int total_bits = (cmprsd_bytes * 8) - compressed_payload[cmprsd_bytes];
    /*
       get the first byte of the payload,
       increment the payload pointer 9to point to the next byte
    */
    unsigned char byte = *compressed_payload++;
    unsigned char left = 8;
    int decompressed_bytes = 0;
    uint8_t* decompressed_payload = NULL;
    for(int i = 0; i < total_bits; i++) {
        if(left == 0) {
            /*
               if all bits in current byte have been traversed,
               get the next byte in compressed payload
            */
            byte = *compressed_payload++;
            left = 8;
        }
        left--;
        unsigned char current_bit = Get_K_Bit(byte, left);
        if(current_bit) {
            /* if bit = 1: go left */
            cursor = cursor -> left;
        }
        else {
            /* if bit = 0: go right */
            cursor = cursor -> right;
        }
        /* if we reached a leaf node => we found our decompressed byte */
        if(cursor -> left == NULL && cursor -> right == NULL) {
            decompressed_payload = realloc(decompressed_payload, sizeof(uint8_t) * decompressed_bytes + 1);
            /* this cast should be okay because we only have values are from 0-255 either way */
            decompressed_payload[decompressed_bytes] = (uint8_t) cursor -> value;
            decompressed_bytes++;
            cursor = root;
        }
    }
    *cmprsd_pl_length = (uint64_t) decompressed_bytes;
    return decompressed_payload;
}


void free_tree(struct node* root) {
    /* free every node */
    if(root == NULL) {
        return;
    }
    free_tree(root -> left);
    free_tree(root -> right);
    free(root);
}
