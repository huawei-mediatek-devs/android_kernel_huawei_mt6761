#ifndef _DYNAMIC_MMEM_H_
#define _DYNAMIC_MMEM_H_
#include "teek_ns_client.h"
struct sg_memory {
	struct ion_handle *ion_handle;
	phys_addr_t ion_phys_addr;
	size_t len;
	void *ion_virt_addr;
};
struct dynamic_mem_item{
	struct list_head head;
	uint32_t configid;
	uint32_t size;
	struct sg_memory memory;
	unsigned int cafd;
	TEEC_UUID uuid;
};
int init_dynamic_mem(void);
void exit_dynamic_mem(void);
int load_app_use_configid(uint32_t configid, uint32_t cafd,  TEEC_UUID* uuid, uint32_t size);
void kill_ion_by_cafd(int cafd);
void kill_ion_by_uuid(TEEC_UUID* uuid);
#endif
