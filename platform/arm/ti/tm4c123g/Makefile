MK_OBJS=tm4c123g.o interrupts.o

ifneq ($(CONFIG_GCC),)
  BSP_SRCS:=gpio.c $(if $(CONFIG_USB),usb.c)
  MK_LINKS:=$(addprefix $(TI_ARM_MCU_BSP_DIR)/driverlib/,$(BSP_SRCS))
  MK_OBJS+=entry_gcc.o $(BSP_SRCS:%.c=%.o)

  LINKER_SCRIPT:=$(BUILD)/$(d)/tm4c123g.ld
  CFLAGS+=-mthumb -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -O3 \
    -ffunction-sections -fdata-sections -I$(TI_ARM_MCU_BSP_DIR) \
    -DPART_TM4C123GH6PM -DTARGET_IS_TM4C123_RB1 \
    -Dgcc -fsingle-precision-constant
  LDFLAGS+=--entry reset_isr --gc-sections --nostdlib
  LIBS+=-L$(call get_libc_dir) -lm -lc -L$(call get_libgcc_dir) -lgcc
endif
