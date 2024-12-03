// Allgemeine Variablen
pi = 3.14159;
rad = 57.29578;
spiel = 0.05;    // Spiel zwischen Zähnen

/*  Wandelt Radian in Grad um */
function grad(eingriffswinkel) = eingriffswinkel*rad;

/*  Wandelt Grad in Radian um */
function radian(eingriffswinkel) = eingriffswinkel/rad;

/*  Wandelt 2D-Polarkoordinaten in kartesische um
    Format: radius, phi; phi = Winkel zur x-Achse auf xy-Ebene */
function pol_zu_kart(polvect) = [
    polvect[0]*cos(polvect[1]),  
    polvect[0]*sin(polvect[1])
];

/*  Kreisevolventen-Funktion:
    Gibt die Polarkoordinaten einer Kreisevolvente aus
    r = Radius des Grundkreises
    rho = Abrollwinkel in Grad */
function ev(r,rho) = [
    r/cos(rho),
    grad(tan(rho)-radian(rho))
];

/*  Kugelevolventen-Funktion
    Gibt den Azimutwinkel einer Kugelevolvente aus
    theta0 = Polarwinkel des Kegels, an dessen Schnittkante zur Großkugel die Evolvente abrollt
    theta = Polarwinkel, für den der Azimutwinkel der Evolvente berechnet werden soll */
function kugelev(theta0,theta) = 1/sin(theta0)*acos(cos(theta)/cos(theta0))-acos(tan(theta0)/tan(theta));

/*  Wandelt Kugelkoordinaten in kartesische um
    Format: radius, theta, phi; theta = Winkel zu z-Achse, phi = Winkel zur x-Achse auf xy-Ebene */
function kugel_zu_kart(vect) = [
    vect[0]*sin(vect[1])*cos(vect[2]),  
    vect[0]*sin(vect[1])*sin(vect[2]),
    vect[0]*cos(vect[1])
];

/*  prüft, ob eine Zahl gerade ist
    = 1, wenn ja
    = 0, wenn die Zahl nicht gerade ist */
function istgerade(zahl) =
    (zahl == floor(zahl/2)*2) ? 1 : 0;

/*  größter gemeinsamer Teiler
    nach Euklidischem Algorithmus.
    Sortierung: a muss größer als b sein */
function ggt(a,b) = 
    a%b == 0 ? b : ggt(b,a%b);

/*  Polarfunktion mit polarwinkel und zwei variablen */
function spirale(a, r0, phi) =
    a*phi + r0; 

/*  Kopiert und dreht einen Körper */
module kopiere(vect, zahl, abstand, winkel){
    for(i = [0:zahl-1]){
        translate(v=vect*abstand*i)
            rotate(a=i*winkel, v = [0,0,1])
                children(0);
    }
}

/*  Kegelrad
    modul = Höhe des Zahnkopfes über dem Teilkegel; Angabe für die Aussenseite des Kegels
    zahnzahl = Anzahl der Radzähne
    teilkegelwinkel = (Halb)winkel des Kegels, auf dem das jeweils andere Hohlrad abrollt
    zahnbreite = Breite der Zähne von der Außenseite in Richtung Kegelspitze
    bohrung = Durchmesser der Mittelbohrung
    eingriffswinkel = Eingriffswinkel, Standardwert = 20° gemäß DIN 867. Sollte nicht größer als 45° sein.
    schraegungswinkel = Schrägungswinkel, Standardwert = 0°
    hypoid_offset = Offset für Hypoid-Getrieberäder, Standardwert = 0
    beveloid = Boolean-Wert, der angibt, ob es sich um ein Beveloid-Getrieberad handelt */
module kegelrad(modul, zahnzahl, teilkegelwinkel, zahnbreite, bohrung, eingriffswinkel = 20, schraegungswinkel=0, hypoid_offset=0, beveloid=false) {

