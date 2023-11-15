#include "hw/misc/virt_isp_model.h"
#include "hw/misc/pattern_gen_regs.h"

struct IspPatternGenModule
{
    struct IspModule isp_module;

    QEMUTimer* fs_timer;
    QEMUTimer* fe_timer;
    uint32_t need_stop;

    char* pattern_cache;
    int pattern_cache_size;
};

struct IspPatternGenState {
    SysBusDevice parent_obj;
    struct IspPatternGenModule pattern_gen_state;
};

/**
 * @brief register helper macros
 * 
 */

#define REG_WRITE(ptg_mod, offset, val) do { (ptg_mod)->isp_module.ui.regs[offset] = (val); } while(0)

#define SET_STATE(ptg_mod, status) do { (ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_OUT_STATUS] = (status); } while(0)
#define GET_STATE(ptg_mod) (ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_OUT_STATUS]
#define GET_PLANE0_ADDR(ptg_mod) MAKE64((ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_OUT_ADDR_HI_0], (ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_OUT_ADDR_LO_0])
#define GET_PLANE1_ADDR(ptg_mod) MAKE64((ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_OUT_ADDR_HI_1], (ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_OUT_ADDR_LO_1])
#define GET_FORMAT(ptg_mod) (ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_OUT_FORMAT]
#define GET_WIDTH(ptg_mod)  (ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_OUT_WIDTH]
#define GET_HEIGHT(ptg_mod) (ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_OUT_HEIGHT]
#define GET_VVALID_DURATION(ptg_mod) (ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_VVLAID_DURATION]
#define GET_VBLANK_DURATION(ptg_mod) (ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_VBLANK_DURATION]

#define GET_IRQ_STATUS_PTR(ptg_mod) (struct PatternGenIRQ*)((ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_IRQ_REG])
#define GET_IRQ_STATUS_VAL(ptg_mod) (ptg_mod)->isp_module.ui.regs[ISP_PATTERN_GEN_IRQ_REG]

#define MIN_INTERVAL_US (8 * 1000)

#define TYPE_ISP_PATTERN_GEN "IspPatternGen"
OBJECT_DECLARE_SIMPLE_TYPE(IspPatternGenState, IspPatternGen)


static int ptg_init_pt_cache(struct IspPatternGenModule* ptg_mod)
{
    uint32_t w = GET_WIDTH(ptg_mod);
    uint32_t h = GET_HEIGHT(ptg_mod);
    uint32_t f = GET_FORMAT(ptg_mod);

    uint32_t cache_size = 0;

    if (ptg_mod->pattern_cache)
    {
        return -1; 
    }

    switch (f)
    {
    case PIX_FMT_SBGGR8:
        cache_size = w * h;
        break;

    default:
        return -1;
        break;
    }

    if (cache_size)
    {
        ptg_mod->pattern_cache = malloc(cache_size);
        if (!ptg_mod->pattern_cache)
        {
            ISP_LOGE("failed to alloc cache\n");
            return -1;
        }
    }

    memset(ptg_mod->pattern_cache, 0xDE, cache_size);
    ptg_mod->pattern_cache_size = cache_size;

    return 0;
}

static void ptg_deinit_pt_cache(struct IspPatternGenModule* ptg_mod)
{
    free(ptg_mod->pattern_cache);
    ptg_mod->pattern_cache = NULL;
}

static void ptg_exec_cycle_timer_fs(void *opaque)
{
    struct IspPatternGenModule* ptg_mod = (struct IspPatternGenModule *)opaque;
    assert(ptg_mod);

    if (GET_STATE(ptg_mod) != PTG_VBLANK)
    {
        assert(0);
    }

    if (ptg_mod->need_stop)
    {
        SET_STATE(ptg_mod, PTG_IDEL);
        ptg_deinit_pt_cache(ptg_mod);
        return;
    }

    SET_STATE(ptg_mod, PTG_VVALID);

    GET_IRQ_STATUS_PTR(ptg_mod)->fs = 1;
    virt_isp_set_irq(ptg_mod, GET_IRQ_STATUS_VAL(ptg_mod));

    timer_mod(ptg_mod->fe_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + GET_VVALID_DURATION(ptg_mod));

    ///< the dma flushing should be finished before the FrameEnd occur
    int ret = virt_isp_dma_write(&ptg_mod->isp_module, 0, ptg_mod->pattern_cache, ptg_mod->pattern_cache_size, 0);
    if (ret)
    {
        ISP_LOGE("error while writing dma\n");
    }
}

