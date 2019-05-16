
define toolchain
$1: CC       := $(TOOLCHAIN)gcc
$1: LD       := $(TOOLCHAIN)ld
$1: AR       := $(TOOLCHAIN)ar
$1: OBJCOPY  := $(TOOLCHAIN)objcopy
$1: CC_LOCAL := gcc
endef

CC       := $(TOOLCHAIN)gcc
CXX      := $(TOOLCHAIN)g++
LD       := $(TOOLCHAIN)ld
AR       := $(TOOLCHAIN)ar
RANLIB   := $(TOOLCHAIN)ranlib
OBJCOPY  := $(TOOLCHAIN)objcopy

ifneq "$(V)" "1"
Q     := @
quiet := quiet_
else
Q     :=
quiet :=
endif

echo-cmd = $(if $($(quiet)cmd_$1),echo '  $($(quiet)cmd_$1)';)
cmd      = @$(call echo-cmd,$1,$2) $(cmd_$1)

# The -MMD flags additionaly creates a .d file with
# the same name as the .o file.
quiet_cmd_cc_o_c   = CC      $@
	cmd_cc_o_c = "$(CC)" -o "$@" $(CFLAGS) -MMD -c "$<"

quiet_cmd_cc_o_s   = AS      $@
	cmd_cc_o_s = "$(CC)" -o "$@" $(ASFLAGS) -c "$<"

quiet_cmd_ar       = AR      $@
	cmd_ar     = "$(AR)" rcs "$@" $(OBJECTS)

quiet_cmd_link     = LINK    $@
	cmd_link   = "$(CC)" -o "$@" "-Wl,-Map=$@.map" \
	$(OBJECTS) -Wl,--start-group $(LIBS) $(LDFLAGS) -Wl,--end-group

quiet_cmd_mkdir    = MKDIR   $@
	cmd_mkdir  = mkdir -p "$@"

quiet_cmd_rm       = RM      $2
	cmd_rm     = rm -rf "$2"

quiet_cmd_bin      = BIN     $@
	cmd_bin    = "$(OBJCOPY)" -O binary "$<" "$@"

quiet_cmd_ihex     = IHEX    $@
	cmd_ihex   = "$(OBJCOPY)" -O ihex "$<" "$@"
