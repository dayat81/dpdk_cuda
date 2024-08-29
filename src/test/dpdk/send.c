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
    .rxmode = {.max_lro_pkt_size = RTE_ETHER_MAX_LEN}};

static inline int
port_init(struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = 1, tx_rings = 1;
    int retval;
    uint16_t q;
    uint32_t cnt_ports;
    cnt_ports = rte_eth_dev_count_avail();
    printf("Number of NICs: %i\n", cnt_ports);
    /*配置端口0,给他分配一个接收队列和一个发送队列*/
    retval = rte_eth_dev_configure(0, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    /* Allocate and set up 1 RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++)
    {
        retval = rte_eth_rx_queue_setup(0, q, RX_RING_SIZE,
                                        rte_eth_dev_socket_id(0), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    /* Allocate and set up 1 TX queue per Ethernet port. */
    for (q = 0; q < tx_rings; q++)
    {
        retval = rte_eth_tx_queue_setup(0, q, TX_RING_SIZE,
                                        rte_eth_dev_socket_id(0), NULL);
        if (retval < 0)
            return retval;
    }

    /* Start the Ethernet port. */
    retval = rte_eth_dev_start(0);
    if (retval < 0)
        return retval;

    return 0;
}

int main(int argc, char *argv[])
{
    struct rte_mempool *mbuf_pool;

    /*进行总的初始话*/
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "initlize fail!");

    // I don't clearly know this two lines
    argc -= ret;
    argv += ret;

    /* Creates a new mempool in memory to hold the mbufs. */
    // 分配内存池
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    // 如果创建失败
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initialize all ports. */
    // 初始话端口设备 顺便给他们分配  队列
    if (port_init(mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu8 "\n",
                 0);
    // 定义报文信息
    struct Message
    {
        char data[10];
    };
    struct rte_ether_hdr *eth_hdr;
    struct Message obj = {{'H', 'e', 'l', 'l', 'o', '2', '0', '1', '8'}};
    struct Message *msg;
    // 自己定义的包头
    struct rte_ether_addr s_addr = {{0x14, 0x02, 0xEC, 0x89, 0x8D, 0x24}};
    struct rte_ether_addr d_addr = {{0x14, 0x02, 0xEC, 0x89, 0xED, 0x54}};
    uint16_t ether_type = 0x0a00;

    // 对每个buf ， 给他们添加包

    struct rte_mbuf *pkt[BURST_SIZE];
    int i;
    for (i = 0; i < BURST_SIZE; i++)
    {
        pkt[i] = rte_pktmbuf_alloc(mbuf_pool);
        eth_hdr = rte_pktmbuf_mtod(pkt[i], struct rte_ether_hdr *);
        eth_hdr->dst_addr = d_addr;
        eth_hdr->src_addr = s_addr;
        eth_hdr->ether_type = ether_type;
        msg = (struct Message *)(rte_pktmbuf_mtod(pkt[i], char *) + sizeof(struct rte_ether_hdr));
        *msg = obj;
        int pkt_size = sizeof(struct Message) + sizeof(struct rte_ether_hdr);
        pkt[i]->data_len = pkt_size;
        pkt[i]->pkt_len = pkt_size;
    }

    uint16_t nb_tx = rte_eth_tx_burst(0, 0, pkt, BURST_SIZE);
    printf("%d \n", nb_tx);
    // 发送完成，答应发送了多少个

    for (i = 0; i < BURST_SIZE; i++)
        rte_pktmbuf_free(pkt[i]);

    return 0;
}
