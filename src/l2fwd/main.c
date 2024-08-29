#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <pthread.h>
#include <string.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 1

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_lro_pkt_size = RTE_ETHER_MAX_LEN}
};

static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = 1, tx_rings = 1;
    int retval;
    uint16_t q;

    if (!rte_eth_dev_is_valid_port(port))
        return -1;

    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    for (q = 0; q < rx_rings; q++) {
        retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
                rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    for (q = 0; q < tx_rings; q++) {
        retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
                rte_eth_dev_socket_id(port), NULL);
        if (retval < 0)
            return retval;
    }

    retval = rte_eth_dev_start(port);
    if (retval < 0)
        return retval;

    return 0;
}

int
main(int argc, char *argv[])
{
    struct rte_mempool *mbuf_pool;
    uint16_t portid;

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    argc -= ret;
    argv += ret;

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * 2,
        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    uint16_t nb_ports = rte_eth_dev_count_avail();
    if (nb_ports < 2)
        rte_exit(EXIT_FAILURE, "Error: number of ports must be at least 2\n");

    for (portid = 0; portid < 2; portid++) {
        if (port_init(portid, mbuf_pool) != 0)
            rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16 "\n", portid);
    }

    printf("Initialized 3 ports\n");
    printf("Starting packet forwarding...\n");

    struct rte_mbuf *rx_pkts[BURST_SIZE];
    struct rte_mbuf *tx_pkts[BURST_SIZE];
    uint16_t nb_rx, nb_tx;

    // Define message structure
    struct Message {
        char data[10];
    };

    // Define Ethernet header and addresses
    struct rte_ether_hdr *eth_hdr;
    struct rte_ether_addr s_addr = {{0x14, 0x02, 0xEC, 0x89, 0x8D, 0x24}};
    struct rte_ether_addr d_addr = {{0x14, 0x02, 0xEC, 0x89, 0xED, 0x54}};
    uint16_t ether_type = 0x0a00;

    // Main loop
    for (;;) {
        // Receive packets on port 0
        nb_rx = rte_eth_rx_burst(0, 0, rx_pkts, BURST_SIZE);
        if (nb_rx > 0) {
            //printf("Received %d packets on port 0\n", nb_rx);
            
            // Process received packets
            for (int i = 0; i < nb_rx; i++) {
                eth_hdr = rte_pktmbuf_mtod(rx_pkts[i], struct rte_ether_hdr *);
                
                // Check if it's an IPv4 packet
                if (rte_be_to_cpu_16(eth_hdr->ether_type) == RTE_ETHER_TYPE_IPV4) {
                    struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
                    
                    // Check if it's an ICMP packet
                    if (ip_hdr->next_proto_id == IPPROTO_ICMP) {
                        char src_ip[INET_ADDRSTRLEN];
                        char dst_ip[INET_ADDRSTRLEN];
                        
                        inet_ntop(AF_INET, &(ip_hdr->src_addr), src_ip, INET_ADDRSTRLEN);
                        inet_ntop(AF_INET, &(ip_hdr->dst_addr), dst_ip, INET_ADDRSTRLEN);
                        
                        printf("Received ICMP packet from IP: %s to IP: %s\n", src_ip, dst_ip);
                        
                        struct rte_icmp_hdr *icmp_hdr = (struct rte_icmp_hdr *)(ip_hdr + 1);
                        printf("ICMP Type: %d, Code: %d\n", icmp_hdr->icmp_type, icmp_hdr->icmp_code);

                        // Forward the ICMP packet to port 1
                        nb_tx = rte_eth_tx_burst(1, 0, &rx_pkts[i], 1);
                        if (nb_tx == 1) {
                            printf("Forwarded ICMP packet to port 1\n");
                        } else {
                            printf("Failed to forward ICMP packet to port 1\n");
                            rte_pktmbuf_free(rx_pkts[i]);
                        }
                    } else {
                        // Free non-ICMP packets
                        rte_pktmbuf_free(rx_pkts[i]);
                    }
                } else {
                    // Free non-IPv4 packets
                    rte_pktmbuf_free(rx_pkts[i]);
                }
            }
        }
    }

    return 0;
}
