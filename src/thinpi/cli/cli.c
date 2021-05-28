#include <stdio.h>
#include <stdlib.h>

#define MIN_REQUIRED 2

int help() {
   printf("Usage: thinpi-cli [-i *] [-d *] [-e *] [-s *] [-r *] [-c *]\n");
   printf("\t-i package: install a package from apt *where package is the name of the package*\n");
   printf("\t-d package: remove a program from apt *where package is the name of the package*\n");
   printf("\t-e config: edit the config file via nano\n");
   printf("\t-s update: runs tpupdate utility\n");
   printf("\t-r start: starts thinpi connect manager\n");
   printf("\t-c start: starts thinpi config manager\n");

   return 1;
}

/* main */
int main(int argc, char *argv[]) {
   if (argc < MIN_REQUIRED) {
      return help();
   }
   int i;

   /* iterate over all arguments */
   for (i = 1; i < (argc - 1); i++) {
       if (strcmp("-i", argv[i]) == 0) {
          /* do something with it */ 
	  char *cmd = malloc(100);
          sprintf(cmd, "sudo apt install %s", argv[++i]);
	  system(cmd);
     free(cmd);
          continue;
       }
       if (strcmp("-d", argv[i]) == 0) {
	  char *cmd = malloc(100);
          sprintf(cmd, "sudo apt remove %s", argv[++i]);
	  system(cmd);
     free(cmd);
          continue;
       }
       if (strcmp("-e", argv[i]) == 0) {
          system("nano /thinpi/config/servers");
	  continue;
       }
	if (strcmp("-s", argv[i]) == 0) {
          system("tpupdate");
	  continue;
       }
       if (strcmp("-c", argv[i]) == 0) {
          system("/thinpi/thinpi-config");
	  continue;
       }
       if (strcmp("-r", argv[i]) == 0) {
          system("/thinpi/thinpi-manager");
	  continue;
       }
       return help();
   }
   return 0;
}