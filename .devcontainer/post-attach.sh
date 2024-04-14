#!/usr/bin/env bash
set -e
# Switch to the .devcontainer folder
cd "$( dirname "${BASH_SOURCE[0]}" )"
# Create a temporary directory
mkdir -p vpntmp
cd vpntmp
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
exit 0