#ifndef VIRT_ISP_MOD_H
#define VIRT_ISP_MOD_H

#include "qemu/osdep.h"
#include "qemu/timer.h"
#include "qemu-common.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "qemu/compiler.h"

#define ISP_LOGE qemu_log
#define ISP_LOGI qemu_log

#define MAKE64(hi, lo) ((uint64_t)((hi) << 32) + (lo))

#define ISP_IOMEM_SIZE    0x400

struct IspIpOps
{
    uint32_t (*reg_read)(struct IspModule *isp_mod, uint32_t addr);
    int (*reg_write)(struct IspModule *isp_mod, uint32_t addr, uint32_t data);
};

struct IspUiDesc
{
    MemoryRegion iomem;
    qemu_irq irq;

    uint32_t regs[ISP_IOMEM_SIZE];
};

enum IspDMAStateMachine
{
    DMA_IDLE = 0,
    DMA_GENERATING,
    DMA_INVALID
};

#define MAX_PLANE_NUM (2)

struct IspDMADesc
{
    uint32_t dma_idx;
    bool is_out_dir;
    hwaddr addr[MAX_PLANE_NUM];
    uint32_t size[MAX_PLANE_NUM];    
};

struct IspDMAModule
{
    struct IspDMADesc desc;
    bool enable;
    enum IspDMAStateMachine state;
    AddressSpace dma_as;
};

#define MAX_IN_DMA_NUM (3)
#define MAX_OUT_DMA_NUM (3)

struct IspModule
{
    struct IspUiDesc ui;
    MemoryRegion *dma_mr;
    struct IspDMAModule dma_modules[MAX_IN_DMA_NUM + MAX_OUT_DMA_NUM];
    const struct IspIpOps *ip_ops;
};

static inline int IsDmaConfigValid(struct IspModule* module, uint32_t dma_idx, uint32_t plane_cnt)
{
    if (dma_idx >= ARRAY_SIZE(module->dma_modules))
    {
        return -1;
    }

    if (plane_cnt > MAX_PLANE_NUM)
    {
        return -1;
    }

    struct IspDMAModule* dma_module = &module->dma_modules[dma_idx];

    for (uint32_t plane = 0; plane < plane_cnt; plane++)
    {
        if ((!dma_module->desc.addr[plane]) || (!dma_module->desc.size[plane]))
        {
            return -1;
        }
    }

    return 0;
}

void virt_isp_module_init(DeviceState *dev, Object* obj, struct IspModule* module, const struct IspIpOps *ops);
int virt_isp_dma_write(struct IspModule* module, uint32_t dma_idx, void* buff, uint32_t len, uint32_t plane_idx);

void virt_isp_set_irq(struct IspModule *module, uint32_t val);

static inline enum IspDMAStateMachine virt_isp_dma_get_state(struct IspModule* module, uint32_t dma_idx)
{
    if (dma_idx >= ARRAY_SIZE(module->dma_modules))
    {
        return DMA_INVALID;
    }
    return module->dma_modules[dma_idx].state;
}

#endif