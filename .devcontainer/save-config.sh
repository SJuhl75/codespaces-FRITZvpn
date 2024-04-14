#!/usr/bin/env bash
set -e
#export EN_VPN="ON"
# Switch to the .devcontainer folder
cd "$( dirname "${BASH_SOURCE[0]}" )"

# Create a temporary directory
mkdir -p vpntmp
cd vpntmp

# Save the configuration from the secret if it is present
if [ ! -z "${OPENVPN_CONFIG}" ]; then 
    echo "${OPENVPN_CONFIG}" > vpnconfig.ovpn
fi
if [ ! -z "${WIREGUARD_CONFIG}" ]; then 
    echo "${WIREGUARD_CONFIG}" > wireguard.conf
fi
if [ ! -z "${VPNC_CONFIG}" ]; then 
    echo "${VPNC_CONFIG}" > vpnc.conf
    #echo "Script $( dirname "${BASH_SOURCE[0]}" )/custom-script" >> vpnc.conf
    echo "Script ../custom-script.sh" >> vpnc.conf
    # /workspaces/codespaces-openvpn/.devcontainer
fi
if [ ! -z "${SSHD_PASS}" ]; then 
cat > sshp <<EOF
${SSHD_PASS}
${SSHD_PASS}
EOF
fi
SSH_USER=vscode
exit 0
#$(whoami)
echo "Updating Password for ${SSH_USER}" > pw-change.log
cat sshp | sudo passwd ${SSH_USER} >> pw-change.log 2>&1
rm sshp
exit 0