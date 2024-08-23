/*
接收一个包并且打印出信息
Author : Hox Zheng
Date : 2017年 12月 31日 星期日 15:21:04 CST
 */
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

#define RX_RING_SIZE 128

#define TX_RING_SIZE 512

#define NUM_MBUFS 8191

#define MBUF_CACHE_SIZE 250

#define BURST_SIZE 32

// 这里用skleten 默认配置
static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_lro_pkt_size = RTE_ETHER_MAX_LEN}};

/*
 *这个是简单的端口初始化
 *我在这里简单的端口0 初始化了一个 接收队列和一个发送队列
 */
static inline int
port_init(struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = 1, tx_rings = 1;
    int retval;
    uint16_t q;

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

/*
我在main 函数里面 调用了输出化端口0的函数
申请了 m_pool
定义m_buf用来取接受队列中的包
 */
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
    // 定义m_buf，用来接受接受对列中的包
    printf("初始话就绪，正在持续接受数据包.......\n");

    for (;;)
    {

        struct rte_mbuf *pkt[BURST_SIZE];
        int i;
        // 定义m_buf 并且分配内存
        for (i = 0; i < BURST_SIZE; i++)
        {
            pkt[i] = rte_pktmbuf_alloc(mbuf_pool);
        }

        // 从接受队列中取出包
        uint16_t nb_rx = rte_eth_rx_burst(0, 0, pkt, BURST_SIZE);
        if (nb_rx == 0)
        {
            // 如果没有接受到就跳过
            continue;
        }
        char *msg;
        struct rte_ether_hdr *eth_hdr;
        // 打印信息
        for (i = 0; i < nb_rx; i++)
        {
            // eth_hdr = rte_pktmbuf_mtod(pkt[i], struct ether_hdr *);
            //  打印接受到的包的地址信息
            eth_hdr = rte_pktmbuf_mtod(pkt[i], struct rte_ether_hdr *);
            printf("收到包 来自MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
                   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " : ",
                   eth_hdr->src_addr.addr_bytes[0], eth_hdr->src_addr.addr_bytes[1],
                   eth_hdr->src_addr.addr_bytes[2], eth_hdr->src_addr.addr_bytes[3],
                   eth_hdr->src_addr.addr_bytes[4], eth_hdr->src_addr.addr_bytes[5]);
            msg = ((rte_pktmbuf_mtod(pkt[i], char *)) + sizeof(struct rte_ether_hdr));
            int j;
            for (j = 0; j < 10; j++)
                printf("%c", msg[j]);
            printf("\n");
            rte_pktmbuf_free(pkt[0]);
        }
    }

    return 0;
}