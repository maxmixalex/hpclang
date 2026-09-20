#ifndef HPCLANG_ARMTOOLBOX_HP39_PORTAL_PRIVATE_H
#define HPCLANG_ARMTOOLBOX_HP39_PORTAL_PRIVATE_H

#define HPCLANG_HP39_PORTAL_MAGIC_VALUE 0x39335048

#define HP39P_MAGIC          0
#define HP39P_FONT_WINDOW    4
#define HP39P_ENTRY          8
#define HP39P_SATURN_STATE   12
#define HP39P_EMULATOR_LR    16
#define HP39P_SATURN_ENTRY   20
#define HP39P_ARMSYS_TARGET  24
#define HP39P_STOCK_CPSR     28
#define HP39P_L1             32
#define HP39P_L2_WORKSPACE   36
#define HP39P_STOCK_DATA_L1  40
#define HP39P_STOCK_CODE_L1  44
#define HP39P_HPC_DATA_L1    48
#define HP39P_HPC_CODE_L1    52
#define HP39P_STOCK_FRAME    56
#define HP39P_STOCK_SP       60
#define HP39P_STOCK_REGS     64
#define HP39P_APP_SP         120
#define HP39P_APP_LR         124
#define HP39P_APP_REGS       128
#define HP39P_ACTIVE         160
#define HP39P_TRIPS          164
#define HP39P_LAST_CPSR      168
#define HP39P_EMULATOR_REGS  172
#define HP39P_PRE_DSKTOP      204
#define HP39P_RESULT_OBJECT   208
#define HP39P_RESULT_PROLOGUE 212
#define HP39P_APP_RESULT_OBJ  216
#define HP39P_APP_RESULT_PRO  220
#define HP39P_REQUEST_KIND      224
#define HP39P_REQUEST_NAME      228
#define HP39P_REQUEST_NAME_LEN  232
#define HP39P_EXPECTED_PROLOGUE 236
#define HP39P_STACK             240
#define HP39P_STACK_SIZE        256
#define HP39P_STACK_END         496

#define HP39P_CATALOG_APLET    1
#define HP39P_CATALOG_PROGRAM  2
#define HP39P_CATALOG_NOTE     3
#define HP39P_BODY_PROGRAM     4
#define HP39P_BODY_NOTE        5
#define HP39P_VARIABLE_SCHEMA  6

#ifndef __ASSEMBLER__

#include <stddef.h>
#include <stdint.h>
#include <hpclang/bootstrap.h>

struct hpclang_hp39_portal_context {
    struct hpclang_hp39_platform_context public_context;
    uintptr_t saturn_state;
    uintptr_t emulator_lr;
    uint32_t saturn_entry;
    uint32_t armsys_target;
    uint32_t stock_cpsr;
    uintptr_t l1;
    uintptr_t l2_workspace;
    uint32_t stock_data_l1;
    uint32_t stock_code_l1;
    uint32_t hpc_data_l1;
    uint32_t hpc_code_l1;
    uintptr_t stock_frame;
    uintptr_t stock_sp;
    uint32_t stock_regs[14];
    uintptr_t app_sp;
    uintptr_t app_lr;
    uint32_t app_regs[8];
    uint32_t active;
    uint32_t trips;
    uint32_t last_cpsr;
    uint32_t emulator_regs[8];
    uint32_t pre_dsktop;
    uint32_t result_object;
    uint32_t result_prologue;
    uintptr_t app_result_object;
    uintptr_t app_result_prologue;
    uint32_t request_kind;
    uintptr_t request_name;
    uint32_t request_name_length;
    uint32_t expected_prologue;
    uint8_t portal_stack[HP39P_STACK_SIZE];
};

_Static_assert(
    HPCLANG_HP39_PLATFORM_MAGIC == HPCLANG_HP39_PORTAL_MAGIC_VALUE,
    "HP39 portal magic must match runtime cookie");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, public_context) == HP39P_MAGIC, "HP39 portal public prefix");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, saturn_state) == HP39P_SATURN_STATE, "HP39 portal Saturn state");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, stock_regs) == HP39P_STOCK_REGS, "HP39 portal Stock frame");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, app_sp) == HP39P_APP_SP, "HP39 portal application SP");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, app_regs) == HP39P_APP_REGS, "HP39 portal application registers");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, emulator_regs) == HP39P_EMULATOR_REGS, "HP39 portal emulator registers");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, pre_dsktop) == HP39P_PRE_DSKTOP, "HP39 portal pre-DSKTOP");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, result_object) == HP39P_RESULT_OBJECT, "HP39 portal result object");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, result_prologue) == HP39P_RESULT_PROLOGUE, "HP39 portal result prologue");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, app_result_object) == HP39P_APP_RESULT_OBJ, "HP39 portal app result object");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, app_result_prologue) == HP39P_APP_RESULT_PRO, "HP39 portal app result prologue");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, request_kind) == HP39P_REQUEST_KIND, "HP39 portal request kind");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, request_name) == HP39P_REQUEST_NAME, "HP39 portal request name");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, request_name_length) == HP39P_REQUEST_NAME_LEN, "HP39 portal request name length");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, expected_prologue) == HP39P_EXPECTED_PROLOGUE, "HP39 portal expected prologue");
_Static_assert(offsetof(struct hpclang_hp39_portal_context, portal_stack) == HP39P_STACK, "HP39 portal stack");
_Static_assert(sizeof(struct hpclang_hp39_portal_context) == HP39P_STACK_END, "HP39 portal context size");

extern struct hpclang_hp39_portal_context hpclang_hp39_portal_context;
int hpclang_hp39_portal_enter(void);
void hpclang_hp39_portal_resume(void);
int hpclang_hp39_portal_prepare_stock(void);
int hpclang_hp39_portal_restore_hpc(void);

#endif
#endif
