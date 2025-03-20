################################################################################
#
# WS281X
#
################################################################################

WS281X_SITE = $(TOPDIR)/package/ws281x/src
WS281X_SITE_METHOD = local
WS281X_LICENSE = MIT
WS281X_LICENSE_FILES = COPYING

define WS281X_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE1) CC=$(TARGET_CC) -C $(@D)/
endef

define WS281X_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/ws281x_test $(TARGET_DIR)/usr/bin/ws281x_test
endef

$(eval $(generic-package))
