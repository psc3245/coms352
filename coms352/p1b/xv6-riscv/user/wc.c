#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
/*
 * Name: Peter Collins
 * netid: psc3245
 * Changes Made:
 * 	I added a sinlge additional variable, d to keep track of digits.
 * 	Within the while loop of the wc function, I checked for digits,
 * 	incrementing d if a digit was found.
 *
 * 	I learned that isdigit is not a native c function, so I made a 
 * 	rudimentary implmentation using a loop running from char 0 to 9,
 * 	which are handled as ints in c.
 *
*/
char buf[512];

bool
isdigit(char c) {
 for (char d = '0'; d <= '9'; d++) {
 	if (c == d) return true;
 }
 return false;

}

void
wc(int fd, char *name)
{
  int i, n;
  int l, w, c, inword;
  int d;

  l = w = c = d = 0;
  inword = 0;
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(i=0; i<n; i++){
      c++;
      if(buf[i] == '\n')
        l++;
      if(isdigit(buf[i])) {
      	d++;
      }
      if(strchr(" \r\t\n\v", buf[i]))
        inword = 0;
      else if(!inword){
        w++;
        inword = 1;
      }
    }
  }
  if(n < 0){
    printf("wc: read error\n");
    exit(1);
  }
  printf("%d %d %d %d %s\n", l, w, c, d, name);
}

int
main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
    wc(0, "");
    exit(0);
  }

  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], O_RDONLY)) < 0){
      printf("wc: cannot open %s\n", argv[i]);
      exit(1);
    }
    wc(fd, argv[i]);
    close(fd);
  }
  exit(0);
}
