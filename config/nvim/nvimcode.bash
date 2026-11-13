# Keep the dedicated editor appearance; preserve shells with other work.
nvimcode() {
    if [[ -n ${NVIM:-} || -n $(jobs -p) ]]; then
        command nvimcode "$@"
        return
    fi
    if ! command -v foot >/dev/null || ! command -v nvim >/dev/null; then
        printf 'nvimcode: foot ou nvim não encontrado.\n' >&2
        return 1
    fi
    JLINUX_REPLACE_TERMINAL=1 exec "$HOME/.local/bin/nvimcode" "$@"
}
