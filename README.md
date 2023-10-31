# TEMA_1_APD Marching Square implementare paralela

## DETALII IMPLEMENTARE
Am lucrat la tema aproximativ 4-5 zile. Mi s-a parut de dificultate medie spre usoara, foarte eficienta pentru a intelege mai bine conceptul de paralelizare precum si utilitatea si modul de lucru cu primitivele de sincronizare in limbajul C.

## DETALII TEHNICE
Am inceput prin a paraleliza functiile sample_grid si march, care descriau cele 2 etape ale algoritmului., dupa care am continuat si cu alte functii auxiliare din varianta seriala prezentata. Functiile pe care le-am paralelizat pentru a obtine punctaj maxim sunt:
-> rescale_image (necesara pentru testele 6 & 7 care aveau imagini de dimensiuni mai mari ce trebuiau rescalate)
-> contour_map
-> sample_grid
-> march
La toate functiile am paralelizat doar primul for, iar indicii de start si de end sunt calculati dupa formulele prezentate in cadrul primului laborator. Crearea threadurilor se face in interiorul  functii main, iar pentru a folosi o unica pereche de functii create-join am creat o functie mai mare in care sa integrez paralelizarea tuturor functiilor. Toate argumentele necesare transmiterii functiei sunt impachetate in cadrul unei structuri ca si la laborator. Am utilizat o bariera care separa functia rescale de functia create_map, functia create_map de functia sample_grid, iar cea din urma separata de march. Am considerat acest lucru necesar pentru a impiedica posibile probleme de race condition, in cazul in care unele threaduri mai rapide ar putea trece la o etapa in care sa utilizeze o matrice care nu este deja creata. De asemenea, am alocat dinamic toate matricile/vectorii in cadrul functiei main inainte de a integrate in struct ca si parametri.  
Am mai creat si o functie de calcul minim pentru a o utiliza in calcului capatului de end al intervalului.
Functia update ramane neparalelizata.

## TESTARE
Am rulat tema de mai multe ori pentru a ma asigura ca obtin scalabilitate constanta.

## BIBLIOGRAFIE
-> laboratoarele 1-3

-> cursuri

-> https://man7.org/linux/man-pages/man7/pthreads.7.html

-> https://www.daemon-systems.org/man/pthread_barrier.3.html
