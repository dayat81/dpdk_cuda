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
#include <rte_icmp.h>
#include <rte_thread.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <rte_log.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 1

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_lro_pkt_size = RTE_ETHER_MAX_LEN}
};

static volatile bool force_quit = false;

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

// Function to handle ICMP packets on a specific port
static void* handle_icmp_packets(void *arg)
{
    unsigned port = *(unsigned *)arg;
    struct rte_mbuf *rx_pkts[BURST_SIZE];
    uint16_t nb_rx, nb_tx;
    struct rte_ether_hdr *eth_hdr;

    while (!force_quit) {
        nb_rx = rte_eth_rx_burst(port, 0, rx_pkts, BURST_SIZE);
        if (nb_rx > 0) {
            for (int i = 0; i < nb_rx; i++) {
                eth_hdr = rte_pktmbuf_mtod(rx_pkts[i], struct rte_ether_hdr *);
                
                if (rte_be_to_cpu_16(eth_hdr->ether_type) == RTE_ETHER_TYPE_IPV4) {
                    struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
                    
                    if (ip_hdr->next_proto_id == IPPROTO_ICMP) {
                        char src_ip[INET_ADDRSTRLEN];
                        char dst_ip[INET_ADDRSTRLEN];
                        
                        inet_ntop(AF_INET, &(ip_hdr->src_addr), src_ip, INET_ADDRSTRLEN);
                        inet_ntop(AF_INET, &(ip_hdr->dst_addr), dst_ip, INET_ADDRSTRLEN);
                        
                        RTE_LOG(INFO, USER1, "Received ICMP packet on port %u from IP: %s to IP: %s\n", port, src_ip, dst_ip);
                        
                        struct rte_icmp_hdr *icmp_hdr = (struct rte_icmp_hdr *)(ip_hdr + 1);
                        RTE_LOG(INFO, USER1, "ICMP Type: %d, Code: %d\n", icmp_hdr->icmp_type, icmp_hdr->icmp_code);

                        // Forward the ICMP packet to the other port
                        unsigned other_port = (port == 0) ? 1 : 0;
                        nb_tx = rte_eth_tx_burst(other_port, 0, &rx_pkts[i], 1);
                        if (nb_tx == 1) {
                            RTE_LOG(INFO, USER1, "Forwarded ICMP packet from port %u to port %u\n", port, other_port);
                        } else {
                            RTE_LOG(WARNING, USER1, "Failed to forward ICMP packet from port %u to port %u\n", port, other_port);
                            rte_pktmbuf_free(rx_pkts[i]);
                        }
                    } else {
                        rte_pktmbuf_free(rx_pkts[i]);
                    }
                } else {
                    rte_pktmbuf_free(rx_pkts[i]);
                }
            }
        }
    }
    return NULL;
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

    RTE_LOG(INFO, USER1, "Initialized 2 ports\n");
    RTE_LOG(INFO, USER1, "Starting packet forwarding...\n");

    // Main loop
    unsigned port_0 = 0;
    unsigned port_1 = 1;
    pthread_t thread_port_0, thread_port_1;

    // Create threads for handling ICMP packets on each port
    if (pthread_create(&thread_port_0, NULL, handle_icmp_packets, &port_0) != 0) {
        rte_exit(EXIT_FAILURE, "Error creating thread for port 0\n");
    }
    if (pthread_create(&thread_port_1, NULL, handle_icmp_packets, &port_1) != 0) {
        rte_exit(EXIT_FAILURE, "Error creating thread for port 1\n");
    }

    // Wait for threads to complete (this will run indefinitely unless force_quit is set)
    pthread_join(thread_port_0, NULL);
    pthread_join(thread_port_1, NULL);

    return 0;
}
