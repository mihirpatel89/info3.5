#
# Makefile for the video capture/playback device drivers.
#

GCOV_PROFILE := y

obj-$(CONFIG_VIDEO_VINO) += indycam.o
obj-$(CONFIG_VIDEO_VINO) += vino.o

obj-$(CONFIG_VIDEO_TIMBERDALE)	+= timblogiw.o
obj-$(CONFIG_VIDEO_M32R_AR_M64278) += arv.o

obj-$(CONFIG_VIDEO_VIA_CAMERA) += via-camera.o
obj-$(CONFIG_VIDEO_CAFE_CCIC) += marvell-ccic/
obj-$(CONFIG_VIDEO_MMP_CAMERA) += marvell-ccic/

obj-$(CONFIG_VIDEO_OMAP3)	+= omap3isp/

obj-$(CONFIG_VIDEO_VIU) += fsl-viu.o

obj-$(CONFIG_VIDEO_VIVID)		+= vivid/
obj-$(CONFIG_VIDEO_MEM2MEM_TESTDEV) += mem2mem_testdev.o

obj-$(CONFIG_VIDEO_TI_VPE)		+= ti-vpe/

obj-$(CONFIG_VIDEO_MX2_EMMAPRP)		+= mx2_emmaprp.o
obj-$(CONFIG_VIDEO_CODA) 		+= coda/

obj-$(CONFIG_VIDEO_SH_VEU)		+= sh_veu.o

obj-$(CONFIG_VIDEO_MEM2MEM_DEINTERLACE)	+= m2m-deinterlace.o

obj-$(CONFIG_VIDEO_S3C_CAMIF) 		+= s3c-camif/
obj-$(CONFIG_VIDEO_SAMSUNG_EXYNOS4_IS) 	+= exynos4-is/
obj-$(CONFIG_VIDEO_SAMSUNG_S5P_JPEG)	+= s5p-jpeg/
obj-$(CONFIG_VIDEO_SAMSUNG_S5P_MFC)	+= s5p-mfc/
obj-$(CONFIG_VIDEO_SAMSUNG_S5P_TV)	+= s5p-tv/

obj-$(CONFIG_VIDEO_SAMSUNG_S5P_G2D)	+= s5p-g2d/
obj-$(CONFIG_VIDEO_SAMSUNG_EXYNOS_GSC)	+= exynos-gsc/

obj-$(CONFIG_BLACKFIN)                  += blackfin/

obj-$(CONFIG_ARCH_DAVINCI)		+= davinci/

obj-$(CONFIG_VIDEO_SH_VOU)		+= sh_vou.o

obj-$(CONFIG_SOC_CAMERA)		+= soc_camera/

obj-$(CONFIG_VIDEO_RENESAS_VSP1)	+= vsp1/

obj-y	+= omap/
obj-$(CONFIG_PLATFORM_TEGRA) += tegra/

ccflags-y += -I$(srctree)/drivers/media/i2c
ccflags-y += -I$(srctree)/../nvhost/drivers/video/tegra/host
