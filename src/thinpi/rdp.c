#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "../include/thinpi.h"

void openConnection(gchar *u, gchar *p, gchar *s)
{	
	char *cmd = malloc(100);
	sprintf(cmd, "tprdp %s -p %s -u %s -z -x b -d %s", s, p, u, currentServerDomain);
	// use wayland over x11 - this will be default in the update -- sprintf(cmd, "wlfreerdp +home-drive +gfx-thin-client /t:ThinPi /cert:tofu /size:1280x720 /f /rfx /audio:sys:alsa /gdi:sw /u:'%s' /p:'%s' /v:%s", u, p, s);
	int rv = system(cmd);
	if(rv == 0 || rv == 2816) {
		gtk_entry_set_text(usernameTextbox, "");
		gtk_entry_set_text(passwordTextbox, "");
	} else if(rv != 0) {
		printf("Wrong Username or password\n");
		gtk_widget_show(wrongCredentialsMessage);
	}
	
}
