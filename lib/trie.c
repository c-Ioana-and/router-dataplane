#include "trie.h"

void print_ip_address(uint32_t ip_addr) {
	uint8_t *ip_bytes = (uint8_t *) &ip_addr;
	printf("%d.%d.%d.%d\n", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
}

// Returns new trie node
struct TrieNode *getNode(void *entry) {
    struct TrieNode *temp = (struct TrieNode *)malloc(sizeof(struct TrieNode));
    temp->isLeaf = 0;
    for (int i = 0; i < MAX_PREFIX_LEN; i++)
        temp->children[i] = NULL;
    return temp;
}
 
// function that inserts the entry in the trie 
void insert(struct TrieNode **root, struct route_table_entry entry) {
    int index;
    struct TrieNode *temp = *root;
    uint8_t *ip_bytes = (uint8_t *) &entry.prefix;
    for (int level = 0; level < MAX_PREFIX_LEN; level++) {
        index = ip_bytes[level];
        if (!temp->children[index])
            temp->children[index] = getNode(NULL);
        temp = temp->children[index];
    }
    temp->isLeaf = 1;
    // if this is the first entry for this prefix, then add it
    if (temp->entry == NULL) {
        temp->entry = (struct route_table_entry *)malloc(sizeof(struct route_table_entry));
        memcpy(temp->entry, &entry, sizeof(struct route_table_entry));
    } else {
        // update entry so that the biggest mask is kept, since that's
        // what I need for the longest prefix match
        struct route_table_entry *old_entry = temp->entry;
        if (ntohl(old_entry->mask) < ntohl(entry.mask))
            memcpy(temp->entry, &entry, sizeof(struct route_table_entry));
    }
}

// function that finds the longest prefix match for the given ip address
struct route_table_entry *get_best_route_trie(struct TrieNode *root, uint32_t ip_addr) {
    struct TrieNode *temp = root;
    uint8_t *ip_bytes = (uint8_t *) &ip_addr;

    int level;
    for (level = 0; level < MAX_PREFIX_LEN; level++) {
        int index = ip_bytes[level];
        if (!temp->children[index])
            break;
        temp = temp->children[index];
    }
    // the match will not be exact (to be expected)
    if (level != MAX_PREFIX_LEN)  {
       while (level != MAX_PREFIX_LEN){
            int index = 0;  // 0 because the rest of the bits are 0
            if (!temp->children[index])
                break;
            temp = temp->children[index];
            level++;
        }
    }
    if (temp != NULL && temp->isLeaf) {
        struct route_table_entry *curr_entry = temp->entry;
        // check if the prefix matches
        if ((ip_addr & curr_entry->mask) == curr_entry->prefix) {
            return curr_entry;
        }
    }
    return NULL;
}

// print the trie for debugging
void print_trie(struct TrieNode *root, int level) {
    if (root->isLeaf) {
        struct route_table_entry *entry = root->entry;
        print_ip_address(entry->prefix);
    }
    for (int i = 0; i < ONE_BYTE_SIZE; i++) {
        if (root->children[i]) {
            print_trie(root->children[i], level + 1);
        }
    }
}

struct TrieNode * read_trie(const char *path, struct TrieNode *root) {
	FILE *fp = fopen(path, "r");
    root = getNode(NULL);
	int j = 0, i;
	char *p, line[64];

    struct route_table_entry entry;
	while (fgets(line, sizeof(line), fp) != NULL) {
		p = strtok(line, " .");
		i = 0;
		while (p != NULL) {
			if (i < 4)
				*(((unsigned char *)&entry.prefix) + i % 4) = (unsigned char)atoi(p);

			if (i >= 4 && i < 8)
				*(((unsigned char *)&entry.next_hop) + i % 4) = atoi(p);

			if (i >= 8 && i < 12)
				*(((unsigned char *)&entry.mask) + i % 4) = atoi(p);

			if (i == 12)
				entry.interface = atoi(p);
			p = strtok(NULL, " .");
			i++;
		}
        insert(&root, entry);
		j++;
	}
	return root;
}
