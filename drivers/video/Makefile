# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2000-2007
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.

ifdef CONFIG_DM
obj-$(CONFIG_BACKLIGHT) += backlight-uclass.o
obj-$(CONFIG_BACKLIGHT_GPIO) += backlight_gpio.o
obj-$(CONFIG_BACKLIGHT_PWM) += pwm_backlight.o
obj-$(CONFIG_CONSOLE_NORMAL) += console_normal.o
obj-$(CONFIG_CONSOLE_ROTATION) += console_rotate.o
obj-$(CONFIG_CONSOLE_TRUETYPE) += console_truetype.o fonts/
obj-$(CONFIG_DISPLAY) += display-uclass.o
obj-$(CONFIG_VIDEO_MIPI_DSI) += dsi-host-uclass.o
obj-$(CONFIG_DM_VIDEO) += video-uclass.o vidconsole-uclass.o
obj-$(CONFIG_DM_VIDEO) += video_bmp.o
obj-$(CONFIG_PANEL) += panel-uclass.o
obj-$(CONFIG_PANEL_HX8238D) += hx8238d.o
obj-$(CONFIG_SIMPLE_PANEL) += simple_panel.o
obj-$(CONFIG_VIDEO_LINK) += video_link.o
endif

obj-${CONFIG_EXYNOS_FB} += exynos/
obj-${CONFIG_VIDEO_ROCKCHIP} += rockchip/
obj-${CONFIG_VIDEO_STM32} += stm32/
obj-${CONFIG_VIDEO_TEGRA124} += tegra124/
obj-y += ti/

obj-$(CONFIG_ATI_RADEON_FB) += ati_radeon_fb.o videomodes.o
obj-$(CONFIG_ATMEL_HLCD) += atmel_hlcdfb.o
obj-$(CONFIG_ATMEL_LCD) += atmel_lcdfb.o
obj-$(CONFIG_CFB_CONSOLE) += cfb_console.o
obj-$(CONFIG_FORMIKE) += formike.o
obj-$(CONFIG_FSL_DIU_FB) += fsl_diu_fb.o videomodes.o
obj-$(CONFIG_IHS_VIDEO_OUT) += ihs_video_out.o
obj-$(CONFIG_LD9040) += ld9040.o
obj-$(CONFIG_LG4573) += lg4573.o
obj-$(CONFIG_LOGICORE_DP_TX) += logicore_dp_tx.o
obj-$(CONFIG_NXP_TDA19988) += tda19988.o
obj-$(CONFIG_OSD) += video_osd-uclass.o
obj-$(CONFIG_PXA_LCD) += pxa_lcd.o
obj-$(CONFIG_SANDBOX_OSD) += sandbox_osd.o
obj-$(CONFIG_S6E8AX0) += s6e8ax0.o
obj-$(CONFIG_SCF0403_LCD) += scf0403_lcd.o
obj-$(CONFIG_S3C64XX_XLCD) += s3c64xx_xlcd.o
obj-$(CONFIG_VIDEO_ARM_MALIDP) += mali_dp.o
obj-$(CONFIG_VIDEO_BCM2835) += bcm2835.o
obj-$(CONFIG_VIDEO_BROADWELL_IGD) += broadwell_igd.o
obj-$(CONFIG_VIDEO_COREBOOT) += coreboot.o
obj-$(CONFIG_VIDEO_DA8XX) += da8xx-fb.o videomodes.o
obj-$(CONFIG_VIDEO_DW_HDMI) += dw_hdmi.o
obj-$(CONFIG_VIDEO_DW_MIPI_DSI) += dw_mipi_dsi.o
obj-$(CONFIG_VIDEO_EFI) += efi.o
obj-$(CONFIG_VIDEO_FSL_DCU_FB) += fsl_dcu_fb.o videomodes.o
obj-$(CONFIG_VIDEO_IVYBRIDGE_IGD) += ivybridge_igd.o
obj-$(CONFIG_VIDEO_LCD_ANX9804) += anx9804.o
obj-$(CONFIG_VIDEO_LCD_HITACHI_TX18D42VM) += hitachi_tx18d42vm_lcd.o
obj-$(CONFIG_VIDEO_LCD_ORISETECH_OTM8009A) += orisetech_otm8009a.o
obj-$(CONFIG_VIDEO_LCD_RAYDIUM_RM68200) += raydium-rm68200.o
obj-$(CONFIG_VIDEO_LCD_RAYDIUM_RM67191) += raydium-rm67191.o
obj-$(CONFIG_VIDEO_LCD_ROCKTECH_HIMAX8394F) += rocktech-hx8394f.o
obj-$(CONFIG_VIDEO_LCD_SSD2828) += ssd2828.o
obj-$(CONFIG_VIDEO_LCD_TDO_TL070WSH30) += tdo-tl070wsh30.o
obj-$(CONFIG_VIDEO_LCD_XINLI_TCXD078IBLMA_98) += xinli-tcxd078iblma-98.o
obj-$(CONFIG_VIDEO_ADV7535) += adv7535.o
obj-${CONFIG_VIDEO_MESON} += meson/
obj-${CONFIG_VIDEO_MIPI_DSI} += mipi_dsi.o
obj-$(CONFIG_VIDEO_IT6263_BRIDGE) += it6263_bridge.o
obj-$(CONFIG_VIDEO_MVEBU) += mvebu_lcd.o
obj-$(CONFIG_VIDEO_MX3) += mx3fb.o videomodes.o
obj-$(CONFIG_VIDEO_MXS) += mxsfb.o videomodes.o
obj-$(CONFIG_VIDEO_NX) += nexell_display.o videomodes.o nexell/
obj-$(CONFIG_VIDEO_OMAP3) += omap3_dss.o
obj-$(CONFIG_VIDEO_DSI_HOST_SANDBOX) += sandbox_dsi_host.o
obj-$(CONFIG_VIDEO_SANDBOX_SDL) += sandbox_sdl.o
obj-$(CONFIG_VIDEO_SIMPLE) += simplefb.o
obj-$(CONFIG_VIDEO_TEGRA20) += tegra.o
obj-$(CONFIG_VIDEO_VCXK) += bus_vcxk.o
obj-$(CONFIG_VIDEO_VESA) += vesa.o
obj-$(CONFIG_VIDEO_SEPS525) += seps525.o
obj-$(CONFIG_MXC_EPDC) += mxc_epdc_fb.o
obj-$(CONFIG_VIDEO_VADC) += mxc_vadc.o
obj-$(CONFIG_VIDEO_CSI) += mxc_csi.o
obj-$(CONFIG_VIDEO_PXP) += mxc_pxp.o
obj-$(CONFIG_VIDEO_GIS) += mxc_gis.o
obj-$(CONFIG_NT35521) += nt35521.o

obj-y += bridge/
obj-y += sunxi/
obj-y += nxp/
