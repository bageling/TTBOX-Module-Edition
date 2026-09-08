#!/bin/sh
# run-ttbox-usb-proxy.sh — TTBOX usb-proxy 启动脚本（自研）
set -eu

USB_PROXY_DEVICE=${USB_PROXY_DEVICE:-fc000000.usb}
USB_PROXY_DRIVER=${USB_PROXY_DRIVER:-dwc3-gadget}
USB_PROXY_WAIT_SECONDS=${USB_PROXY_WAIT_SECONDS:-1}
USB_PROXY_EXTRA_ARGS=${USB_PROXY_EXTRA_ARGS:-}
USB_PROXY_MODE=${USB_PROXY_MODE:-full}   # full | synthetic

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

find_mouse()
{
	for dev in /sys/bus/usb/devices/*; do
		[ -f "$dev/idVendor" ] || continue
		[ -f "$dev/idProduct" ] || continue

		for intf in "$dev":*; do
			[ -f "$intf/bInterfaceClass" ] || continue
			class=$(cat "$intf/bInterfaceClass")
			protocol=$(cat "$intf/bInterfaceProtocol" 2>/dev/null || printf '00')

			if [ "$class" = "03" ] && [ "$protocol" = "02" ]; then
				printf '%s %s\n' "$(cat "$dev/idVendor")" "$(cat "$dev/idProduct")"
				return 0
			fi
		done
	done

	return 1
}

cd "$PROJECT_DIR"

while [ ! -e "/sys/class/udc/$USB_PROXY_DEVICE" ]; do
	printf 'Waiting for USB device controller %s...\n' "$USB_PROXY_DEVICE"
	sleep "$USB_PROXY_WAIT_SECONDS"
done

ARGS="--device=$USB_PROXY_DEVICE --driver=$USB_PROXY_DRIVER"

if [ "$USB_PROXY_MODE" = "synthetic" ]; then
	printf 'TTBOX usb-proxy synthetic mode\n'
	ARGS="$ARGS --synthetic_mouse --enable_mouse_control"
else
	while ! ids=$(find_mouse); do
		printf 'Waiting for a USB HID mouse on the Orange Pi side...\n'
		sleep "$USB_PROXY_WAIT_SECONDS"
	done

	set -- $ids
	vendor_id=$1
	product_id=$2

	printf 'Using USB mouse %s:%s\n' "$vendor_id" "$product_id"
	ARGS="$ARGS --vendor_id=$vendor_id --product_id=$product_id --hid_passthrough_compat --enable_mouse_control"
fi

# shellcheck disable=SC2086
exec ./usb-proxy $ARGS $USB_PROXY_EXTRA_ARGS
