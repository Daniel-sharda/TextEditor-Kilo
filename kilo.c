#include <ctype.h> //función iscntrl()
#include <stdio.h> //funcion printf()
#include <stdlib.h> // funcion atexit()
#include <termios.h> // todo relacionado con RawMode.
#include <unistd.h> // funcion read() exclusiva de UNIX


struct termios orig_termios;




void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); //TCSAFLUSH descarta todos los inputs antes de aplicar los cambios (a la terminal)
}


void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw;
  raw = orig_termios;

  raw.c_lflag &= ~(ECHO | ICANON); //Desactivamos echo ECHO==00000000000000000000000000001000, ~ECHO=11111111111111111111111111110111 y aplicamos operador AND
  //ICANON activa que se lean los bytes hasta final de línea.

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


int main(void) {
  enableRawMode();

  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
    if (iscntrl(c)) {
      printf("%d\n", c);
    } else {
      printf("%d ('%c')\n", c, c);
    }
  }
  return 0;
}
