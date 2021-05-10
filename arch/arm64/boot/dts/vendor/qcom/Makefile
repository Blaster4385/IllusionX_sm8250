ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY_NOT_OEM),y)
        dtbo-$(CONFIG_ARCH_KONA) += \
                kona-cdp-overlay.dtbo \
                kona-cdp-lcd-overlay.dtbo \
                kona-mtp-overlay.dtbo \
                kona-mtp-ws-overlay.dtbo \
		kona-sa-mtp-overlay.dtbo \
                kona-xr-overlay.dtbo \
                kona-rumi-overlay.dtbo \
                kona-qrd-overlay.dtbo \
		kona-xrfusion-overlay.dtbo \
		kona-xrfusion-ult-overlay.dtbo \
                kona-hdk-overlay.dtbo

kona-cdp-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-cdp-lcd-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-mtp-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-mtp-ws-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-sa-mtp-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-xr-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-rumi-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-qrd-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-xrfusion-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-xrfusion-ult-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-hdk-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
else
ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_KONA) += \
                instantnoodlep-overlay-evb.dtbo \
                instantnoodlep-overlay-t0.dtbo \
                instantnoodlep-overlay-evt1.dtbo \
                instantnoodlep-overlay-dvt.dtbo \
                instantnoodle-overlay-t0.dtbo \
                instantnoodle-overlay-evt1.dtbo \
                instantnoodle-overlay-dvt.dtbo \
                instantnoodlev-overlay-t0.dtbo \
                instantnoodlev-overlay-evt1.dtbo \
                instantnoodlev-overlay-dvt.dtbo \
                kebab-overlay.dtbo \
                lemonades-overlay.dtbo

instantnoodlep-overlay-evb.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
instantnoodlep-overlay-t0.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
instantnoodlep-overlay-evt1.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
instantnoodlep-overlay-dvt.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb

instantnoodle-overlay-t0.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
instantnoodle-overlay-evt1.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
instantnoodle-overlay-dvt.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb

instantnoodlev-overlay-t0.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
instantnoodlev-overlay-evt1.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
instantnoodlev-overlay-dvt.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb

kebab-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
lemonades-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
else
dtb-$(CONFIG_ARCH_KONA) += kona-rumi.dtb \
        kona-mtp.dtb \
        kona-mtp-ws.dtb \
        kona-mtp-sa.dtb \
        kona-xr.dtb \
	kona-xrfusion.dtb \
	kona-xrfusion-ult.dtb \
        kona-cdp.dtb \
        kona-cdp-lcd.dtb \
        kona-qrd.dtb \
        kona-v2-rumi.dtb \
        kona-v2-mtp.dtb \
        kona-v2-mtp-ws.dtb \
        kona-v2-mtp-sa.dtb \
        kona-v2-cdp.dtb \
        kona-v2-qrd.dtb \
	kona-v2-xrfusion.dtb \
	kona-v2-xrfusion-ult.dtb \
        kona-hdk.dtb \
        kona-v2.1-mtp.dtb \
        kona-v2.1-mtp-ws.dtb \
        kona-v2.1-mtp-sa.dtb \
        kona-v2.1-cdp.dtb \
        kona-v2.1-qrd.dtb \
        kona-v2.1-hdk.dtb \
	kona-v2.1-xrfusion.dtb \
	kona-v2.1-xrfusion-ult.dtb \
        qrb5165-iot-rb5.dtb \
        kona-v2.1-iot-rb5.dtb
endif
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_KONA) += \
		oneplus8-overlay.dtbo \
		oneplus8p-overlay.dtbo \
		oneplus8t-overlay.dtbo \
		oneplus8v-overlay.dtbo \
		oneplus9r-overlay.dtbo

oneplus8-overlay.dtbo-base := kona-v2.1.dtb
oneplus8p-overlay.dtbo-base := kona-v2.1.dtb
oneplus8t-overlay.dtbo-base := kona-v2.1.dtb
oneplus8v-overlay.dtbo-base := kona-v2.1.dtb
oneplus9r-overlay.dtbo-base := kona-v2.1.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files    := *.dtb *.dtbo
