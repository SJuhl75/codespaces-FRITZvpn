#!/usr/bin/env bash
set -e
if [ ! -z "${SSHD_PASS}" ]; then 
cat > sshp <<EOF
${SSHD_PASS}
${SSHD_PASS}
EOF
fi
SSH_USER=$(whoami)
echo "Updating Password for ${SSH_USER}" > pw-change.log
cat sshp | sudo passwd ${SSH_USER} >> pw-change.log 2>&1
rm sshp
exit 0