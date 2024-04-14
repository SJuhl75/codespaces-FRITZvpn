#!/usr/bin/env bash
set -e

# Switch to the .devcontainer folder
cd "$( dirname "${BASH_SOURCE[0]}" )"

# Create a temporary directory
mkdir -p vpntmp
cd vpntmp

# Touch file to make sure this user can read it
touch vpn.log

# If we are running as root, we do not need to use sudo
sudo_cmd=""
if [ "$(id -u)" != "0" ]; then
    sudo_cmd="sudo"
fi

# Check for SSHD_PASS environment variable
if [ ! -z "${SSHD_PASS}" ]; then 
cat > sshp <<EOF
${SSHD_PASS}
${SSHD_PASS}
EOF
SSH_USER=$(whoami)
echo "Updating Password for ${SSH_USER}" > pw-change.log
cat sshp | sudo passwd ${SSH_USER} >> pw-change.log 2>&1
cat pw-change.log
rm sshp
fi

# Start up the VPN client based on credentials stored in environment variables
if [ "${VPNS}" == "ON" ] && [ ! -z "${WIREGUARD_CONFIG}" ]; then
    nohup ${sudo_cmd} /bin/sh -c "wg-quick down ./wireguard.conf" | tee vpn-launch.log
    nohup ${sudo_cmd} /bin/sh -c "wg-quick up ./wireguard.conf &" | tee vpn-launch.log
elif [ "${VPNS}" == "ON" ] && [ ! -z "${VPNC_CONFIG}" ]; then
    nohup ${sudo_cmd} /bin/sh -c "vpnc --debug 1 --target-network 192.168.178.0/255.255.255.0 ./vpnc &" | tee vpn-launch.log
fi

#if [ "${VPNS}" == "ON" ]; then
#nohup ${sudo_cmd} /bin/sh -c "openvpn --config vpnconfig.ovpn --log vpn.log &" | tee vpn-launch.log
#fi