TARGET = hypersawx
PLATFORM = minilogue-xd
SDK_DIR = ../../..

# Adjust this path to your ARM GCC Toolchain
CROSS = "C:/Program Files (x86)/GNU Arm Embedded Toolchain/12.3.rel1/bin/arm-none-eabi"

include $(SDK_DIR)/platform/$(PLATFORM)/makefile.mk