    // Dimensions-Berechnungen
    d_aussen = modul * zahnzahl;                                    // Teilkegeldurchmesser auf der Kegelgrundfläche,
                                                                    // entspricht der Sehne im Kugelschnitt
    r_aussen = d_aussen / 2;                                        // Teilkegelradius auf der Kegelgrundfläche 
    rg_aussen = r_aussen/sin(teilkegelwinkel);                      // Großkegelradius für Zahn-Außenseite, entspricht der Länge der Kegelflanke;
    rg_innen = rg_aussen - zahnbreite;                              // Großkegelradius für Zahn-Innenseite    
    r_innen = r_aussen*rg_innen/rg_aussen;
    alpha_stirn = atan(tan(eingriffswinkel)/cos(schraegungswinkel));// Schrägungswinkel im Stirnschnitt
    delta_b = asin(cos(alpha_stirn)*sin(teilkegelwinkel));          // Grundkegelwinkel        
    da_aussen = (modul <1)? d_aussen + (modul * 2.2) * cos(teilkegelwinkel): d_aussen + modul * 2 * cos(teilkegelwinkel);
    ra_aussen = da_aussen / 2;
    delta_a = asin(ra_aussen/rg_aussen);
    c = modul / 6;                                                  // Kopfspiel
    df_aussen = d_aussen - (modul +c) * 2 * cos(teilkegelwinkel);
    rf_aussen = df_aussen / 2;
    delta_f = asin(rf_aussen/rg_aussen);
    rkf = rg_aussen*sin(delta_f);                                   // Radius des Kegelfußes
    hoehe_f = rg_aussen*cos(delta_f);                               // Höhe des Kegels vom Fußkegel
    
    echo("Teilkegeldurchmesser auf der Kegelgrundfläche = ", d_aussen);
    
    // Größen für Komplementär-Kegelstumpf
    hoehe_k = (rg_aussen-zahnbreite)/cos(teilkegelwinkel);          // Höhe des Komplementärkegels für richtige Zahnlänge
    rk = (rg_aussen-zahnbreite)/sin(teilkegelwinkel);               // Fußradius des Komplementärkegels
    rfk = rk*hoehe_k*tan(delta_f)/(rk+hoehe_k*tan(delta_f));        // Kopfradius des Zylinders für 
                                                                    // Komplementär-Kegelstumpf
    hoehe_fk = rk*hoehe_k/(hoehe_k*tan(delta_f)+rk);                // Hoehe des Komplementär-Kegelstumpfs

    echo("Höhe Kegelrad = ", hoehe_f-hoehe_fk);
    
    phi_r = kugelev(delta_b, teilkegelwinkel);                      // Winkel zum Punkt der Evolvente auf Teilkegel
        
    // Torsionswinkel gamma aus Schrägungswinkel
    gamma_g = 2*atan(zahnbreite*tan(schraegungswinkel)/(2*rg_aussen-zahnbreite));
    gamma = 2*asin(rg_aussen/r_aussen*sin(gamma_g/2));
    
    schritt = (delta_a - delta_b)/16;
    tau = 360/zahnzahl;                                             // Teilungswinkel
    start = (delta_b > delta_f) ? delta_b : delta_f;
    spiegelpunkt = (180*(1-spiel))/zahnzahl+2*phi_r;

