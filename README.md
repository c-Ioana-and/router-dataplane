# Dataplane router

În acest repository am implementat dataplane-ul unui router capabil să dirijeze pachete, să identifice și să trimită pachete de tip ARP și ICMP. Partea de dirijare, ARP și ICMP sunt implementate în fișierul router.c, iar partea de LPM (Longest Prefix Match) se află în fișierele trie.c și trie.h. Când router-ul primește un pachet, acesta extrage headerul Ethernet și verifică tipul pachetului (daca este de tip ARP sau IP).

- Dacă pachetul este de tip IP și checksum-ul este cel corespunzător, router-ul verifică:
  - dacă pachetul este destinat lui (ip == ip_hdr->daddr, unde ip este adresa routerului obținută
folosind funcția get_interface_ip). Dacă da, oferă un răspuns de tip ICMP. Aici am preferat să
modific bufferul deja existent. Aici am folosit funcția inet_pton pentru a converti adresa IP
din char* in uint32_t;
  - altfel, router-ul verifică dacă adresa destinație a pachetului este accesibilă folosind funcția get_best_route_trie. Aici intervine implementarea algoritmului Longest Prefix Match, unde am folosit un trie în care fiecare nivel corespunde unui byte dintr-un prefix.
    Fiecare nod are un vector de copii (un nod poate avea maxim 255 de copii), un câmp isLeaf,
și un câmp pentru a reține structura route_table_entry. Am ales ca numai nodurile frunză să rețină
datele din tabela de rutare și, atunci când router-ul întâmpină prefixuri care se repetă, trie-ul
va reține doar intrarea cu masca cea mai mare;

- După aceea, router-ul verifică dacă TTL-ul este valid - daca nu, modific buferul curent
astfel încat să trimit un mesaj de tip Destination Unreachable: adaug header-ul ICMP,
modific header-ul IP și Ethernet pentru a trimite pachetul înapoi la sursă și pentru a
actualiza lungimea pachetului:
    14 (Ether) + 20 (IP header) + 64 (32 code, type și checksum + 32 unused de la ICMP) +
20 (headerul IP original) + 64 (8 bytes din conținutul pachetului original)
    Modific și TTL-ul și protocolul din headerul IP (protocol = 1 pentru ICMP);

- Dacă este vaid, doar scad TTL-ul și recalculez checksum-ul, și verific dacă router-ul meu
știe care este adresa MAC pentru next hop (get_arp_entry(best_router->next_hop) != NULL).
Dacă nu, rețin pachetul curent într-o coadă, generez un request spre host-ul cu adresa de
next hop și îl trimit. Aici am folosit un buffer nou pentru pachet-ul de tip ARP Request.
Altfel, trimit pur și simplu pachetul.

- Daca pachetul este de tip ARP, și este de tip:
  - REQUEST (op == 0x100) : modific pachetul existent pentru a oferi un reply conținând adresa IP
și adresa MAC a interfeței curente pe router.
  - REPLY : rețin datele oferite din reply (adresa IP și MAC) în tabela arp și folosesc funcția
send_all_packets pentru a trimite toate pachetele care corespund acelei intrări noi în tabelă.
Funcția practic parcurge coada cu pachetele reținute și, dacă acum next-hop-ul este accesibil
(daca am primit un reply), trimit pachetul. Restul de pachete vor fi puse la loc.
