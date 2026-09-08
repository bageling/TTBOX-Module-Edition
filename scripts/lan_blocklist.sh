#!/usr/bin/env bash
# TTBOX 局域网黑名单管理（iptables）
# 隔离设计：使用独立 chain TTBOX_BLOCKLIST，绝不触碰 YU 的 AIASSISTANCE_BLOCKLIST。
# 用法: lan_blocklist.sh {status|set <ip>|clear|delete <ip>}
set -u
CHAIN="TTBOX_BLOCKLIST"
IPTABLES="iptables"

ensure_chain() {
    "$IPTABLES" -N "$CHAIN" 2>/dev/null || true
    # INPUT 链只挂一次跳转（nft 后端 -C 匹配成功会回显规则行，须吞 stdout）
    if ! "$IPTABLES" -C INPUT -j "$CHAIN" >/dev/null 2>&1; then
        "$IPTABLES" -I INPUT -j "$CHAIN" 2>/dev/null || true
    fi
}

status() {
    ensure_chain
    # 从 chain 规则提取已拉黑 IP（-A TTBOX_BLOCKLIST -s <ip>/32 -j DROP）
    local ips
    ips=$("$IPTABLES" -S "$CHAIN" 2>/dev/null | grep "^-A" | sed -n 's/.*-s \([0-9.]*\)\/32.*/\1/p')
    local msg="未设置局域网黑名单"
    if [ -n "$ips" ]; then
        msg="已拉黑 $(echo "$ips" | wc -l) 个局域网设备"
    fi
    # 输出 JSON（blocked_ips 数组）
    local json_ips="[]"
    if [ -n "$ips" ]; then
        json_ips=$(printf '%s\n' "$ips" | sed 's/^/"/; s/$/"/' | paste -sd, - | sed 's/^/[/; s/$/]/')
    fi
    printf '{"blocked_ips":%s,"chain":"%s","firewall_supported":true,"message":"%s","supported":true}\n' \
        "$json_ips" "$CHAIN" "$msg"
}

set_ip() {
    local ip="$1"
    ensure_chain
    # 已存在则幂等返回（-C 回显规则行须吞 stdout）
    if "$IPTABLES" -C "$CHAIN" -s "$ip" -j DROP >/dev/null 2>&1; then
        printf '{"message":"已在黑名单","ip":"%s","ok":true}\n' "$ip"
        return 0
    fi
    "$IPTABLES" -A "$CHAIN" -s "$ip" -j DROP 2>/dev/null
    printf '{"message":"已拉黑","ip":"%s","ok":true}\n' "$ip"
}

delete_ip() {
    local ip="$1"
    "$IPTABLES" -D "$CHAIN" -s "$ip" -j DROP 2>/dev/null || true
    printf '{"message":"已解除拉黑","ip":"%s","ok":true}\n' "$ip"
}

clear_all() {
    ensure_chain
    "$IPTABLES" -F "$CHAIN" 2>/dev/null || true
    # 移除 INPUT 跳转（保留 chain 定义）
    "$IPTABLES" -D INPUT -j "$CHAIN" 2>/dev/null || true
    printf '{"message":"已清空局域网黑名单","ok":true}\n'
}

case "${1:-status}" in
    status) status ;;
    set) [ -n "${2:-}" ] && set_ip "$2" || { echo '{"error":"ip required","ok":false}'; exit 1; } ;;
    delete) [ -n "${2:-}" ] && delete_ip "$2" || { echo '{"error":"ip required","ok":false}'; exit 1; } ;;
    clear) clear_all ;;
    *) echo "usage: $0 {status|set <ip>|delete <ip>|clear}" >&2; exit 2 ;;
esac
