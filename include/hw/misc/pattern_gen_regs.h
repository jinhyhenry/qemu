#ifndef ISP_PATTERN_GEN_REG_H
#define ISP_PATTERN_GEN_REG_H

#define ISP_PATTERN_GEN_EN 0x00

#define ISP_PATTERN_GEN_OUT_ADDR_LO_0 0x08
#define ISP_PATTERN_GEN_OUT_ADDR_HI_0 0x0C

#define ISP_PATTERN_GEN_OUT_ADDR_LO_1 0x10
#define ISP_PATTERN_GEN_OUT_ADDR_HI_1 0x14

#define ISP_PATTERN_GEN_OUT_FORMAT 0x18

#define ISP_PATTERN_GEN_OUT_WIDTH 0x1C
#define ISP_PATTERN_GEN_OUT_HEIGHT 0x20

#define ISP_PATTERN_GEN_OUT_STATUS 0x24

#define ISP_PATTERN_GEN_VVLAID_DURATION 0x28
#define ISP_PATTERN_GEN_VBLANK_DURATION 0x2C

#define ISP_PATTERN_GEN_IRQ_REG 0x30

/**
 * @brief value enum of ISP_PATTERN_GEN_OUT_STATUS
 * 
 */
enum PatternGenStateMachine
{
    PTG_IDEL = 0,
    PTG_VVALID,
    PTG_VBLANK,
};

struct PatternGenIRQ
{
    uint32_t fs : 1;
    uint32_t fe : 1;
    uint32_t error  : 1;
    uint32_t reserved : 29;
};

/**
 * @brief value enum of ISP_PATTERN_GEN_OUT_FORMAT
 *
 */
enum PatternGenFormat
{
    PIX_FMT_SBGGR8 = 0
};



#endif