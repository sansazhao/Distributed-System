姓名：赵樱

学号：516030910422

## APIs
### The meter:
```
int 
rte_meter_srtcm_config(struct rte_meter_srtcm *m,
	struct rte_meter_srtcm_params *params);
```  
传入预分配的srtcm数据结构指针和用户自定义的参数params，配置每个流的srtcm。
 
```
enum rte_meter_color
rte_meter_srtcm_color_blind_check(struct rte_meter_srtcm *m, uint64_t time, uint32_t pkt_len);
```
传入srtcm、时间戳、包的长度，以色盲模式为传入的包标记颜色。

### The dropper:
```
int
rte_red_rt_data_init(struct rte_red *red);
```
传入RED指针，初始化run-time数据，返回0为成功，否则为error。

```
int
rte_red_config_init(struct rte_red_config *red_cfg,
	const uint16_t wq_log2,
	const uint16_t min_th,
	const uint16_t max_th,
	const uint16_t maxp_inv);
```
传入config和自定义的参数，初始化RED config，返回0为成功。

```
int
rte_red_enqueue(const struct rte_red_config *red_cfg,
	struct rte_red *red,
	const unsigned q,
	const uint64_t time)
```
根据RED配置和当前queue长度、包的时间，判断包是进入队列还是丢弃。

## 参数调整
### The meter:
- 由main.c: ``` 1000 packets per period avg and 640 bytes per packet avg```, 每秒会发送1000 * 640 * 1000 bytes, 即每个flow平均一秒发送 1.6e8 bytes。
- CIR为承诺信息的速率，需要大于1.6e8的发送速度，可按照最大带宽设置。根据flow0的带宽1.28Gbps, 约1.7e8 Bytes, 所以将flow0 的CIR设置为接近最大带宽值，再根据8:4:2:1的要求，设置flow1，flow2，flow3。
- CBS为承诺突发速率，EBS为超额突发速率，两者也按照比例相应调整，接近CIR。由于令牌桶的流入流出是动态的，CBS的值也可以比CIR大，参考了一些资料在限速时CBS可以为CIR的100-200倍。

### The dropper:
- 主要调整了min_th和max_th，为了尽可能接收绿包，将GREEN的队列长度设为最大。按照8:4:2:1，并考虑到测试中flow_id的随机性，每个period每个流确保能接受1000/4 = 250，将flow0的设为了800，flow1、flow2和flow3按照比例下调（最低250）。
- maxp_inv与丢包率负相关,将GREEN的参数适当调高。
- 在调整了以上参数后，wq_log2的调整影响不大，保留了默认值。