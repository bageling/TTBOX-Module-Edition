#!/bin/sh
# run-ttbox-usb-proxy.sh — TTBOX usb-proxy 启动脚本（自研）
set -eu

USB_PROXY_DEVICE=${USB_PROXY_DEVICE:-fc000000.usb}
USB_PROXY_DRIVER=${USB_PROXY_DRIVER:-dwc3-gadget}
USB_PROXY_WAIT_SECONDS=${USB_PROXY_WAIT_SECONDS:-1}
USB_PROXY_EXTRA_ARGS=${USB_PROXY_EXTRA_ARGS:-}
USB_PROXY_MODE=${USB_PROXY_MODE:-full}   # full | synthetic
USB_PROXY_SOCKET_DIR=${USB_PROXY_SOCKET_DIR:-/run/ttbox-mouse-passthrough}

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

stop_conflicting_services()
{
	# TTBOX 独占 UDC：把板端其它 USB 透传服务先停掉，避免和 raw-gadget 抢控制器。
	for unit in usb-proxy.service usb-proxy-test.service mouse-passthrough.service \
		opi-mouse-gadget.service usbdevice.service; do
		if systemctl is-active --quiet "$unit" 2>/dev/null; then
			systemctl stop "$unit" >/dev/null 2>&1 || true
			printf 'Stopped conflicting service %s\n' "$unit"
		fi
	done
}

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
mkdir -p "$USB_PROXY_SOCKET_DIR"
stop_conflicting_services

while [ ! -e "/sys/class/udc/$USB_PROXY_DEVICE" ]; do
	printf 'Waiting for USB device controller %s...\n' "$USB_PROXY_DEVICE"
	sleep "$USB_PROXY_WAIT_SECONDS"
done

ARGS="--device=$USB_PROXY_DEVICE --driver=$USB_PROXY_DRIVER"
ARGS="$ARGS --mouse_control_cmd_socket=$USB_PROXY_SOCKET_DIR/cmd.sock"
ARGS="$ARGS --mouse_control_event_socket=$USB_PROXY_SOCKET_DIR/event.sock"

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
