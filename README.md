# MBH CROSSHAIR

Un mirino per Windows 10 e 11 a 64 bit, scritto in C con le API di Windows. Puoi scegliere croce, punto o anello e regolare colore, dimensione e spessore.

Apri `bin/MBH-Crosshair.exe`, scegli il monitor e premi la **X** per tornare al gioco. Il mirino resta visibile e lascia passare i clic. **Ctrl + Alt + X** lo mostra o lo nasconde. Per riaprire il pannello, clicca sull’icona vicino all’orologio. Per chiudere tutto, fai clic destro sull’icona e scegli **Esci**.

## Quanto pesa mentre giochi?

L’immagine del mirino è una bitmap di 100 × 100 pixel e occupa circa 39 KiB. Con la X, l’app libera l’anteprima e i controlli del pannello. Nel test su Windows, l’intero processo usava circa 12,4 MiB di RAM con il pannello chiuso. Finché il mirino resta fermo, il programma aspetta gli eventi di Windows senza ridisegnarlo a ogni fotogramma.

## Compilazione

Installa gli strumenti di compilazione dal terminale **UCRT64** di [MSYS2](https://www.msys2.org/).

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils
```

Poi esegui `build.cmd` dalla cartella del progetto. Troverai l’eseguibile in `build/MBH-Crosshair.exe`. Per lanciare il test, chiudi l’app ed esegui `.\tests\Verify.ps1` da PowerShell 7.

Licenza [MIT](LICENSE).
