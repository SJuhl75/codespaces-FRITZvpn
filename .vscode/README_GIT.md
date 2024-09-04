1) Initalisieren des Submoduls
    git submodule init
2) Aktualisieren der Submodule
    cd /workspaces/codespaces-openvpn
    git submodule update --remote
3) Commit & Push
    git add .
    git commit -m "Updated submodules to latest commit"
    git push
4) Status-Check
    git submodul status

5) Submodule aktualisieren / mergen
    a) Original-Repo hinzufügen
       - ggf. git remote add upstream https://github.com/original-owner/original-repo.git
    b) Änderungen vom Original-Repo holen
       - git fetch upstream
    c) Änderungen zusammenführen
    - git checkout main
    - git merge upstream/main

    Beispiel dab3 (aktive Entwicklung in diesem Codespace <-> dabex Repo)
    a) Quelle festlegen
        - git remote add upstream https://github.com/SJuhl75/dabex.git
    b) Aktualisierung abholen
        - git fetch upstream
        - git checkout main
            Already on 'main' Your branch is behind 'origin/main' by 1 commit, and can be fast-forwarded.
            (use "git pull" to update your local branch)
        - git merge upstream/main
            Updating 0afaa75..57fdd58
            Fast-forward
             README.md | 3 ++-
              1 file changed, 2 insertions(+), 1 deletion(-)
              
