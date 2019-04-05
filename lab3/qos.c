#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

/*
 * Traffic metering configuration
 *
 */

// flow0: 1.28Gbps, 8:4:2:1 
struct rte_meter_srtcm_params app_srtcm_params[APP_FLOWS_MAX] = {
        {.cir = 171700000, .cbs = 170000000, .ebs = 170000000},
        {.cir = 85850000 , .cbs = 85000000, .ebs = 85000000},
        {.cir = 42925000 , .cbs = 42000000, .ebs = 42000000},
        {.cir = 21462500 , .cbs = 21000000, .ebs = 21000000}
};

struct rte_meter_srtcm app_flows[APP_FLOWS_MAX];

/**
 * srTCM
 */
int
qos_meter_init(void)
{
    int ret = 0;
    for (int i = 0; i < APP_FLOWS_MAX; ++i) {
        ret = rte_meter_srtcm_config(&app_flows[i], &app_srtcm_params[i]);
        if (ret) 
            return ret;
    }
    return 0;
}

enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    return rte_meter_srtcm_color_blind_check(&app_flows[flow_id], time, pkt_len);	
}


/**
 * WRED
 */

struct rte_red_config red_config[APP_FLOWS_MAX][e_RTE_METER_COLORS];
struct rte_red reds[APP_FLOWS_MAX][e_RTE_METER_COLORS];
unsigned queue[APP_FLOWS_MAX];
struct rte_red_params red_param[APP_FLOWS_MAX][e_RTE_METER_COLORS] = {
	/* Green / Yellow / Red */
	{
        {.min_th = 800, .max_th = 810, .maxp_inv = 50, .wq_log2 = 9},
	    {.min_th = 48,  .max_th = 64,  .maxp_inv = 10, .wq_log2 = 9},
	    {.min_th = 1,   .max_th = 2,   .maxp_inv = 1, .wq_log2 = 9}
    },
	{
        {.min_th = 400, .max_th = 421, .maxp_inv = 50, .wq_log2 = 9},
	    {.min_th = 48,  .max_th = 64,  .maxp_inv = 10, .wq_log2 = 9},
	    {.min_th = 1,   .max_th = 2,   .maxp_inv = 1, .wq_log2 = 9}
    },
	{
        {.min_th = 250, .max_th = 260, .maxp_inv = 50, .wq_log2 = 9},
        {.min_th = 20, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
        {.min_th = 1,  .max_th = 2,  .maxp_inv = 1, .wq_log2 = 9}
    },
	{
        {.min_th = 250, .max_th = 260, .maxp_inv = 50, .wq_log2 = 9},
        {.min_th = 20, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
        {.min_th = 1,  .max_th = 2,  .maxp_inv = 1, .wq_log2 = 9}
    } 
};

uint64_t period = 0;
int
qos_dropper_init(void)
{
    int ret;
    for (int i = 0; i < APP_FLOWS_MAX; ++i) {
        for (int j = 0; j < e_RTE_METER_COLORS; ++j) {
            ret = rte_red_rt_data_init(&reds[i][j]); //run-time data
            if (ret < 0)
                rte_exit(EXIT_FAILURE, "Unable to rte_red_rt_data_init\n"); 
            ret = rte_red_config_init(&red_config[i][j],
                                        red_param[i][j].wq_log2, 
                                        red_param[i][j].min_th,
                                        red_param[i][j].max_th, 
                                        red_param[i][j].maxp_inv); 
            if (ret < 0)
            	rte_exit(EXIT_FAILURE, "Unable to rte_red_config_init\n"); 
        }
        queue[i] = 0;
    }
    return 0;
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    if (period != time/1000000) {   //clear queue every period
        period = time/1000000;
        for (int i = 0; i < APP_FLOWS_MAX; ++i)
            queue[i] = 0;
    }
    if (rte_red_enqueue(&red_config[flow_id][color],
	                    &reds[flow_id][color], queue[flow_id], time) == 0) {
        queue[flow_id]++;
        return 0;
    }
    return 1;
}