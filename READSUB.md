1. Forken des gewünschten Repositories
2. Das Haupt-Repository, welchem das geforkte hinzugefügt werden soll, klonen
   git clone https://github.com/IhrBenutzername/IhrHauptRepo.git
   cd IhrHauptRepo
3. Das geforkte Repository als Submodul hinzufügen
   git submodule add https://github.com/IhrBenutzername/geforktesRepo.git pfad/zum/Unterordner
4. Submodul initalisieren und aktualisieren
   git submodule init
   git submodule update
5. Commit and Push
   git add .
   git commit -m "Geforktes Repository als Submodul hinzugefügt"
   git push

**!WICHTIG!** 
Remote-URL MUSS auf SSH geändert werden, damit aus dem Codespace heraus ein Branch angelegt werden kann!
# Überprüfen Sie die Remote-URL
git remote -v

# Ändern Sie die Remote-URL auf SSH (falls erforderlich)
git remote set-url origin git@github.com:SJuhl75/dab-cmdline.git

# Pushen Sie den Branch erneut
git push origin newbranch

--- Umgang mit Upstream Pull request ---
Wie können die Features eines Branches in einen anderen integriert werden?
a) in den Ziel-Branch wechseln
   git checkout <target-branch>
b) Den gewünschten Branch "einmergen" 
   git merge packetDataRS
c) Merge-Konflike auflösen
   - git status
   - Konflikte manuell lösen
   - git add <konflikt-datei>
   - git commit
   - git push

2) Was tun, wenn der Pull-Request akzeptiert wird?
--------------------------------------------------
a) In den Ziel-Branch wechseln
   git checkout <target-branch>
b) Den Ziel-Branch aktualisieren
   git pull origin <target-branch>
c) Den Feature-Branch löschen (optional)
   git branch -d <feature-branch>
   git push origin --delete <feature-branch>

3) Was tun, wenn der Pull-Request abgelehnt wird?
-------------------------------------------------
a) Feedback überprüfen und Änderungen vornehmen
   - Überprüfen Sie die Kommentare und das Feedback zum Pull-Request.
   - Nehmen Sie die erforderlichen Änderungen im Feature-Branch vor.
b) Änderungen committen und pushen
   git add .
   git commit -m "Feedback berücksichtigt"
   git push origin <feature-branch>
c) Neuen Pull-Request erstellen
   - Erstellen Sie einen neuen Pull-Request mit den vorgenommenen Änderungen.