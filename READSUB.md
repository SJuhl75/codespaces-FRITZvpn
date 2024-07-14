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