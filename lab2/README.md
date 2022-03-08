# Lab 2 send and receive packets with DPDK

## Part1

> Q1: What's the purpose of using hugepage?

大页可以减少TLB缓存项的使用，增大TLB命中概率；减少查询页表项的层数，提升查询效率。

> Q2: Take examples/helloworld as an example, describe the execution flow of DPDK programs?

具体执行流如下所示：

+ ```c++
  ret = rte_eal_init(argc, argv);
  ```

  rte_eal_init函数根据用户指定的参数进行各种初始化，构建一个针对包处理设计的运行环境。

+ DPDK是面向多核设计的，程序会试图独占运行在逻辑核(Icore)上。

  ```c++
  /* call lcore_hello() on every worker lcore */
  RTE_LCORE_FOREACH_WORKER(lcore_id) {
      rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
  }
  ```

  该函数会遍历所有可使用的Icore，然后通过rte_eal_remote_launch函数在每一个lcore上启动被指定的线程。

+ ```c++
  /* call it on main lcore too */
  lcore_hello(NULL);
  ```

  主lcore也调用该函数。

+ ```c++
  static int
  lcore_hello(__rte_unused void *arg)
  {
  	unsigned lcore_id;
  	lcore_id = rte_lcore_id();
  	printf("hello from core %u\n", lcore_id);
  	return 0;
  }
  ```

  每个lcore都会打印自己的信息。

+ ```c++
  rte_eal_mp_wait_lcore();
  ```

  主lcore调用该函数挂起并等待其他lcore执行完毕。

+ rte_eal_cleanup清理环境，退出。

官方文档中的一张图很好地展示了相关流程。

![linuxapp_launch](https://doc.dpdk.org/guides-2.0/_images/linuxapp_launch.svg)

> Q3: Read the codes of examples/skeleton, describe DPDK APIs related to sending and receiving packets.

| Api                                              | 作用描述                                                     |
| ------------------------------------------------ | ------------------------------------------------------------ |
| rte_eth_rx_queue_setup && rte_eth_tx_queue_setup | 为以太网设备分配和设置接收队列。对指定端口的某个队列，指定内存、描述符数量和报文缓冲区。 |
| rte_eth_dev_start                                | 启动以太网设备，开启网口。                                   |
| rte_eth_macaddr_get                              | 根据端口获取MAC物理地址。                                    |
| rte_eth_promiscuous_enable                       | 开启混杂模式，接受全部的报文。                               |
| rte_eth_rx_burst && rte_eth_tx_burst             | 基于端口队列的报文收发函数。                                 |

> Q4: Describe the data structure of 'rte_mbuf'.

主要结构如下图所示（源自于官方文档）。

rte_mbuf主要由元数据和数据包构成。rte_mbuf将元数据嵌入到单个内存缓冲区中，该结构后跟用于数据包数据的固定大小区域。元数据包含控制信息，例如消息类型、长度、到数据开头的偏移量以及用于允许缓冲区链接的其他 mbuf 结构的指针。

uint16_t nb_segs 表示当前的mbuf报文有多少个分段；struct rte_mbuf *next 表示下一个分段的地址。

![../_images/mbuf1.svg](http://doc.dpdk.org/guides/_images/mbuf1.svg)

![../_images/mbuf2.svg](http://doc.dpdk.org/guides/_images/mbuf2.svg)
