/*** Includes***/
#include <ctype.h> //función iscntrl()
#include <stdio.h> //funcion printf(), perror()
#include <stdlib.h> // funcion atexit(), exit()
#include <termios.h> // todo relacionado con RawMode.
#include <unistd.h> // funcion read() exclusiva de UNIX
#include <errno.h>


/*** data ***/
struct termios orig_termios;



/*** terminal  ***/
void die(const char *s)
{
  perror(s);//mira el numero de error para determinar el tipo de error y muestra un numero de error descriptivo
  exit(1);//termina la ejecucion del programa
}


void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios)==-1) die("tcsetattr"); //TCSAFLUSH descarta todos los inputs antes de aplicar los cambios (a la terminal)
}


void enableRawMode() {
  if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw;
  raw = orig_termios;

  raw.c_iflag &= ~(IXON | ICRNL); //IXON desactiva cntrl-s y cntrl-q que desactiva y reactiva la entrada de inputs a la terminal (XON y XOFF)
  //ICRNL hace que '\r' (cntrl-m) no se convierta en '\n' (cntrl-j)

  raw.c_oflag &= ~(OPOST); //OPOST Hace que la terminal no escriba '\r\n' en lugar de '\n'

  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); //Desactivamos echo ECHO==00000000000000000000000000001000, ~ECHO=11111111111111111111111111110111 y aplicamos operador AND
  //ICANON activa que se lean los bytes hasta final de línea
  //ISIG desactiva cntrl-c y cntrl-z que cortarian la ejecucion
  //IEXTEN desactiva que cuando pulses cntrl-v no tengas que pulsar otra tecla para que lo lea.

// activavomos varias flags que no tienen efecto aparente ya que suelen estar desactivadas de base en las terminales modernas
  raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP);
  raw.c_cflag |= (CS8); //hacemos que los caracteres sean de 8 bits en caso de que no lo fueran


  raw.c_cc[VMIN] = 0; //Numero min de bites que hacen falta para que lea el read (cuando llegue algo lo leera)
  raw.c_cc[VTIME] = 1; //Tiempo max que espera el read para hacer el return, para leer en decimas de segundo



  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}





/*** init ***/
int main(void) {
  enableRawMode();
  while (1) {
    char c;
    if (read(STDIN_FILENO, &c, 1) == -1 && errno!=EAGAIN) die("read");//Para dar soporte a CYGWIN
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }
    if (c == 'q') break;
  }
  return 0;
}
