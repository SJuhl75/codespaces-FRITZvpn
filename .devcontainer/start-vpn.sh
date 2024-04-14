#!/usr/bin/env bash
set -e

# Switch to the .devcontainer folder
cd "$( dirname "${BASH_SOURCE[0]}" )"

# Create a temporary directory
mkdir -p vpntmp
cd vpntmp

# Touch file to make sure this user can read it
touch openvpn.log

# If we are running as root, we do not need to use sudo
sudo_cmd=""
if [ "$(id -u)" != "0" ]; then
    sudo_cmd="sudo"
fi

if [ ! -z "${SSHD_PASS}" ]; then 
cat > sshp <<EOF
${SSHD_PASS}
${SSHD_PASS}
EOF
fi
SSH_USER=$(whoami)
echo "Updating Password for ${SSH_USER}" > pw-change.log
cat sshp | sudo passwd ${SSH_USER} >> pw-change.log 2>&1
cat pw-change.log
rm sshp

# Start up the VPN client using the config stored in vpnconfig.ovpn by save-config.sh
if [ "${VPNS}" == "ON" ]; then
    #nohup ${sudo_cmd} /bin/sh -c "openvpn --config vpnconfig.ovpn --log openvpn.log &" | tee openvpn-launch.log
    #nohup ${sudo_cmd} /bin/sh -c "vpnc --debug 1 --target-network 192.168.178.0/255.255.255.0 ./vpnc &" | tee vpn-launch.log
    #nohup ${sudo_cmd} /bin/sh -c "../wg-quicker.sh up ./wireguard.conf &" | tee vpn-launch.log
    nohup ${sudo_cmd} /bin/sh -c "wg-quick up ./wireguard.conf &" | tee vpn-launch.log
fi