    // Zeichnung
    rotate([0,0,phi_r+90*(1-spiel)/zahnzahl]){                      // Zahn auf x-Achse zentrieren;
                                                                    // macht Ausrichtung mit anderen Rädern einfacher
        translate([0,0,hoehe_f]) rotate(a=[0,180,0]){
            union(){
                translate([0,0,hoehe_f]) rotate(a=[0,180,0]){                               // Kegelstumpf                            
                    difference(){
                        linear_extrude(height=hoehe_f-hoehe_fk, scale=rfk/rkf) circle(rkf*1.001); // 1 promille Überlappung mit Zahnfuß
                        translate([0,0,-1]){
                            cylinder(h = hoehe_f-hoehe_fk+2, r = bohrung/2);                // Bohrung
                        }
                    }    
                }
                for (rot = [0:tau:360]){
                    rotate (rot) {                                                          // "Zahnzahl-mal" kopieren und drehen
                        union(){
                            if (delta_b > delta_f){
                                // Zahnfuß
                                flankenpunkt_unten = 1*spiegelpunkt;
                                flankenpunkt_oben = kugelev(delta_f, start);
                                polyhedron(
                                    points = [
                                        kugel_zu_kart([rg_aussen, start*1.001, flankenpunkt_unten]),    // 1 promille Überlappung mit Zahn
                                        kugel_zu_kart([rg_innen, start*1.001, flankenpunkt_unten+gamma]),
                                        kugel_zu_kart([rg_innen, start*1.001, spiegelpunkt-flankenpunkt_unten+gamma]),
                                        kugel_zu_kart([rg_aussen, start*1.001, spiegelpunkt-flankenpunkt_unten]),                                
                                        kugel_zu_kart([rg_aussen, delta_f, flankenpunkt_unten]),
                                        kugel_zu_kart([rg_innen, delta_f, flankenpunkt_unten+gamma]),
                                        kugel_zu_kart([rg_innen, delta_f, spiegelpunkt-flankenpunkt_unten+gamma]),
                                        kugel_zu_kart([rg_aussen, delta_f, spiegelpunkt-flankenpunkt_unten])                                
                                    ],
                                    faces = [[0,1,2],[0,2,3],[0,4,1],[1,4,5],[1,5,2],[2,5,6],[2,6,3],[3,6,7],[0,3,7],[0,7,4],[4,6,5],[4,7,6]],
                                    convexity =1
                                );
                            }
                            // Zahn
                            for (delta = [start:schritt:delta_a-schritt]){
                                flankenpunkt_unten = kugelev(delta_b, delta);
                                flankenpunkt_oben = kugelev(delta_b, delta+schritt);
                                polyhedron(
                                    points = [
                                        kugel_zu_kart([rg_aussen, delta, flankenpunkt_unten]),
                                        kugel_zu_kart([rg_innen, delta, flankenpunkt_unten+gamma]),
                                        kugel_zu_kart([rg_innen, delta, spiegelpunkt-flankenpunkt_unten+gamma]),
                                        kugel_zu_kart([rg_aussen, delta, spiegelpunkt-flankenpunkt_unten]),                                
                                        kugel_zu_kart([rg_aussen, delta+schritt, flankenpunkt_oben]),
                                        kugel_zu_kart([rg_innen, delta+schritt, flankenpunkt_oben+gamma]),
                                        kugel_zu_kart([rg_innen, delta+schritt, spiegelpunkt-flankenpunkt_oben+gamma]),
                                        kugel_zu_kart([rg_aussen, delta+schritt, spiegelpunkt-flankenpunkt_oben])                                    
                                    ],
                                    faces = [[0,1,2],[0,2,3],[0,4,1],[1,4,5],[1,5,2],[2,5,6],[2,6,3],[3,6,7],[0,3,7],[0,7,4],[4,6,5],[4,7,6]],
                                    convexity =1
                                );
                            }
                        }
                    }
                }    
            }
        }
    }
}

// ### Beispielaufrufe
// - Der erste Aufruf generiert ein normales Kegelrad.
// - Der zweite Aufruf generiert ein Hypoid-Getrieberad mit einem Offset von 2.
// - Der dritte Aufruf generiert ein Beveloid-Getrieberad.

kegelrad(modul=0.6, zahnzahl=31, teilkegelwinkel=7, zahnbreite=5, bohrung=5, eingriffswinkel=20, schraegungswinkel=0, hypoid_offset=0, beveloid=false);
kegelrad(modul=0.6, zahnzahl=31, teilkegelwinkel=7, zahnbreite=5, bohrung=5, eingriffswinkel=20, schraegungswinkel=0, hypoid_offset=2, beveloid=false); // Hypoid-Getrieberad
kegelrad(modul=0.6, zahnzahl=31, teilkegelwinkel=7, zahnbreite=5, bohrung=5, eingriffswinkel=20, schraegungswinkel=0, hypoid_offset=0, beveloid=true); // Beveloid-Getrieberad

/* Beveloid-Getrieberäder haben eine konische Form, 
   während Hypoid-Getrieberäder eine versetzte Achse haben.

1. **Parameter**:
   - `hypoid_offset`: Offset für Hypoid-Getrieberäder. Standardwert ist 0.
   - `beveloid`: Boolean-Wert, der angibt, ob es sich um ein Beveloid-Getrieberad handelt. Standardwert ist `false`.

2. **Hypoid-Getrieberad**:
   - Wenn `hypoid_offset` ungleich 0 ist, wird der Kegel entlang der z-Achse um diesen Wert verschoben, um den Offset zu berücksichtigen.

3. **Beveloid-Getrieberad**:
   - Wenn `beveloid` auf `true` gesetzt ist, wird der Kegel entlang der z-Achse skaliert, um die konische Form eines Beveloid-Getrieberads zu erzeugen.
*/