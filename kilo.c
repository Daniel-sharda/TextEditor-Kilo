/*** Includes***/
#include <asm-generic/ioctls.h>
#include <ctype.h> //función iscntrl()
#include <stdio.h> //funcion printf(), perror(), sscanf()
#include <stdlib.h> // funcion atexit(), exit(), free(), realloc()
#include <termios.h> // todo relacionado con RawMode.
#include <unistd.h> // funcion read() exclusiva de UNIX
#include <errno.h>
#include <sys/ioctl.h> //para determinar el tamaño de la terminal
#include <string.h> //función memcpy()


/*** definies ***/
#define CTRL_KEY(k) ((k) & 0x1f)//Transforma la letra k a cntrl+k (la tecla control quita los bits en las posiciones 5 y 6)


/*** data ***/
struct editorConfig {
  int screenRows;
  int screenCols;
  struct termios orig_termios;
};

struct editorConfig E;


/*** terminal  ***/
void die(const char *s)
{
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);//mira el numero de error para determinar el tipo de error y muestra un numero de error descriptivo
  exit(1);//termina la ejecucion del programa
}


void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios)==-1) die("tcsetattr"); //TCSAFLUSH descarta todos los inputs antes de aplicar los cambios (a la terminal)
}


void enableRawMode() {
  if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw;
  raw = E.orig_termios;

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

char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");//Para dar soporte a CYGWIN ya que lanza un error indebido y pone marca EAGAIN
  }
  return c;
}


int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;//Se utiliza para pedir la posición del cursor. (n se utiliza para pedir información a la terminal y 6 es el cursor)

  while(i<sizeof(buf)-1) {
    if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if(buf[i] == 'R') break;
    i++;
  }
  buf[i]='\0';

  if (buf[0]!='\x1b' || buf[1] != '[') return -1;
  if(sscanf(&buf[2], "%d;%d", rows, cols ) != 2) return -1;

  return 0;
}




int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col==0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1; //C y B previenen que el cursor se salga de la pantalla, frente a x;yH
    return getCursorPosition(rows, cols);
  } else {
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
  }
}


/*** append buffer ***/
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT = {NULL, 0};


void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(  ab->b, ab->len + len);

  if(new == NULL) return;//Si new==NULL no se ha podido alocar la memoria. Pero sigue existiendo el puntero ab.b(si se asigna valor a new entonces se borra ab.b)
  memcpy(&new[ab->len], s, len);//añadimos la cadena s a la cadena abuf.b(justo al final en ab.b[len]). Cuidado con OVERFLOWS (escribir más alla de la memoria reservada para el string destino). Como hemos guardado con realloc memoria sabbemos que no nos sobrepasamos.
  ab->b = new;
  ab->len += len;
}


void abFree(struct abuf *ab) {
  free(ab->b);
}



/*** input ***/

void editorProcessKeypress() {
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);//Borra toda la pantalla
      write(STDOUT_FILENO, "\x1b[H", 3);//Coloca el cursor al inicio
      exit(0);
      break;
  }
}



/*** output ***/
void editorDrawRaws() {
  int y;
  for (y=0;y<E.screenRows; y++) {
    write(STDOUT_FILENO, "~", 1);
    if (y < E.screenRows-1) {
      write(STDOUT_FILENO, "\r\n", 2);
    }
  }
}


void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);//"\x1b" indica el caracter ESC "\x" indica que se va a escribir un bit Hexadecimal (1b)
  //Con <ESC> + [ indicamos a la terminal de hacer varias funciones de formateo. (Usando el estandar VT100). Con J le indicamos que borre la pantalla y con 2 por delante y detrás del cursor. (0 sería delante hasta fin y 1 incio hasta cursor).
  write(STDOUT_FILENO, "\x1b[H", 3); //Con este le indicamos que coloque el cursor al inicio. (default: 1,1). ("[12;27H" sería fila 12 columna 27).

  editorDrawRaws();

  write(STDOUT_FILENO, "\x1b[H", 3);
}


/*** init ***/
void initEditor() {
  if(getWindowSize(&E.screenRows, &E.screenCols) == -1) die("getWindowSize");
}


int main(void) {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
