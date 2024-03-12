#include "lib.h"

// Max value of a byte
#define ONE_BYTE_SIZE 255
// Maximum prefix length in bits
#define MAX_PREFIX_LEN 4
 
// Trie node
struct TrieNode {
    void *entry;
    struct TrieNode *children[ONE_BYTE_SIZE];
    // isLeaf is true if the node represents end of a prefix
    int isLeaf;
};

/*
 * funtion to create a new trie node
 */
struct TrieNode *getNode(void *entry);

/*
 * funtion to insert a new entry into the trie
 */
void insert(struct TrieNode **root, struct route_table_entry key);

/*
 * funtion to search for the best route in the trie
 */
struct route_table_entry *get_best_route_trie(struct TrieNode *root, uint32_t ip);

/*
 * funtion to print the ip address in a readable format (debugging)
 */
void print_ip_address(uint32_t ip_addr);

/*
 * funtion to print the trie (debugging)
 */
void print_trie(struct TrieNode *root, int level);

/*
 * funtion to insert all the entries from the file into the trie
 */
struct TrieNode *read_trie(const char *path, struct TrieNode *root);