static void ptg_exec_cycle_timer_fe(void *opaque)
{
    struct IspPatternGenModule* ptg_mod = (struct IspPatternGenModule *)opaque;
    assert(ptg_mod);

    if (GET_STATE(ptg_mod) != PTG_VVALID)
    {
        assert(0);
    }

    IspDMAStateMachine dma_state = virt_isp_dma_get_state(&ptg_mod->isp_module, 0);
    if (DMA_IDLE != dma_state)
    {
        ISP_LOGE("error dma state %d\n", dma_state);
    }

    if (ptg_mod->need_stop)
    {
        SET_STATE(ptg_mod, PTG_IDEL);
        ptg_deinit_pt_cache(ptg_mod);
        return;
    }

    SET_STATE(ptg_mod, PTG_VBLANK);

    GET_IRQ_STATUS_PTR(ptg_mod)->fe = 1;
    virt_isp_set_irq(ptg_mod, GET_IRQ_STATUS_VAL(ptg_mod));

    timer_mod(ptg_mod->fs_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + GET_VBLANK_DURATION(ptg_mod));
}

static uint32_t ptg_reg_read(struct IspModule *isp_mod, uint32_t addr)
{
    return isp_mod->ui.regs[addr];
}

static int ptg_reg_write(struct IspModule *isp_mod, uint32_t addr, uint32_t data)
{
    struct IspPatternGenModule* ptg_mod = container_of(isp_mod, struct IspPatternGenModule, isp_module);
    int ret = 0;
    switch (addr)
    {
    case ISP_PATTERN_GEN_EN:
        if (data)
        {
            ret = IsDmaConfigValid(isp_mod, 0, 1);
            if ((GET_STATE(ptg_mod) != PTG_IDEL)
                || (0 != ret))
            {
                ISP_LOGE("bad status\n");
                return -1;
            }
            if (!ptg_mod->fs_timer)
            {
                ptg_mod->fs_timer = timer_new_us(QEMU_CLOCK_VIRTUAL, ptg_exec_cycle_timer_fs, ptg_mod);
                if (!ptg_mod->fs_timer)
                {
                    ISP_LOGE("failed to create frame start timer\n");
                    return -1;
                }
            }
            if (!ptg_mod->fe_timer)
            {
                ptg_mod->fe_timer = timer_new_us(QEMU_CLOCK_VIRTUAL, ptg_exec_cycle_timer_fe, ptg_mod);
                if (!ptg_mod->fe_timer)
                {
                    ISP_LOGE("failed to create frame end timer\n");
                    return -1;
                }
            }
            ret = ptg_init_pt_cache(ptg_mod);
            if (ret)
            {
                return ret;
            }
            if (GET_STATE(ptg_mod) == PTG_IDEL)
            {
                SET_STATE(ptg_mod, PTG_VBLANK);
                timer_mod(ptg_mod->fs_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + GET_VBLANK_DURATION(ptg_mod));
            }            
        }
        else
        {
            if (GET_STATE(ptg_mod) != PTG_IDEL)
            {
                ptg_mod->need_stop = 1;
            }
            else
            {
                ptg_deinit_pt_cache(ptg_mod);
            }
        }
        break;

    case ISP_PATTERN_GEN_OUT_ADDR_LO_0:
    case ISP_PATTERN_GEN_OUT_ADDR_LO_1:
    case ISP_PATTERN_GEN_OUT_ADDR_HI_0:
    case ISP_PATTERN_GEN_OUT_ADDR_HI_1:
    case ISP_PATTERN_GEN_OUT_FORMAT:
    case ISP_PATTERN_GEN_OUT_WIDTH:
    case ISP_PATTERN_GEN_OUT_HEIGHT:
        break;

    default:
        return -1;
    }
    return 0;
}

static struct IspIpOps pattern_gen_ops = {
    .reg_read = ptg_reg_read;
    .reg_write = ptg_reg_write;
};

static void pattern_gen_module_reset(struct IspPatternGenModule* ptg_mod)
{
    SET_STATE(ptg_mod, PTG_IDEL);

    ptg_mod->interval_us = 0;
    ptg_mod->need_stop = 0;
}

static void pattern_gen_realize(DeviceState *dev, Error **errp)
{
    IspPatternGenState *s = IspPatternGen(dev);

    virt_isp_module_init(dev, OBJECT(s), &s->pattern_gen_state.isp_module, &pattern_gen_ops);

    // IspPatternGenState->pattern_gen_state.timer = timer_new_us(QEMU_CLOCK_VIRTUAL, ptg_exec_cycle_timer, &IspPatternGenState->pattern_gen_state);
    // assert(IspPatternGenState->pattern_gen_state.timer);

    pattern_gen_module_reset(&IspPatternGenState->pattern_gen_state);
}

static void pattern_gen_reset(DeviceState *dev)
{
}

static void pattern_gen_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = pattern_gen_realize;
    dc->reset = pattern_gen_reset;
    dc->vmsd = NULL;
}

static const TypeInfo pattern_gen_type_info = {
    .name           = TYPE_ISP_PATTERN_GEN,
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(IspPatternGenState),
    .class_init      = pattern_gen_class_init,
};

static void pattern_gen_register_types(void)
{
    type_register_static(&pattern_gen_type_info);
}

type_init(pattern_gen_register_types)