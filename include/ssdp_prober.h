#ifndef __SSDP_PROBER_H__
#define __SSDP_PROBER_H__

#include "configuration.h"

typedef struct ssdp_prober_s ssdp_prober_s;

/**
 * Create a standard SSDP probe message.
 */
const char *create_ssdp_probe_message(void);

/**
 * Tells the SSDP prober to probe/scan for SSDP-capable nodes.
 *
 * @param prober The prober to start.
 * @param conf The global configuration to use.
 *
 * 0 on success, non-0 value on error.
 */
int ssdp_prober_start(ssdp_prober_s *prober, configuration_s *conf);

#endif /* __SSDP_PROBER_H__ */
