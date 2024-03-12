#include "lib.h"
#include "protocols.h"
#include "queue.h"
#include "list.h"
#include "trie.h"

/* Routing table */
struct route_table_entry *rtable;
int rtable_len;

/* Arp table */
struct arp_entry *arp_table;
int arp_table_len;

/* Queue */
queue arp_queue;

/* Trie */
struct TrieNode *trie;

struct arp_entry *get_arp_entry(uint32_t ip_dest) {
	// Iterate through the ARP table and search for an entry that matches ip_dest
	for (struct arp_entry *i = arp_table; i < arp_table + arp_table_len; i++)
		if (ip_dest == i->ip)
			return i;
	return NULL;
}

void send_all_packets() {
	// search through the queue and send all packets that can now be sent after
	// the router received an ARP reply to its' request
	queue aux = queue_create();
	while (!queue_empty(arp_queue)) {
		char *buf = ((queue_entry)(arp_queue->head->element))->buf;
		int len = ((queue_entry)arp_queue->head->element)->len;
		struct ether_header *eth_hdr = (struct ether_header *) buf;
		struct iphdr *ip_hdr = (struct iphdr *)(buf + sizeof(struct ether_header));
		struct route_table_entry *best_router = get_best_route_trie(trie, ip_hdr->daddr);

		if (best_router == NULL)
			continue;

		struct arp_entry *arp_entry = get_arp_entry(best_router->next_hop);
		if (arp_entry != NULL) {
			// if the packet can now be sent (it's destination address is in the router's arp table)
			uint32_t ip;
			inet_pton(AF_INET, get_interface_ip(best_router->interface), &ip);
			get_interface_mac(best_router->interface, eth_hdr->ether_shost);
			memcpy(eth_hdr->ether_dhost, arp_entry->mac, 6);
			send_to_link(best_router->interface, buf, len);
		}
		else queue_enq(aux, arp_queue->head->element);

		arp_queue->head = arp_queue->head->next;
	}
	// arp_queue will be empty after this loop
	// need to dump what's in aux into the original queue
	while (!queue_empty(aux)) {
		queue_enq(arp_queue, aux->head->element);
		aux->head = aux->head->next;
	}
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);

	int interface;
	char buf[MAX_PACKET_LEN];
	int len;
	/* Don't touch this */
	init(argc - 2, argv + 2);

	/* Code to allocate the ARP and route tables */
	rtable = malloc(sizeof(struct route_table_entry) * 100000);
	arp_table = malloc(sizeof(struct arp_entry) * 100);

	DIE(rtable == NULL, "memory");
	DIE(arp_table == NULL, "memory");

	/* Read the static routing table and insert in trie */
	trie = read_trie(argv[1], trie);
	arp_table_len = 0;

	while (1) {
		interface = recv_from_any_link(buf, ((size_t *)&len));
		DIE(interface < 0, "recv_from_any_links");

		// Extract the Ethernet header from the packet
		struct ether_header *eth_hdr = (struct ether_header *) buf;
		// check what type of packet the router is receiving: ARP or IP?
		if (eth_hdr->ether_type == 0x08) {
			struct iphdr *ip_hdr = (struct iphdr *)(buf + sizeof(struct ether_header));
			uint16_t old_check = ip_hdr->check;
			ip_hdr->check = 0;
			
			// Check the ip_hdr integrity
			if (old_check != htons(checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)))) {
				continue;
			}
			else {
				uint32_t ip;
			 	inet_pton(AF_INET, get_interface_ip(interface), &ip);
			 	
				if (ip == ip_hdr->daddr) {
					// ICMP ECHO REPLY
			 		// the packet is for this router
			 		struct icmphdr *icmp_hdr = (struct icmphdr *)(buf + sizeof(struct ether_header) + sizeof(struct iphdr));
			 		// switch mac addreses in ether header
			 		uint8_t aux;
			 		for (int i = 0 ; i < 6 ; i++) {
			 			aux = eth_hdr->ether_dhost[i];
			 			eth_hdr->ether_dhost[i] = eth_hdr->ether_shost[i];
			 			eth_hdr->ether_shost[i] = aux;
			 		}
			 		// set icmp reply header
			 		icmp_hdr->type = 0;
			 		icmp_hdr->checksum = 0x0000;
			 		icmp_hdr->checksum = htons(checksum((uint16_t *)icmp_hdr, 64));
			 		
			 		send_to_link(interface, buf, len);
			 		continue;
			 	}
				// Call get_best_route_trie to find the most specific route, drop if null 
				struct route_table_entry *best_router = get_best_route_trie(trie, ip_hdr->daddr);

				if (best_router == NULL) {
					// DESTINATION UNREACHABLE : my router cannot redirect the packet
			 		struct icmphdr *icmp_hdr = (struct icmphdr *)(buf + sizeof(struct ether_header) + sizeof(struct iphdr));
			 		// switch mac addreses in ether header
			 		uint8_t aux;
			 		for (int i = 0 ; i < 6 ; i++) {
			 			aux = eth_hdr->ether_dhost[i];
			 			eth_hdr->ether_dhost[i] = eth_hdr->ether_shost[i];
			 			eth_hdr->ether_shost[i] = aux;
			 		}
			 		// set icmp reply header
					memcpy((uint8_t *)icmp_hdr + 8, ip_hdr, sizeof(struct iphdr) + 8);
			 		icmp_hdr->code = 0;
			 		icmp_hdr->type = 3;
			 		icmp_hdr->checksum = 0x0000;
			 		icmp_hdr->checksum = htons(checksum((uint16_t *)icmp_hdr, 8 + sizeof(struct iphdr) + 8));
					
					ip_hdr->daddr = ip_hdr->saddr;
					char *ip_char = get_interface_ip(interface);
					inet_pton(AF_INET, ip_char, &ip_hdr->saddr);

					len = sizeof(struct ether_header) + sizeof(struct iphdr) * 2 + 16;

					ip_hdr->tot_len = htons((uint16_t)len - sizeof(struct ether_header));
					ip_hdr->check = htons(checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));
					ip_hdr->protocol = 1;

					send_to_link(interface, buf, len);
			 		continue;
			 	}
				// Check TTL >= 1. Update TLL. Update checksum
				if (ip_hdr->ttl <= 1) {
					// TIME EXCEEDED
			 		struct icmphdr *icmp_hdr = (struct icmphdr *)(buf + sizeof(struct ether_header) + sizeof(struct iphdr));
			 		// switch mac addreses in ether header
			 		uint8_t aux;
			 		for (int i = 0 ; i < 6 ; i++) {
			 			aux = eth_hdr->ether_dhost[i];
			 			eth_hdr->ether_dhost[i] = eth_hdr->ether_shost[i];
			 			eth_hdr->ether_shost[i] = aux;
			 		}
			 		// set icmp reply header
					memcpy((uint8_t *)icmp_hdr + 8, ip_hdr, sizeof(struct iphdr) + 8);
			 		icmp_hdr->code = 0;
			 		icmp_hdr->type = 11;
			 		icmp_hdr->checksum = 0x0000;
			 		icmp_hdr->checksum = htons(checksum((uint16_t *)icmp_hdr, 8 + sizeof(struct iphdr) + 8));
					
					ip_hdr->daddr = ip_hdr->saddr;
					char *ip_char = get_interface_ip(interface);
					inet_pton(AF_INET, ip_char, &ip_hdr->saddr);
					// the packet contains an ether header, two ip headers (the actual + original one)
					// 8 bytes for ICMP and 8 bytes of data
					len = sizeof(struct ether_header) + sizeof(struct iphdr) * 2 + 16;

					ip_hdr->tot_len = htons((uint16_t)len - sizeof(struct ether_header));
					ip_hdr->check = htons(checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));
					ip_hdr->protocol = 1;
					ip_hdr->ttl = 64;

			 		send_to_link(interface, buf, len);
			 		continue;
				} else {
					ip_hdr->ttl--;
					ip_hdr->check = 0;
					ip_hdr->check = htons(checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));
				}
				// Update the ethernet addresses
			 	struct arp_entry *arp_entry = get_arp_entry(best_router->next_hop);

			 	if (arp_entry == NULL) {
					// Add the packet to the queue
					if (arp_queue == NULL)
						arp_queue = queue_create();

					queue_entry q_entry = malloc(sizeof(struct queue_entry));
					q_entry->buf = malloc(len);
					memcpy(q_entry->buf, buf, len);
					q_entry->len = len;

					queue_enq(arp_queue, q_entry);
					// Generate an ARP Request
					char arp_buf[MAX_PACKET_LEN];
					// populating ethernet header
					struct ether_header *eth_hdr_arp = (struct ether_header *) arp_buf;
					eth_hdr_arp->ether_type = htons(0x806);
					get_interface_mac(best_router->interface, eth_hdr_arp->ether_shost);
					for (int i = 0; i < 6 ; i++)
						eth_hdr_arp->ether_dhost[i] = 0xFF;
					// populate arp header
					struct arp_header *arp_hdr = (struct arp_header *)(arp_buf + sizeof(struct ether_header));

					arp_hdr->htype = htons(1);
					arp_hdr->ptype = 0x08;
					arp_hdr->hlen = 6;
					arp_hdr->plen = 4;
					arp_hdr->op = htons(1);

					for (int i = 0; i < 6 ; i++)
						arp_hdr->tha[i] = 0x0;
					arp_hdr->tpa = best_router->next_hop;

					uint32_t ip = 0;
					char *ip_char = get_interface_ip(best_router->interface);
					inet_pton(AF_INET, ip_char, &ip);
					inet_pton(AF_INET, get_interface_ip(best_router->interface), &(arp_hdr->spa));
					get_interface_mac(best_router->interface, arp_hdr->sha);

					send_to_link(best_router->interface, arp_buf, sizeof(struct ether_header) + sizeof(struct arp_header));
				}
			 	else {
					// I can send the packet with no issues
				 	memcpy(eth_hdr->ether_dhost, arp_entry->mac, 6);
				 	send_to_link(best_router->interface, buf, len);
				}
			}
		} else {
			struct arp_header *arp_hdr = (struct arp_header *)(buf + sizeof(struct ether_header));
			if (arp_hdr->op == 0x100) {
				// ARP REQUEST, need to generate an arp reply
				// arp header: the source is now my router
				memcpy(arp_hdr->tha, arp_hdr->sha, 6);
				get_interface_mac(interface, arp_hdr->sha);

				arp_hdr->op = 0x200;
				// ether header
				memcpy(eth_hdr->ether_dhost, eth_hdr->ether_shost, 6);
				get_interface_mac(interface, eth_hdr->ether_shost);
	
				uint32_t aux = arp_hdr->tpa;
				arp_hdr->tpa = arp_hdr->spa;
				arp_hdr->spa = aux;

				send_to_link(interface, buf, sizeof(struct ether_header) + sizeof(struct arp_header));
			} else {
				if (get_arp_entry(arp_hdr->spa) == NULL) {
					// arp response
					// add the ip + mac address combo into the arp table
					arp_table[arp_table_len].ip = arp_hdr->spa;
					memcpy(arp_table[arp_table_len].mac, arp_hdr->sha, 6);
					arp_table_len++;
					// search queue for any packets waiting for this entry
					if (arp_queue != NULL)
						send_all_packets(interface);
				}
			}
		}	
	}
}