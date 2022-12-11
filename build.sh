#!/usr/bin/env bash

# This is a script to build an android kernel and push it to a telegram channel
# It is meant to be run on a CI server, but can be run locally as well
# It requires the following environment variables to be set:
# - TOKEN: The telegram bot token
# - CHAT_ID: The chat id to send the message to

# Exit on error
set -e

KERNEL_DIR="${PWD}"
cd "${KERNEL_DIR}"
CHEAD="$(git rev-parse --short HEAD)"
KERN_IMG="${KERNEL_DIR}"/out/arch/arm64/boot/Image.gz
KERN_DTB="${KERNEL_DIR}"/out/arch/arm64/boot/dtbo.img
ANYKERNEL="${HOME}"/anykernel

# Repo URL
ANYKERNEL_REPO="https://github.com/Blaster4385/anykernel.git"
ANYKERNEL_BRANCH="master"

# Repo info
PARSE_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
PARSE_ORIGIN="$(git config --get remote.origin.url)"
COMMIT_POINT="$(git log --pretty=format:'%h : %s' -1)"

# Setup Neutron Clang
mkdir -p "/mnt/workdir/neutron-clang"
CLANG_DIR="/mnt/workdir/neutron-clang"
cd "${CLANG_DIR}"

curl -LO "https://raw.githubusercontent.com/Neutron-Toolchains/antman/main/antman"
./antman -S

cd "${KERNEL_DIR}"

CSTRING=$("$CLANG_DIR"/bin/clang --version | head -n 1 | perl -pe 's/\(http.*?\)//gs' | sed -e 's/  */ /g' -e 's/[[:space:]]*$//')
COMP_PATH="$CLANG_DIR/bin:${PATH}"

# Defconfig
DEFCONFIG="vendor/kona-perf_defconfig"

# Telegram
CHATID="$CHANNEL_ID" # Group/channel chatid (use rose/userbot to get it)
TELEGRAM_TOKEN="${TG_TOKEN}"

# Export Telegram.sh
TELEGRAM_FOLDER="${HOME}"/telegram
if ! [ -d "${TELEGRAM_FOLDER}" ]; then
    git clone https://github.com/fabianonline/telegram.sh/ "${TELEGRAM_FOLDER}"
fi

TELEGRAM="${TELEGRAM_FOLDER}"/telegram
tg_cast() {
    "${TELEGRAM}" -t "${TELEGRAM_TOKEN}" -c "${CHATID}" -H \
    "$(
		for POST in "${@}"; do
			echo "${POST}"
		done
    )"
}
tg_ship() {
    "${TELEGRAM}" -f "${ZIPNAME}" -t "${TELEGRAM_TOKEN}" -c "${CHATID}" -H \
    "$(
                for POST in "${@}"; do
                        echo "${POST}"
                done
    )"
}

#Versioning
KERNEL="[TEST]IllusionX"
DEVICE="oneplus-sm8250"
KERNELTYPE="$CONFIG_LOCALVERSION"
KERNELNAME="${KERNEL}-${DEVICE}-${KERNELTYPE}-$(date +%y%m%d-%H%M)"
ZIPNAME="${KERNELNAME}.zip"

# Build Failed
build_failed() {
	    END=$(date +"%s")
	    DIFF=$(( END - START ))
	    echo -e "Kernel compilation failed, See buildlog to fix errors"
	    tg_cast "Build for ${DEVICE} <b>failed</b> in $((DIFF / 60)) minute(s) and $((DIFF % 60)) second(s)!"
	    exit 1
}

# Building
makekernel() {
    export PATH="${COMP_PATH}"
    make O=out ARCH=arm64 CC=clang ${DEFCONFIG}
    make -j$(nproc --all) CC=clang CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi- O=out ARCH=arm64 AS=llvm-as AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip LLVM=1 LLVM_IAS=1
    packingkernel
}

# Packing kranul
packingkernel() {
    # Copy compiled kernel
    if [ -d "${ANYKERNEL}" ]; then
        rm -rf "${ANYKERNEL}"
    fi
    git clone "$ANYKERNEL_REPO" -b "$ANYKERNEL_BRANCH" "${ANYKERNEL}"
    if ! [ -f "${KERN_IMG}" ]; then
        build_failed
    fi
    if ! [ -f "${KERN_DTB}" ]; then
        build_failed
    fi
    cp "${KERN_IMG}" "${ANYKERNEL}"/Image.gz-dtb
    cp "${KERN_DTB}" "${ANYKERNEL}"/dtbo.img


    # Zip the kernel, or fail
    cd "${ANYKERNEL}" || exit
    zip -r9 "${ZIPNAME}" ./*

    END=$(date +"%s")
    DIFF=$(( END - START ))

    # Ship it to the CI channel
    tg_ship "<b>-------- Build Succeeded --------</b>" \
            "" \
            "<b>Device:</b> ${DEVICE}" \
            "<b>Version:</b> ${KERNELTYPE}" \
            "<b>Commit Head:</b> ${CHEAD}" \
            "<b>Time elapsed:</b> $((DIFF / 60)):$((DIFF % 60))" \
            "" \
            "Leave a comment below if you encounter any bugs!"
}

# Starting
tg_cast "<b>$DRONE_BUILD_NUMBER CI Build Triggered</b>" \
        "Compiling with $(nproc --all) CPUs" \
	"" \
        "Compiler: <code>${CSTRING}</code>" \
	"Device: ${DEVICE}" \
	"Kernel: <code>${KERNEL}</code>" \
	"Linux Version: <code>$(make kernelversion)</code>" \
	"Branch: <code>${DRONE_BRANCH}</code>" \
	"Clocked at: <code>$(date +%Y%m%d-%H%M)</code>" \
	"Latest commit: <code>${COMMIT_POINT}</code>" \
	"Log: https://cloud.drone.io/Blaster4385/kernel_realme_RMX1851/$DRONE_BUILD_NUMBER"

START=$(date +"%s")
makekernel
