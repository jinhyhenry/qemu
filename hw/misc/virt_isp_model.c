#include "hw/misc/virt_isp_model.h"

static void virt_isp_iomem_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    int ret;
    struct IspModule *module = (struct IspModule*)opaque;
    assert(module);

    uint32_t reg_addr = (uint32_t)offset;
    uint32_t reg_val = (uint32_t)value;

    assert(reg_addr < ISP_IOMEM_SIZE);

    if (module->ip_ops && module->ip_ops->reg_write)
    {
        ret = module->ip_ops->reg_write(module, reg_addr, (uint32_t)value);
        if (ret)
        {
            LOGE("fail to write register\n");
            return;
        }
    }
    else
    {
        assert(0);
    }

    module->ui.regs[reg_addr] = reg_val;
}

static uint64_t virt_isp_iomem_read(void *opaque, hwaddr offset,
        unsigned size)
{
    uint32_t reg_addr = (uint32_t)offset;

    assert(reg_addr < ISP_IOMEM_SIZE);

    if (module->ip_ops && module->ip_ops->reg_read)
    {
        return module->ip_ops->reg_read(module, reg_addr);
    }

    return module->ui.regs[reg_addr];
}

void virt_isp_set_irq(struct IspModule *module, uint32_t val)
{
    qemu_set_irq(module->ui.irq, val);
}

static const MemoryRegionOps virt_isp_iomem_ops = {
    .read = virt_isp_iomem_read,
    .write = virt_isp_iomem_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    }
};

int virt_isp_dma_write(struct IspModule* module, uint32_t dma_idx, void* buff, uint32_t len, uint32_t plane_idx)
{
    module->dma_modules[dma_idx].state = DMA_GENERATING;
    MemTxResult ret = address_space_write(&module->dma_modules[dma_idx].dma_as,
                        module->dma_modules[dma_idx].desc.addr[plane_idx], MEMTXATTRS_UNSPECIFIED, buff, len);
    if (ret)
    {
        ISP_LOGE("failed to write dma %d\n", ret);
    }

    module->dma_modules[dma_idx].state = DMA_IDLE;

    return ret;
}

int virt_isp_dma_init(struct IspModule* module, struct IspDMADesc *desc)
{
    if ((!module) || (!desc) || (desc->dma_idx > ARRAY_SIZE(module->dma_modules)))
    {
        return -1;
    }

    memcpy(&module->dma_modules[dma_idx].desc, desc, sizeof(*desc));

    module->dma_modules[dma_idx].enable = false;
    module->dma_modules[dma_idx].state = DMA_IDEL;

    address_space_init(&s->dma_as, module->dma_mr, "isp_dma");
    return 0;
}

void virt_isp_module_init(DeviceState *dev, Object* obj, struct IspModule* module, const struct IspIpOps *ops)
{
    memset(module, 0, sizeof(*module));
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &module->ui.irq);
    memory_region_init_io(&module->ui.iomem, obj, &virt_isp_iomem_ops, module,
                          "ISP", ISP_IOMEM_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), );

    object_property_add_link(obj, "isp_dma", TYPE_MEMORY_REGION,
                             module->dma_mr,
                             qdev_prop_allow_set_link_before_realize,
                             OBJ_PROP_LINK_STRONG);

    module->ip_ops = ops;
}