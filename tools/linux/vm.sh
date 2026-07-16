
make_iso() {
    local arch="$1"
    local config="$2"

    if [[ "$EUID" -ne 0 ]]; then
        echo "need sudo"
        exit 1
    fi

    if [[ ! -f alpine-virt-3.21.2-"$arch".iso ]]; then
        echo "missing alpine-virt-3.21.2-$arch.iso"
        exit 1
    fi

    local temp="temp"
    mkdir -p "$temp"
    trap "rm -rf \"$temp\"" EXIT

    create_file() {
        local owner="$1"
        local permissions="$2"
        local filename="$3"
        cat > "$filename"
        chown "$owner" "$filename"
        chmod "$permissions" "$filename"
    }

    mkdir -p "$temp"/etc/network
    create_file 0:0 0644 "$temp"/etc/network/interfaces <<EOF
auto lo
iface lo inet loopback
$VM_INTERFACES
EOF

    create_file 0:0 0644 "$temp"/etc/resolv.conf <<EOF
$(printf 'nameserver %s\n' $VM_NAMESERVERS)
EOF

    mkdir -p "$temp"/etc/iptables
    create_file 0:0 0644 "$temp"/etc/iptables/rules-save <<EOF
*filter
:INPUT ACCEPT [0:0]
:FORWARD ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
COMMIT
EOF
    create_file 0:0 0644 "$temp"/etc/iptables/rules6-save <<EOF
*filter
:INPUT ACCEPT [0:0]
:FORWARD ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
COMMIT
EOF

# -A INPUT -p tcp -m tcp --dport 22 -j ACCEPT
# -A INPUT -p tcp -m tcp --dport 443 -j ACCEPT

    mkdir -p "$temp"/etc/ssh
    create_file 0:0 0644 "$temp"/etc/ssh/sshd_config <<EOF
Port 22
PermitRootLogin prohibit-password
AuthorizedKeysFile .ssh/authorized_keys
AllowTcpForwarding no
GatewayPorts no
X11Forwarding no
Subsystem sftp internal-sftp
EOF

    create_file 0:0 0644 "$temp"/etc/fstab <<EOF
/dev/cdrom /media/cdrom iso9660 noauto,ro 0 0
/dev/usbdisk /media/usb vfat noauto,ro 0 0
EOF
    if [[ "$config" == "server" ]]; then
        cat >> "$temp"/etc/fstab <<EOF
/dev/sda1 /mnt/shared ext4 noauto,rw,relatime 0 0
EOF
    else
        cat >> "$temp"/etc/fstab <<EOF
shared /mnt/shared 9p trans=virtio,rw 0 0
EOF
    fi

    mkdir -p "$temp"/etc/lbu
    if [[ "$config" == "server" ]]; then
        create_file 0:0 0644 "$temp"/etc/lbu/lbu.conf <<EOF
LBU_BACKUPDIR=/mnt/shared
EOF
    else
        create_file 0:0 0644 "$temp"/etc/lbu/lbu.conf <<EOF
LBU_BACKUPDIR=/media/vda
EOF
    fi

    mkdir -p "$temp"/etc/apk
    create_file 0:0 0644 "$temp"/etc/apk/world <<EOF
alpine-base
openssh
openssl
EOF
    create_file 0:0 0644 "$temp"/etc/apk/repositories <<EOF
/media/vda/apks
EOF
    mkdir -p "$temp"/etc/apk/protected_paths.d
    create_file 0:0 0644 "$temp"/etc/apk/protected_paths.d/lbu.list <<EOF
+root
-root/.ash_history
+etc/init.d
-etc/apk/arch
-etc/apk/keys
-etc/apk/cache
EOF

    create_file 0:0 0644 "$temp"/etc/hostname <<EOF
$config
EOF

    create_file 0:0 0644 "$temp"/etc/motd <<EOF
EOF

    create_file 0:0 0644 "$temp"/etc/profile <<EOF
export PS1='\u@\h \w \$([[ "\$(id -u)" -eq 0 ]] && echo "#" || echo "%") '
alias ll="ls -la"
acme() {
    acme.sh --home /etc/acme "\$@"
}
apkr() {
    apk --repository http://dl-cdn.alpinelinux.org/alpine/v3.21/main --repository http://dl-cdn.alpinelinux.org/alpine/v3.21/community "\$@"
}
clear
EOF

    create_file 0:0 0644 "$temp"/etc/inittab <<EOF
::sysinit:/sbin/openrc sysinit
::sysinit:/sbin/openrc boot
::wait:/sbin/openrc default
::ctrlaltdel:/sbin/reboot
::shutdown:/sbin/openrc shutdown

tty1::respawn:/sbin/getty 38400 tty1
tty2::respawn:/sbin/getty 38400 tty2
tty3::respawn:/sbin/getty 38400 tty3
tty4::respawn:/sbin/getty 38400 tty4
tty5::respawn:/sbin/getty 38400 tty5
tty6::respawn:/sbin/getty 38400 tty6
ttyS0::respawn:/sbin/getty -L 115200 ttyS0 vt100
EOF

    mkdir -p "$temp"/etc/init.d
    create_file 0:0 0744 "$temp"/etc/init.d/custom-mount <<EOF
#!/sbin/openrc-run

depend() {
    before localmount
}

start() {
    mkdir -p /mnt/shared
}
EOF

    local packages

    if [[ "$config" == "server" ]]; then
        packages="iptables acme.sh"
    else
        packages="iptables vim mandoc mandoc-apropos docs less lldb"
    fi

    create_file 0:0 0744 "$temp"/etc/init.d/custom-setup <<EOF
#!/sbin/openrc-run

depend() {
    need net
}

start() {
    . /etc/profile
    if [[ -d "/mnt/shared/tools/linux" ]]; then
        local arch=\$(cat /etc/apk/arch)
        local dir=/mnt/shared/tools/linux/apks/"\$arch"
        mkdir -p "\$dir"
        ln -sf "\$dir" /etc/apk/cache
        local index_count=\$(find "\$dir" -name "APKINDEX*.tar.gz" | wc -l)
        if [[ "\$index_count" -ne 2 ]]; then
            apkr update
        fi
        apkr fetch -o "\$dir" -R $packages
        apkr add --allow-untrusted "\$dir"/*.apk
    else
        apkr update
        apkr add $packages
    fi
}
EOF

    mkdir -p "$temp"/root/.ssh
    create_file 0:0 0644 "$temp"/root/.ssh/authorized_keys <<EOF
$VM_AUTHORIZED_KEYS
EOF

    rc_add() {
        mkdir -p "$temp"/etc/runlevels/"$2"
        ln -sf /etc/init.d/"$1" "$temp"/etc/runlevels/"$2"/"$1"
    }

    rc_add devfs sysinit
    rc_add dmesg sysinit
    rc_add mdev sysinit
    rc_add hwdrivers sysinit
    rc_add modloop sysinit

    rc_add hwclock boot
    rc_add modules boot
    rc_add sysctl boot
    rc_add hostname boot
    rc_add bootmisc boot
    rc_add syslog boot
    rc_add networking boot
    rc_add sshd boot
    rc_add custom-mount boot
    rc_add custom-setup boot
    rc_add iptables boot
    rc_add ip6tables boot

    rc_add mount-ro shutdown
    rc_add killprocs shutdown
    rc_add savecache shutdown

    local iso_dir="iso"

    if [[ "$config" == "server" ]]; then
        local iso_dir+="/server"
    else
        local iso_dir+="/$arch"
    fi

    mkdir -p "$iso_dir"

    tar -C temp -c etc root | gzip -9n > "$iso_dir"/"$config".apkovl.tar.gz

    local console_args=""

    if [[ "$arch" == "x86_64" ]]; then
        console_args="console=ttyS0"
    fi

    create_file 0:0 0644 "$temp"/grub.cfg <<EOF
set timeout=0

menuentry "Linux" {
linux /boot/vmlinuz-virt modules=loop,squashfs,sd-mod,usb-storage $console_args
initrd /boot/initramfs-virt
}
EOF

    rm -f "$iso_dir"/alpine.iso
    xorriso \
-indev alpine-virt-3.21.2-"$arch".iso \
-outdev "$iso_dir"/alpine.iso \
-map "$iso_dir"/"$config".apkovl.tar.gz /"$config".apkovl.tar.gz \
-map temp/grub.cfg /boot/grub/grub.cfg \
-boot_image any replay
}

start_qemu() {
    local arch="$1"
    local debug="$2"

    local qemu_bin=$(dirname $(realpath $(which qemu-system-aarch64)))
    local qemu_share=$(realpath $qemu_bin/../share/qemu/)

    local qemu_efi_file="$qemu_share/edk2-$arch-code.fd"
    local qemu_iso="iso/$arch/alpine.iso"
    local qemu_exec="qemu-system-$arch"
    local qemu_flags

    if [[ "$arch" == "aarch64" ]]; then
        # qemu-aarch64 has a bug on m4, need to use cortex-a57
        if [[ $(sysctl -n machdep.cpu.brand_string) =~ .*M4.* ]]; then
            qemu_flags="-machine virt -cpu cortex-a57"
        else
            qemu_flags="-machine virt -cpu host -accel hvf"
        fi
    else
        qemu_flags="-machine q35"
    fi

    local qemu_cmd="\
$qemu_exec \
$qemu_flags \
-m 2048 \
-nographic \
-drive if=pflash,format=raw,readonly=on,file=$qemu_efi_file \
-drive if=virtio,format=raw,readonly=on,file=$qemu_iso \
-fsdev local,id=myfs0,path=..,security_model=none \
-device virtio-9p-pci,fsdev=myfs0,mount_tag=shared \
-netdev user,id=mynet0,hostfwd=tcp::2222-:22,hostfwd=tcp::8443-:443 \
-device e1000,netdev=mynet0 \
"

    if [[ "$2" == "debug" ]]; then
        $qemu_cmd
    else
        nohup $qemu_cmd > /dev/null 2> /dev/null &
    fi
}

set -e
if [[ "$1" == "arm" ]]; then
    arch="aarch64"
    config="$arch"
elif [[ "$1" == "x86" ]]; then
    arch="x86_64"
    config="$arch"
elif [[ "$1" == "server" ]]; then
    arch="x86_64"
    config="server"
else
    echo "invalid arch"
    exit 1
fi
if [[ "$config" == "server" ]]; then
    if [[ ! -f "$2" ]]; then
        echo "usage: vm.sh server <config>"
        exit 1
    fi
    . "$2"
    if [[ -z "$address" || -z "$gateway" || -z "$nameservers" || -z "$authorized_keys" ]]; then
        echo "$2 must set address, gateway, nameservers, and authorized_keys"
        exit 1
    fi
    interfaces="auto eth0
iface eth0 inet static
    address $address
    gateway $gateway"
    if [[ -n "$address6" ]]; then
        interfaces="$interfaces
iface eth0 inet6 static
    address $address6
    gateway $gateway6"
    fi
else
    interfaces="auto eth0
iface eth0 inet dhcp"
    nameservers="8.8.8.8"
    authorized_keys="$(cat "$HOME"/.ssh/*.pub 2> /dev/null)" || true
    if [[ -z "$authorized_keys" ]]; then
        echo "no public keys in $HOME/.ssh"
        exit 1
    fi
fi
sudo env VM_INTERFACES="$interfaces" VM_NAMESERVERS="$nameservers" VM_AUTHORIZED_KEYS="$authorized_keys" sh -$- -c "$(declare -f make_iso); make_iso $arch $config"
if [[ "$config" != "server" ]]; then
    pkill -f qemu || true
    start_qemu $arch $2
fi

# deploy script, needs different build configs
# use bearssl
# use xxd for file includes
