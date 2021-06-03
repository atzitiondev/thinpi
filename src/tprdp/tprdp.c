//START INCLUDE
#include <stdarg.h>		
#include <unistd.h>	
#include <fcntl.h>		
#include <pwd.h>		
#include <termios.h>	
#include <sys/stat.h>	
#include <sys/time.h>	
#include <sys/times.h>	
#include <ctype.h>		
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include "tprdp.h"
#include "./Protocol/ssl.h"
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef EGD_SOCKET
#include <sys/types.h>
#include <sys/socket.h>	
#include <sys/un.h>		
#endif

//END INCLUDE

//TPRDP CODE START

#define TIMEOUT (3600+600)
#define TPRDP_STORE "/thinpi/licenses"

uint8 tprdpStaticSalt[16] = { 0xb8, 0x82, 0x29, 0x31, 0xc5, 0x39, 0xd9, 0x44, 0x54, 0x15, 0x5e, 0x14, 0x71, 0x38, 0xd5, 0x4d };

char tpTitle[64] = "";
char *tpUsername;
char tpPassword[64] = "";
char tpHostname[16] = "";
char tpKeymap[PATH_MAX] = "";
unsigned int tpKBLayout = 0x409;	
int tpKBType = 0x4;	
int tpKBSubType = 0x0;	
int tpKBFuncKeys = 0xc;	
int tpDPI = 0;

uint32 tpSessionWidth = 1280;
uint32 tpSessionHeight = 720;

window_size_type_t tpWindowType = Fixed;


int tpXPos = 0;
int tpYPos = 0;
int tpPos = 0;	   
extern int tpTCPPort;
int tpServerDepth = -1;
int tpButtonSize = 0;	/* If zero, disable single app mode */
RD_BOOL tpNetworkError = False;
RD_BOOL tpSendmotion = True;
RD_BOOL tpBitmapCache = True;
RD_BOOL tpBitmapCachePersistEnable = False;
RD_BOOL tpBitmapCachePrecache = True;
RD_BOOL tpCtrlMode = True;
RD_BOOL tpEncryption = True;
RD_BOOL tpEncryptionInitial = True;
RD_BOOL tpEncryptionPacket = True;
RD_BOOL tpDesktopSave = True;	/* desktop save order */
RD_BOOL tpPolygonEllipseOrder = True;	/* polygon / ellipse orders */
RD_BOOL tpFullscreen = False;
RD_BOOL tpKeyboardGrab = True;
RD_BOOL tpLocalCursor = False;
RD_BOOL tpHideDecorations = False;
RDP_VERSION tpRDPVersion = RDP_V5;	/* Default to version 5 */
RD_BOOL tpRDPClip = True;
RD_BOOL tpConsoleSession = False;
RD_BOOL tpNUMLCKSync = False;
RD_BOOL tpLSPCIEnabled = False;
RD_BOOL tpColMap = False;
RD_BOOL tpSelfBackstore = True;	/* We can't rely on external BackingStore */
RD_BOOL tpSeamlessRDP = False;
RD_BOOL tpPasswordASPin = False;
char tpSeamlessShell[512];
char tpSeamlessShellSpawn[512];
char tpTLSVersion[4];
RD_BOOL tpSeamlessPersistent = True;
RD_BOOL tpUserQuit = False;
uint32 tpEmbedWindow;
uint32 tpPerformanceFlags = (PERF_DISABLE_FULLWINDOWDRAG | PERF_DISABLE_MENUANIMATIONS | PERF_ENABLE_FONT_SMOOTHING);

/* Session Directory redirection */
RD_BOOL tpRedirect = False;
char *tpRedirectServer;
uint32 tpRedirectServerLen;
char *tpRedirectDomain;
uint32 tpRedirectDomainLen;
char *tpRedirectUsername;
uint32 tpRedirectUsernameLen;
uint8 *tpRedirectLBInfo;
uint32 tpRedirectLBInforLen;
uint8 *tpRedirectCookie;
uint32 tpRedirectCookieLen;
uint32 tpRedirectFlags = 0;
uint32 tpRedirectSessionID = 0;

uint32 tpReconnectID = 0;
char tpReconnectRandom[16];
time_t tpReconnectRandomTS;
RD_BOOL tpReconnectHASRandom = False;
RD_BOOL tpReconnectLoop = False;
uint8 tpClientRandom[SEC_RANDOM_SIZE];
RD_BOOL tpPendingResize = False;
RD_BOOL tpPendingResizeDefer = True;
struct timeval tpPendingResizeDeferTimer = { 0 };

#ifdef WITH_RDPSND
RD_BOOL tpRDPSound = False;
#endif

char tpCodepage[16] = "";

char *tpSCCSPName = NULL;
char *tpSCReaderName = NULL;
char *tpSCCardName = NULL;
char *tpSCContainerName = NULL;

extern RDPDR_DEVICE tpRDPDRDevice[];
extern uint32 tpRDPDRNumDevices;
extern char *tpRDPDRClientName;

//USAGE INFO
/////////////TODO 
//END USAGE

static int tpDisconnectReason(RD_BOOL deactivated, uint16 reason)
{
	char *text;
	int retval;

	switch (reason)
	{
		case ERRINFO_NO_INFO:
			text = "No information available";
			if (deactivated)
				retval = EX_OK;
			else
				retval = EXRD_UNKNOWN;
			break;

		case ERRINFO_RPC_INITIATED_DISCONNECT:
			text = "Administrator initiated disconnect";
			retval = EXRD_DISCONNECT_BY_ADMIN;
			break;

		case ERRINFO_RPC_INITIATED_LOGOFF:
			text = "Administrator initiated logout";
			retval = EXRD_LOGOFF_BY_ADMIN;
			break;

		case ERRINFO_IDLE_TIMEOUT:
			text = "Server idle session time limit reached";
			retval = EXRD_IDLE_TIMEOUT;
			break;

		case ERRINFO_LOGON_TIMEOUT:
			text = "Server active session time limit reached";
			retval = EXRD_LOGON_TIMEOUT;
			break;

		case ERRINFO_DISCONNECTED_BY_OTHERCONNECTION:
			text = "The session was replaced";
			retval = EXRD_REPLACED;
			break;

		case ERRINFO_OUT_OF_MEMORY:
			text = "The server is out of memory";
			retval = EXRD_OUT_OF_MEM;
			break;

		case ERRINFO_SERVER_DENIED_CONNECTION:
			text = "The server denied the connection";
			retval = EXRD_DENIED;
			break;

		case ERRINFO_SERVER_DENIED_CONNECTION_FIPS:
			text = "The server denied the connection for security reasons";
			retval = EXRD_DENIED_FIPS;
			break;

		case ERRINFO_SERVER_INSUFFICIENT_PRIVILEGES:
			text = "The user cannot connect to the server due to insufficient access privileges.";
			retval = EXRD_INSUFFICIENT_PRIVILEGES;
			break;

		case ERRINFO_SERVER_FRESH_CREDENTIALS_REQUIRED:
			text = "The server does not accept saved user credentials and requires that the user enter their credentials for each connection.";
			retval = EXRD_FRESH_CREDENTIALS_REQUIRED;
			break;

		case ERRINFO_RPC_INITIATED_DISCONNECT_BYUSER:
			text = "Disconnect initiated by user";
			retval = EXRD_DISCONNECT_BY_USER;
			break;

		case ERRINFO_LOGOFF_BYUSER:
			text = "Logout initiated by user";
			retval = EXRD_LOGOFF_BY_USER;
			break;

		case ERRINFO_LICENSE_INTERNAL:
			text = "Internal licensing error";
			retval = EXRD_LIC_INTERNAL;
			break;

		case ERRINFO_LICENSE_NO_LICENSE_SERVER:
			text = "No license server available";
			retval = EXRD_LIC_NOSERVER;
			break;

		case ERRINFO_LICENSE_NO_LICENSE:
			text = "No valid license available";
			retval = EXRD_LIC_NOLICENSE;
			break;

		case ERRINFO_LICENSE_BAD_CLIENT_MSG:
			text = "Invalid licensing message from client";
			retval = EXRD_LIC_MSG;
			break;

		case ERRINFO_LICENSE_HWID_DOESNT_MATCH_LICENSE:
			text = "The client license has been modified and does no longer match the hardware ID";
			retval = EXRD_LIC_HWID;
			break;

		case ERRINFO_LICENSE_BAD_CLIENT_LICENSE:
			text = "The client license is in an invalid format";
			retval = EXRD_LIC_CLIENT;
			break;

		case ERRINFO_LICENSE_CANT_FINISH_PROTOCOL:
			text = "Network error during licensing protocol";
			retval = EXRD_LIC_NET;
			break;

		case ERRINFO_LICENSE_CLIENT_ENDED_PROTOCOL:
			text = "Licensing protocol was not completed";
			retval = EXRD_LIC_PROTO;
			break;

		case ERRINFO_LICENSE_BAD_CLIENT_ENCRYPTION:
			text = "Incorrect client license encryption";
			retval = EXRD_LIC_ENC;
			break;

		case ERRINFO_LICENSE_CANT_UPGRADE_LICENSE:
			text = "Can't upgrade or renew license";
			retval = EXRD_LIC_UPGRADE;
			break;

		case ERRINFO_LICENSE_NO_REMOTE_CONNECTIONS:
			text = "The server is not licensed to accept remote connections";
			retval = EXRD_LIC_NOREMOTE;
			break;

		case ERRINFO_CB_DESTINATION_NOT_FOUND:
			text = "The target endpoint chosen by the broker could not be found";
			retval = EXRD_CB_DEST_NOT_FOUND;
			break;

		case ERRINFO_CB_LOADING_DESTINATION:
			text = "The target endpoint is disconnecting from the broker";
			retval = EXRD_CB_DEST_LOADING;
			break;

		case ERRINFO_CB_REDIRECTING_TO_DESTINATION:
			text = "Error occurred while being redirected by broker";
			retval = EXRD_CB_REDIR_DEST;
			break;

		case ERRINFO_CB_SESSION_ONLINE_VM_WAKE:
			text = "Error while the endpoint VM was being awakened by the broker";
			retval = EXRD_CB_VM_WAKE;
			break;

		case ERRINFO_CB_SESSION_ONLINE_VM_BOOT:
			text = "Error while the endpoint VM was being started by the broker";
			retval = EXRD_CB_VM_BOOT;
			break;

		case ERRINFO_CB_SESSION_ONLINE_VM_NO_DNS:
			text = "The IP address of the endpoint VM could not be determined by the broker";
			retval = EXRD_CB_VM_NODNS;
			break;

		case ERRINFO_CB_DESTINATION_POOL_NOT_FREE:
			text = "No available endpoints in the connection broker pool";
			retval = EXRD_CB_DEST_POOL_NOT_FREE;
			break;

		case ERRINFO_CB_CONNECTION_CANCELLED:
			text = "Connection processing cancelled by the broker";
			retval = EXRD_CB_CONNECTION_CANCELLED;
			break;

		case ERRINFO_CB_CONNECTION_ERROR_INVALID_SETTINGS:
			text = "The connection settings could not be validated by the broker";
			retval = EXRD_CB_INVALID_SETTINGS;
			break;

		case ERRINFO_CB_SESSION_ONLINE_VM_BOOT_TIMEOUT:
			text = "Timeout while the endpoint VM was being started by the broker";
			retval = EXRD_CB_VM_BOOT_TIMEOUT;
			break;

		case ERRINFO_CB_SESSION_ONLINE_VM_SESSMON_FAILED:
			text = "Session monitoring error while the endpoint VM was being started by the broker";
			retval = EXRD_CB_VM_BOOT_SESSMON_FAILED;
			break;

		case ERRINFO_REMOTEAPPSNOTENABLED:
			text = "The server can only host Remote Applications";
			retval = EXRD_RDP_REMOTEAPPSNOTENABLED;
			break;

		case ERRINFO_UPDATESESSIONKEYFAILED:
			text = "Update of session keys failed";
			retval = EXRD_RDP_UPDATESESSIONKEYFAILED;
			break;

		case ERRINFO_DECRYPTFAILED:
			text = "Decryption or session key creation failed";
			retval = EXRD_RDP_DECRYPTFAILED;
			break;

		case ERRINFO_ENCRYPTFAILED:
			text = "Encryption failed";
			retval = EXRD_RDP_ENCRYPTFAILED;
			break;

		default:
			text = "Unknown reason";
			retval = EXRD_UNKNOWN;
	}

	if (reason > 0x1000 && reason < 0x7fff && retval == EXRD_UNKNOWN)
	{
		fprintf(stderr, "Internal protocol error: %x", reason);
	}
	else if (reason != ERRINFO_NO_INFO)
	{
		fprintf(stderr, "disconnect: %s.\n", text);
	}

	return retval;
}

static void tprdpResetState(void)
{
	tpPendingResizeDefer = True;

	rdp_reset_state();
#ifdef WITH_SCARD
	scard_reset_state();
#endif
#ifdef WITH_RDPSND
	rdpsnd_reset_state();
#endif
}

static RD_BOOL tprdpReadPassword(char *password, int size)
{
	struct termios tios;
	RD_BOOL ret = False;
	int istty = 0;
	const char *prompt;
	char *p;


	if (tpPasswordASPin)
	{
		prompt = "Smart card PIN: ";
	}
	else
	{
		prompt = "Password: ";
	}

	if (tcgetattr(STDIN_FILENO, &tios) == 0)
	{
		fputs(prompt, stderr);
		tios.c_lflag &= ~ECHO;
		tcsetattr(STDIN_FILENO, TCSANOW, &tios);
		istty = 1;
	}

	if (fgets(password, size, stdin) != NULL)
	{
		ret = True;

		/* strip final newline */
		p = strchr(password, '\n');
		if (p != NULL)
			*p = 0;
	}

	if (istty)
	{
		tios.c_lflag |= ECHO;
		tcsetattr(STDIN_FILENO, TCSANOW, &tios);
		fprintf(stderr, "\n");
	}

	return ret;
}

static void tprdpParseServerAndPort(char *server)
{
	char *p;
#ifdef IPv6
	int addr_colons;
#endif

#ifdef IPv6
	p = server;
	addr_colons = 0;
	while (*p)
		if (*p++ == ':')
			addr_colons++;
	if (addr_colons >= 2)
	{
		/* numeric IPv6 style address format - [1:2:3::4]:port */
		p = strchr(server, ']');
		if (*server == '[' && p != NULL)
		{
			if (*(p + 1) == ':' && *(p + 2) != '\0')
				tpTCPPort = strtol(p + 2, NULL, 10);
			/* remove the port number and brackets from the address */
			*p = '\0';
			strncpy(server, server + 1, strlen(server));
		}
	}
	else
	{
		/* DNS name or IPv4 style address format - server.example.com:port or 1.2.3.4:port */
		p = strchr(server, ':');
		if (p != NULL)
		{
			tpTCPPort = strtol(p + 1, NULL, 10);
			*p = 0;
		}
	}
#else /* no IPv6 support */
	p = strchr(server, ':');
	if (p != NULL)
	{
		tpTCPPort = strtol(p + 1, NULL, 10);
		*p = 0;
	}
#endif /* IPv6 */

}

int tpParseGeometry(const char *optarg)
{
	sint32 value;
	const char *ps;
	char *pe;

	/* special keywords */
	if (strcmp(optarg, "workarea") == 0)
	{
		tpWindowType = Workarea;
		return 0;
	}

	/* parse first integer */
	ps = optarg;
	value = strtol(ps, &pe, 10);
	if (ps == pe || value <= 0)
	{
		logger(Core, Error, "invalid geometry, expected positive integer for width");
		return -1;
	}

	tpSessionWidth = value;
	ps = pe;

	/* expect % or x */
	if (*ps != '%' && *ps != 'x')
	{
		logger(Core, Error, "invalid geometry, expected '%%' or 'x' after width");
		return -1;
	}

	if (*ps == '%')
	{
		tpWindowType = PercentageOfScreen;
		ps++;
		pe++;
	}

	if (*ps == 'x')
	{
		ps++;
		value = strtol(ps, &pe, 10);
		if (ps == pe || value <= 0)
		{
			logger(Core, Error,
			       "invalid geometry, expected positive integer for height");
			return -1;
		}

		tpSessionHeight = value;
		ps = pe;

		if (*ps == '%' && tpWindowType == Fixed)
		{
			logger(Core, Error, "invalid geometry, unexpected '%%' after height");
			return -1;
		}

		if (tpWindowType == PercentageOfScreen)
		{
			if (*ps != '%')
			{
				logger(Core, Error, "invalid geometry, expected '%%' after height");
				return -1;
			}
			ps++;
			pe++;
		}
	}
	else
	{
		if (tpWindowType == PercentageOfScreen)
		{
			/* percentage of screen used for both width and height */
			tpSessionHeight = tpSessionWidth;
		}
		else
		{
			logger(Core, Error, "invalid geometry, missing height (WxH)");
			return -1;
		}
	}

	/* parse optional dpi */
	if (*ps == '@')
	{
		ps++;
		pe++;
		value = strtol(ps, &pe, 10);
		if (ps == pe || value <= 0)
		{
			logger(Core, Error, "invalid geometry, expected positive integer for DPI");
			return -1;
		}

		tpDPI = value;
		ps = pe;
	}

	/* parse optional window position */
	if (*ps == '+' || *ps == '-')
	{
		/* parse x position */
		value = strtol(ps, &pe, 10);
		if (ps == pe)
		{
			logger(Core, Error, "invalid geometry, expected an integer for X position");
			return -1;
		}

		tpPos |= (value < 0) ? 2 : 1;
		tpXPos = value;
		ps = pe;
	}

	if (*ps == '+' || *ps == '-')
	{
		/* parse y position */
		value = strtol(ps, &pe, 10);
		if (ps == pe)
		{
			logger(Core, Error, "invalid geometry, expected an integer for Y position");
			return -1;
		}
		tpPos |= (value < 0) ? 4 : 1;
		tpYPos = value;
		ps = pe;
	}

	if (*pe != '\0')
	{
		logger(Core, Error, "invalid geometry, unexpected characters at end of string");
		return -1;
	}
	return 0;
}

static void tpSetupRequestedSessionSize()
{
	switch (tpWindowType)
	{
		case Fullscreen:
			ui_get_screen_size(&tpSessionWidth, &tpSessionWidth);
			break;

		case Workarea:
			ui_get_workarea_size(&tpSessionWidth,
					     &tpSessionWidth);
			break;

		case Fixed:
			break;

		case PercentageOfScreen:
			ui_get_screen_size_from_percentage(tpSessionWidth,
							   tpSessionWidth,
							   &tpSessionWidth,
							   &tpSessionWidth);
			break;
	}
}


/* Client program */
int
main(int argc, char *argv[])
{


	char server[256];
	char fullhostname[64];
	char domain[256];
	char shell[256];
	char directory[256];
	RD_BOOL prompt_password, deactivated;
	struct passwd *pw;
	uint32 flags, ext_disc_reason = 0;
	char *p;
	int c;
	char *locale = NULL;
	int username_option = 0;
	RD_BOOL geometry_option = False;
#ifdef WITH_RDPSND
	char *rdpsnd_optarg = NULL;
#endif

	/* setup debug logging from environment */
	logger_set_subjects(getenv("tprdp_DEBUG"));

#ifdef HAVE_LOCALE_H
	/* Set locale according to environment */
	locale = setlocale(LC_ALL, "");
	if (locale)
	{
		locale = xstrdup(locale);
	}

#endif

	/* Ignore SIGPIPE, since we are using popen() */
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);

	/* setup default flags for TS_INFO_PACKET */
	flags = RDP_INFO_MOUSE | RDP_INFO_DISABLECTRLALTDEL | RDP_INFO_UNICODE | RDP_INFO_MAXIMIZESHELL | RDP_INFO_ENABLEWINDOWSKEY;

	prompt_password = False;
	tpSeamlessShellSpawn[0] = tpTLSVersion[0] = domain[0] = tpPassword[0] = shell[0] = directory[0] = 0;
	tpEmbedWindow = 0;

	tpRDPDRNumDevices = 0;

	while ((c = getopt(argc, argv,
			   "A:V:u:L:d:s:c:p:n:k:g:o:fbBeEitmMzCDKS:T:NX:a:x:Pr:045vh?")) != -1)
	{
		switch (c)
		{
			case 'A':
				tpSeamlessRDP = True;
				STRNCPY(tpSeamlessShell, optarg, sizeof(tpSeamlessShell));
				break;

			case 'V':
				STRNCPY(tpTLSVersion, optarg, sizeof(tpTLSVersion));
				break;

			case 'u':
				tpUsername = (char *) xmalloc(strlen(optarg) + 1);
				strcpy(tpUsername, optarg);
				username_option = 1;
				break;

			case 'L':
				STRNCPY(tpCodepage, optarg, sizeof(tpCodepage));
				break;

			case 'd':
				STRNCPY(domain, optarg, sizeof(domain));
				break;

			case 's':
				STRNCPY(shell, optarg, sizeof(shell));
				tpSeamlessPersistent = False;
				break;

			case 'c':
				STRNCPY(directory, optarg, sizeof(directory));
				break;

			case 'p':
				if ((optarg[0] == '-') && (optarg[1] == 0))
				{
					prompt_password = True;
					break;
				}

				STRNCPY(tpPassword, optarg, sizeof(tpPassword));
				flags |= RDP_INFO_AUTOLOGON;

				/* try to overwrite argument so it won't appear in ps */
				p = optarg;
				while (*p)
					*(p++) = 'X';
				break;
#ifdef WITH_SCARD
			case 'i':
				flags |= RDP_INFO_PASSWORD_IS_SC_PIN;
				tpPasswordASPin = True;
				break;
#endif
			case 't':
				tpCtrlMode = False;
				break;

			case 'n':
				STRNCPY(tpHostname, optarg, sizeof(tpHostname));
				break;

			case 'k':
				STRNCPY(tpKeymap, optarg, sizeof(tpKeymap));
				break;

			case 'g':
				geometry_option = True;
				tpFullscreen = False;
				if (tpParseGeometry(optarg) != 0)
				{
					return EX_USAGE;
				}
				break;

			case 'f':
				tpWindowType = Fullscreen;
				tpFullscreen = True;
				break;

			case 'b':
				tpBitmapCache = False;
				break;

			case 'B':
				tpSelfBackstore = False;
				break;

			case 'e':
				tpEncryptionInitial = tpEncryption = False;
				break;
			case 'E':
				tpEncryptionPacket = False;
				break;
			case 'm':
				tpSendmotion = False;
				break;
			case 'M':
				tpLocalCursor = True;
				break;

			case 'C':
				tpColMap = True;
				break;

			case 'D':
				tpHideDecorations = True;
				break;

			case 'K':
				tpKeyboardGrab = False;
				break;

			case 'S':
				if (!strcmp(optarg, "standard"))
				{
					tpButtonSize = 18;
					break;
				}

				tpButtonSize = strtol(optarg, &p, 10);

				if (*p)
				{
					logger(Core, Error, "invalid button size");
					return EX_USAGE;
				}

				break;

			case 'T':
				STRNCPY(tpTitle, optarg, sizeof(tpTitle));
				break;

			case 'N':
				tpNUMLCKSync = True;
				break;

			case 'X':
				tpEmbedWindow = strtol(optarg, NULL, 0);
				break;

			case 'a':
				tpServerDepth = strtol(optarg, NULL, 10);
				if (tpServerDepth != 8 &&
				    tpServerDepth != 16 &&
				    tpServerDepth != 15 && tpServerDepth != 24
				    && tpServerDepth != 32)
				{
					logger(Core, Error,
					       "Invalid server colour depth specified");
					return EX_USAGE;
				}
				break;

			case 'z':
				logger(Core, Debug, "rdp compression enabled");
				flags |= (RDP_INFO_COMPRESSION | RDP_INFO_COMPRESSION2);
				break;

			case 'x':
				if (str_startswith(optarg, "m"))	/* modem */
				{
					fprintf(stderr, "Using RDP5 Modem Mode");
					tpPerformanceFlags = (PERF_DISABLE_CURSOR_SHADOW |
								   PERF_DISABLE_WALLPAPER |
								   PERF_DISABLE_FULLWINDOWDRAG |
								   PERF_DISABLE_MENUANIMATIONS |
								   PERF_DISABLE_THEMING);
				}
				else if (str_startswith(optarg, "b"))	/* broadband */
				{
					fprintf(stderr, "Using RDP5 Broadband Mode");
					tpPerformanceFlags = (PERF_DISABLE_WALLPAPER |
								   PERF_ENABLE_FONT_SMOOTHING);
				}
				else if (str_startswith(optarg, "l"))	/* LAN */
				{
					fprintf(stderr, "Using RDP5 LAN Mode");
					tpPerformanceFlags = PERF_ENABLE_FONT_SMOOTHING;
				}
				else
				{
					fprintf(stderr, "Using RDP5 NULL Mode");
					tpPerformanceFlags = strtol(optarg, NULL, 16);
				}
				break;

			case 'P':
				tpBitmapCachePersistEnable = True;
				break;

			case 'r':

				if (str_startswith(optarg, "sound"))
				{
					optarg += 5;

					if (*optarg == ':')
					{
						optarg++;
						while ((p = next_arg(optarg, ',')))
						{
							if (str_startswith(optarg, "remote"))
								flags |= RDP_INFO_REMOTE_CONSOLE_AUDIO;

							if (str_startswith(optarg, "local"))
#ifdef WITH_RDPSND
							{
								rdpsnd_optarg =
									next_arg(optarg, ':');
								tpRDPSound = True;
							}

#else
								logger(Core, Warning,
								       "Not compiled with sound support");
#endif

							if (str_startswith(optarg, "off"))
#ifdef WITH_RDPSND
								tpRDPSound = False;
#else
								logger(Core, Warning,
								       "Not compiled with sound support");
#endif

							optarg = p;
						}
					}
					else
					{
#ifdef WITH_RDPSND
						tpRDPSound = True;
#else
						logger(Core, Warning,
						       "Not compiled with sound support");
#endif
					}
				}
				else if (str_startswith(optarg, "disk"))
				{
					/* -r disk:h:=/mnt/floppy */
					disk_enum_devices(&tpRDPDRNumDevices, optarg + 4);
				}
				else if (str_startswith(optarg, "comport"))
				{
					serial_enum_devices(&tpRDPDRNumDevices, optarg + 7);
				}
				else if (str_startswith(optarg, "lspci"))
				{
					tpLSPCIEnabled = True;
				}
				else if (str_startswith(optarg, "lptport"))
				{
					parallel_enum_devices(&tpRDPDRNumDevices, optarg + 7);
				}
				else if (str_startswith(optarg, "printer"))
				{
					printer_enum_devices(&tpRDPDRNumDevices, optarg + 7);
				}
				else if (str_startswith(optarg, "clientname"))
				{
					tpRDPDRClientName = xmalloc(strlen(optarg + 11) + 1);
					strcpy(tpRDPDRClientName, optarg + 11);
				}
				else if (str_startswith(optarg, "clipboard"))
				{
					optarg += 9;

					if (*optarg == ':')
					{
						optarg++;

						if (str_startswith(optarg, "off"))
							tpRDPClip = False;
						else
							cliprdr_set_mode(optarg);
					}
					else
						tpRDPClip = True;
				}
				else if (strncmp("scard", optarg, 5) == 0)
				{
#ifdef WITH_SCARD
					scard_enum_devices(&tpRDPDRNumDevices, optarg + 5);
#else
					logger(Core, Warning,
					       "Not compiled with smartcard support\n");
#endif
				}
				else
				{
					logger(Core, Warning,
					       "Unknown -r argument '%s'. Possible arguments are: comport, disk, lptport, printer, sound, clipboard, scard",
					       optarg);
				}
				break;

			case '0':
				tpConsoleSession = True;
				break;

			case '4':
				tpRDPVersion = RDP_V4;
				break;

			case '5':
				tpRDPVersion = RDP_V5;
				break;
#if WITH_SCARD
			case 'o':
				{
					char *p = strchr(optarg, '=');
					if (p == NULL)
					{
						logger(Core, Warning,
						       "Skipping specified option '%s', lacks name=value format",
						       optarg);
						continue;
					}

					if (strncmp(optarg, "sc-csp-name", strlen("sc-scp-name")) ==
					    0)
						tpSCCSPName = strdup(p + 1);
					else if (strncmp
						 (optarg, "sc-reader-name",
						  strlen("sc-reader-name")) == 0)
						tpSCReaderName = strdup(p + 1);
					else if (strncmp
						 (optarg, "sc-card-name",
						  strlen("sc-card-name")) == 0)
						tpSCCardName = strdup(p + 1);
					else if (strncmp
						 (optarg, "sc-container-name",
						  strlen("sc-container-name")) == 0)
						tpSCContainerName = strdup(p + 1);

				}
				break;
#endif
			case 'v':
				logger_set_verbose(1);
				break;
			case 'h':
			case '?':
			default:
				// usage(argv[0]);
				return EX_USAGE;
		}
	}

	if (argc - optind != 1)
	{
		// usage(argv[0]);
		return EX_USAGE;
	}
	if (tpLocalCursor)
	{
		/* there is no point wasting bandwidth on cursor shadows
		 * that we're just going to throw out anyway */
		tpPerformanceFlags |= PERF_DISABLE_CURSOR_SHADOW;
	}

	STRNCPY(server, argv[optind], sizeof(server));
	tprdpParseServerAndPort(server);

	if (tpSeamlessRDP)
	{
		if (shell[0])
			STRNCPY(tpSeamlessShellSpawn, shell, sizeof(tpSeamlessShellSpawn));

		STRNCPY(shell, tpSeamlessShell, sizeof(shell));

		if (tpButtonSize)
		{
			logger(Core, Error, "You cannot use -S and -A at the same time");
			return EX_USAGE;
		}
		tpPerformanceFlags &= ~PERF_DISABLE_FULLWINDOWDRAG;
		if (geometry_option)
		{
			logger(Core, Error, "You cannot use -g and -A at the same time");
			return EX_USAGE;
		}
		if (tpFullscreen)
		{
			logger(Core, Error, "You cannot use -f and -A at the same time");
			return EX_USAGE;
		}
		if (tpHideDecorations)
		{
			logger(Core, Error, "You cannot use -D and -A at the same time");
			return EX_USAGE;
		}
		if (tpEmbedWindow)
		{
			logger(Core, Error, "You cannot use -X and -A at the same time");
			return EX_USAGE;
		}
		if (tpRDPVersion < RDP_V5)
		{
			logger(Core, Error, "You cannot use -4 and -A at the same time");
			return EX_USAGE;
		}

		tpWindowType = Fullscreen;
		tpKeyboardGrab = False;
	}

	if (!username_option)
	{
		pw = getpwuid(getuid());
		if ((pw == NULL) || (pw->pw_name == NULL))
		{
			logger(Core, Error,
			       "could not determine username, use -u <username> to set one");
			return EX_OSERR;
		}
		/* +1 for trailing \0 */
		int pwlen = strlen(pw->pw_name) + 1;
		tpUsername = (char *) xmalloc(pwlen);
		STRNCPY(tpUsername, pw->pw_name, pwlen);
	}

	if (tpCodepage[0] == 0)
	{
		if (setlocale(LC_CTYPE, ""))
		{
			STRNCPY(tpCodepage, nl_langinfo(CODESET), sizeof(tpCodepage));
		}
		else
		{
			STRNCPY(tpCodepage, DEFAULT_CODEPAGE, sizeof(tpCodepage));
		}
	}

	if (tpHostname[0] == 0)
	{
		if (gethostname(fullhostname, sizeof(fullhostname)) == -1)
		{
			logger(Core, Error, "could not determine local hostname, use -n\n");
			return EX_OSERR;
		}

		p = strchr(fullhostname, '.');
		if (p != NULL)
			*p = 0;

		STRNCPY(tpHostname, fullhostname, sizeof(tpHostname));
	}

	if (tpKeymap[0] == 0)
	{
		if (locale && xkeymap_from_locale(locale))
		{
			logger(Core, Notice, "Autoselecting keyboard map '%s' from locale",
			       tpKeymap);
		}
		else
		{
			STRNCPY(tpKeymap, "en-us", sizeof(tpKeymap));
		}
	}
	if (locale)
		xfree(locale);

	if (prompt_password)
	{
		if (tprdpReadPassword(tpPassword, sizeof(tpPassword)))
		{
			flags |= RDP_INFO_AUTOLOGON;
		}
		else
		{
			logger(Core, Error, "Failed to read password or pin from stdin");
			return EX_OSERR;
		}
	}

	if (tpTitle[0] == 0)
	{
		strcpy(tpTitle, "tprdp - ");
		strncat(tpTitle, server, sizeof(tpTitle) - sizeof("tprdp - "));
	}

	/* Only startup ctrl functionality is seamless are used for now. */
	if (tpCtrlMode && tpSeamlessRDP)
	{
		if (ctrl_init(server, domain, tpUsername) < 0)
		{
			logger(Core, Error, "Failed to initialize ctrl mode");
			exit(1);
		}

		if (ctrl_is_slave())
		{
			logger(Core, Notice,
			       "tprdp in slave mode sending command to master process");

			if (tpSeamlessShellSpawn[0])
				return ctrl_send_command("seamless.spawn", tpSeamlessShellSpawn);

			logger(Core, Notice, "No command specified to be spawned in seamless mode");
			return EX_USAGE;
		}
	}

	if (!ui_init())
		return EX_OSERR;

#ifdef WITH_RDPSND
	if (!rdpsnd_init(rdpsnd_optarg))
		logger(Core, Warning, "Initializing sound-support failed");
#endif

	if (tpLSPCIEnabled)
		lspci_init();

	rdpdr_init();

	dvc_init();
	rdpedisp_init();

	tpSetupRequestedSessionSize();

	tpReconnectLoop = False;
	while (1)
	{
		tprdpResetState();

		if (tpRedirect)
		{
			STRNCPY(domain, tpRedirectDomain, sizeof(domain));
			xfree(tpUsername);
			tpUsername = (char *) xmalloc(strlen(tpRedirectUsername) + 1);
			strcpy(tpUsername, tpRedirectUsername);
			STRNCPY(server, tpRedirectServer, sizeof(server));
			flags |= RDP_INFO_AUTOLOGON;

			logger(Core, Notice, "Redirected to %s@%s session %d.",
			       tpRedirectUsername, tpRedirectServer, tpRedirectSessionID);

			/* A redirect on SSL from a 2003 WTS will result in a 'connection reset by peer'
			   and therefor we just clear this error before we connect to redirected server.
			 */
			tpNetworkError = False;
		}

		utils_apply_session_size_limitations(&tpSessionWidth,
						     &tpSessionHeight);

		if (!rdp_connect
		    (server, flags, domain, tpPassword, shell, directory, tpReconnectLoop))
		{

			tpNetworkError = False;

			if (tpReconnectLoop == False)
				return EX_PROTOCOL;

			/* check if auto reconnect cookie has timed out */
			if (time(NULL) - tpReconnectRandomTS > TIMEOUT)
			{
				logger(Core, Notice,
				       "Tried to reconnect for %d minutes, giving up.",
				       TIMEOUT / 60);
				return EX_PROTOCOL;
			}

			sleep(4);
			continue;
		}

		if (tpRedirect)
		{
			rdp_disconnect();
			continue;
		}

		/* By setting encryption to False here, we have an encrypted login
		   packet but unencrypted transfer of other packets */
		if (!tpEncryptionPacket)
			tpEncryptionInitial = tpEncryption = False;

		logger(Core, Verbose, "Connection successful");

		rd_create_ui();
		tcp_run_ui(True);

		deactivated = False;
		tpReconnectLoop = False;
		ext_disc_reason = ERRINFO_UNSET;
		rdp_main_loop(&deactivated, &ext_disc_reason);

		tcp_run_ui(False);

		logger(Core, Verbose, "Disconnecting...");
		rdp_disconnect();

		/* Version <= Windows 2008 server have a different behaviour for
		   user initiated disconnected. Lets translate this specific
		   behaviour into the same as for later versions for proper
		   handling.
		 */
		if (deactivated == True && ext_disc_reason == ERRINFO_NO_INFO)
		{
			deactivated = 0;
			ext_disc_reason = ERRINFO_LOGOFF_BYUSER;
		}
		else if (ext_disc_reason == 0)
		{
			/* We do not know how to handle error info value of 0 */
			ext_disc_reason = ERRINFO_UNSET;
		}

		/* Handler disconnect */
		if (tpUserQuit || deactivated == True || ext_disc_reason != ERRINFO_UNSET)
		{
			/* We should exit the tprdp instance */
			break;
		}
		else
		{
			/* We should handle a reconnect for any reason */
			if (tpRedirect)
			{
				logger(Core, Verbose, "Redirect reconnect loop triggered.");
			}
			else if (tpNetworkError)
			{
				if (tpReconnectRandomTS == 0)
				{
					/* If there is no auto reconnect cookie available
					   for reconnect, do not enter reconnect loop. Windows
					   2016 server does not send any for unknown reasons.
					 */
					logger(Core, Notice,
					       "Disconnected due to network error, exiting...");
					break;
				}

				/* handle network error and start autoreconnect */
				logger(Core, Notice,
				       "Disconnected due to network error, retrying to reconnect for %d minutes.",
				       TIMEOUT / 60);
				tpNetworkError = False;
				tpReconnectLoop = True;
			}
			else if (tpPendingResize)
			{
				/* Enter a reconnect loop if we have a pending resize request */
				logger(Core, Verbose,
				       "Resize reconnect loop triggered, new size %dx%d",
				       tpSessionWidth, tpSessionHeight);
				tpPendingResize = False;
				tpReconnectLoop = True;

				ui_seamless_end();
				ui_destroy_window();
			}
			else
			{
				logger(Core, Debug, "Unhandled reconnect reason, exiting...");
				break;
			}
		}
	}

	ui_seamless_end();
	ui_destroy_window();

	cache_save_state();
	ui_deinit();

	if (tpUserQuit)
		return EXRD_WINDOW_CLOSED;

	return tpDisconnectReason(deactivated, ext_disc_reason);

	if (tpRedirectUsername)
		xfree(tpRedirectUsername);

	xfree(tpUsername);
}

#ifdef EGD_SOCKET
/* Read 32 random bytes from PRNGD or EGD socket (based on OpenSSL RAND_egd) */
static RD_BOOL
generate_random_egd(uint8 * buf)
{
	struct sockaddr_un addr;
	RD_BOOL ret = False;
	int fd;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
		return False;

	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, EGD_SOCKET, sizeof(EGD_SOCKET));
	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
		goto err;

	/* PRNGD and EGD use a simple communications protocol */
	buf[0] = 1;		/* Non-blocking (similar to /dev/urandom) */
	buf[1] = 32;		/* Number of requested random bytes */
	if (write(fd, buf, 2) != 2)
		goto err;

	if ((read(fd, buf, 1) != 1) || (buf[0] == 0))	/* Available? */
		goto err;

	if (read(fd, buf, 32) != 32)
		goto err;

	ret = True;

      err:
	close(fd);
	return ret;
}
#endif

/* Generate a 32-byte random for the secure transport code. */
void
generate_random(uint8 * random)
{
	struct stat st;
	struct tms tmsbuf;
	RDSSL_MD5 md5;
	uint32 *r;
	int fd, n;

	/* If we have a kernel random device, try that first */
	if (((fd = open("/dev/urandom", O_RDONLY)) != -1)
	    || ((fd = open("/dev/random", O_RDONLY)) != -1))
	{
		n = read(fd, random, 32);
		close(fd);
		if (n == 32)
			return;
	}

#ifdef EGD_SOCKET
	/* As a second preference use an EGD */
	if (generate_random_egd(random))
		return;
#endif

	/* Otherwise use whatever entropy we can gather - ideas welcome. */
	r = (uint32 *) random;
	r[0] = (getpid()) | (getppid() << 16);
	r[1] = (getuid()) | (getgid() << 16);
	r[2] = times(&tmsbuf);	/* system uptime (clocks) */
	gettimeofday((struct timeval *) &r[3], NULL);	/* sec and usec */
	stat("/tmp", &st);
	r[5] = st.st_atime;
	r[6] = st.st_mtime;
	r[7] = st.st_ctime;

	/* Hash both halves with MD5 to obscure possible patterns */
	rdssl_md5_init(&md5);
	rdssl_md5_update(&md5, random, 16);
	rdssl_md5_final(&md5, random);
	rdssl_md5_update(&md5, random + 16, 16);
	rdssl_md5_final(&md5, random + 16);
}

/* malloc; exit if out of memory */
void *
xmalloc(int size)
{
	void *mem = malloc(size);
	if (mem == NULL)
	{
		logger(Core, Error, "xmalloc, failed to allocate %d bytes", size);
		exit(EX_UNAVAILABLE);
	}
	return mem;
}

/* Exit on NULL pointer. Use to verify result from XGetImage etc */
void
exit_if_null(void *ptr)
{
	if (ptr == NULL)
	{
		logger(Core, Error, "unexpected null pointer. Out of memory?");
		exit(EX_UNAVAILABLE);
	}
}

/* strdup */
char *
xstrdup(const char *s)
{
	char *mem = strdup(s);
	if (mem == NULL)
	{
		logger(Core, Error, "xstrdup(), strdup() failed: %s", strerror(errno));
		exit(EX_UNAVAILABLE);
	}
	return mem;
}

/* realloc; exit if out of memory */
void *
xrealloc(void *oldmem, size_t size)
{
	void *mem;

	if (size == 0)
		size = 1;
	mem = realloc(oldmem, size);
	if (mem == NULL)
	{
		logger(Core, Error, "xrealloc, failed to reallocate %ld bytes", size);
		exit(EX_UNAVAILABLE);
	}
	return mem;
}

/* free */
void
xfree(void *mem)
{
	free(mem);
}

/* produce a hex dump */
void
hexdump(unsigned char *p, unsigned int len)
{
	unsigned char *line = p;
	unsigned offset = 0;
	int i, thisline;

	while (offset < len)
	{
		printf("%04x ", offset);
		thisline = len - offset;
		if (thisline > 16)
			thisline = 16;

		for (i = 0; i < thisline; i++)
			printf("%02x ", line[i]);

		for (; i < 16; i++)
			printf("   ");

		for (i = 0; i < thisline; i++)
			printf("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');

		printf("\n");
		offset += thisline;
		line += thisline;
	}
}

/*
  input: src is the string we look in for needle.
  	 Needle may be escaped by a backslash, in
	 that case we ignore that particular needle.
  return value: returns next src pointer, for
  	successive executions, like in a while loop
	if retval is 0, then there are no more args.
  pitfalls:
  	src is modified. 0x00 chars are inserted to
	terminate strings.
	return val, points on the next val chr after ins
	0x00

	example usage:
	while( (pos = next_arg( optarg, ',')) ){
		printf("%s\n",optarg);
		optarg=pos;
	}

*/
char *
next_arg(char *src, char needle)
{
	char *nextval;
	char *p;
	char *mvp = 0;

	/* EOS */
	if (*src == (char) 0x00)
		return 0;

	p = src;
	/*  skip escaped needles */
	while ((nextval = strchr(p, needle)))
	{
		mvp = nextval - 1;
		/* found backslashed needle */
		if (*mvp == '\\' && (mvp > src))
		{
			/* move string one to the left */
			while (*(mvp + 1) != (char) 0x00)
			{
				*mvp = *(mvp + 1);
				mvp++;
			}
			*mvp = (char) 0x00;
			p = nextval;
		}
		else
		{
			p = nextval + 1;
			break;
		}

	}

	/* more args available */
	if (nextval)
	{
		*nextval = (char) 0x00;
		return ++nextval;
	}

	/* no more args after this, jump to EOS */
	nextval = src + strlen(src);
	return nextval;
}


void
toupper_str(char *p)
{
	while (*p)
	{
		if ((*p >= 'a') && (*p <= 'z'))
			*p = toupper((int) *p);
		p++;
	}
}


RD_BOOL
str_startswith(const char *s, const char *prefix)
{
	return (strncmp(s, prefix, strlen(prefix)) == 0);
}


/* Split input into lines, and call linehandler for each
   line. Incomplete lines are saved in the rest variable, which should
   initially point to NULL. When linehandler returns False, stop and
   return False. Otherwise, return True.  */
RD_BOOL
str_handle_lines(const char *input, char **rest, str_handle_lines_t linehandler, void *data)
{
	char *buf, *p;
	char *oldrest;
	size_t inputlen;
	size_t buflen;
	size_t restlen = 0;
	RD_BOOL ret = True;

	/* Copy data to buffer */
	inputlen = strlen(input);
	if (*rest)
		restlen = strlen(*rest);
	buflen = restlen + inputlen + 1;
	buf = (char *) xmalloc(buflen);
	buf[0] = '\0';
	if (*rest)
		STRNCPY(buf, *rest, buflen);
	strncat(buf, input, buflen);
	p = buf;

	while (1)
	{
		char *newline = strchr(p, '\n');
		if (newline)
		{
			*newline = '\0';
			if (!linehandler(p, data))
			{
				p = newline + 1;
				ret = False;
				break;
			}
			p = newline + 1;
		}
		else
		{
			break;

		}
	}

	/* Save in rest */
	oldrest = *rest;
	restlen = buf + buflen - p;
	*rest = (char *) xmalloc(restlen);
	STRNCPY((*rest), p, restlen);
	xfree(oldrest);

	xfree(buf);
	return ret;
}

/* Execute the program specified by argv. For each line in
   stdout/stderr output, call linehandler. Returns false on failure. */
RD_BOOL
subprocess(char *const argv[], str_handle_lines_t linehandler, void *data)
{
	pid_t child;
	int fd[2];
	int n = 1;
	char output[256];
	char *rest = NULL;

	if (pipe(fd) < 0)
	{
		logger(Core, Error, "subprocess(), pipe() failed: %s", strerror(errno));
		return False;
	}

	if ((child = fork()) < 0)
	{
		logger(Core, Error, "subprocess(), fork() failed: %s", strerror(errno));
		return False;
	}

	/* Child */
	if (child == 0)
	{
		/* Close read end */
		close(fd[0]);

		/* Redirect stdout and stderr to pipe */
		dup2(fd[1], 1);
		dup2(fd[1], 2);

		/* Execute */
		execvp(argv[0], argv);
		logger(Core, Error, "subprocess(), execvp() failed: %s", strerror(errno));
		_exit(128);
	}

	/* Parent. Close write end. */
	close(fd[1]);
	while (n > 0)
	{
		n = read(fd[0], output, 255);
		output[n] = '\0';
		str_handle_lines(output, &rest, linehandler, data);
	}
	xfree(rest);

	return True;
}


/* not all clibs got ltoa */
#define LTOA_BUFSIZE (sizeof(long) * 8 + 1)

char *
l_to_a(long N, int base)
{
	static char ret[LTOA_BUFSIZE];

	char *head = ret, buf[LTOA_BUFSIZE], *tail = buf + sizeof(buf);

	register int divrem;

	if (base > 36 || 2 > base)
		base = 10;

	if (N < 0)
	{
		*head++ = '-';
		N = -N;
	}

	tail = buf + sizeof(buf);
	*--tail = 0;

	do
	{
		divrem = N % base;
		*--tail = (divrem <= 9) ? divrem + '0' : divrem + 'a' - 10;
		N /= base;
	}
	while (N);

	strcpy(head, tail);
	return ret;
}

int
load_licence(unsigned char **data)
{
	uint8 ho[20], hi[16];
	char *home, path[PATH_MAX], hash[41];
	struct stat st;
	int fd, length;

	home = getenv("HOME");
	if (home == NULL)
		return -1;

	memset(hi, 0, sizeof(hi));
	snprintf((char *) hi, 16, "%s", tpHostname);
	sec_hash_sha1_16(ho, hi, tprdpStaticSalt);
	sec_hash_to_string(hash, sizeof(hash), ho, sizeof(ho));

	snprintf(path, PATH_MAX, "%s" TPRDP_STORE "/%s.cal", home, hash);
	path[sizeof(path) - 1] = '\0';

	fd = open(path, O_RDONLY);
	if (fd == -1)
	{
		/* fallback to try reading old license file */
		snprintf(path, PATH_MAX, "%s/.tprdp/license.%s", home, tpHostname);
		path[sizeof(path) - 1] = '\0';
		if ((fd = open(path, O_RDONLY)) == -1)
			return -1;
	}

	if (fstat(fd, &st))
	{
		close(fd);
		return -1;
	}

	*data = (uint8 *) xmalloc(st.st_size);
	length = read(fd, *data, st.st_size);
	close(fd);
	return length;
}

void
save_licence(unsigned char *data, int length)
{
	uint8 ho[20], hi[16];
	char *home, path[PATH_MAX], tmppath[PATH_MAX], hash[41];
	int fd;

	home = getenv("HOME");
	if (home == NULL)
		return;

	snprintf(path, PATH_MAX, "%s" TPRDP_STORE, home);
	path[sizeof(path) - 1] = '\0';
	if (utils_mkdir_p(path, 0700) == -1)
	{
		logger(Core, Error, "save_license(), utils_mkdir_p() failed: %s", strerror(errno));
		return;
	}

	memset(hi, 0, sizeof(hi));
	snprintf((char *) hi, 16, "%s", tpHostname);
	sec_hash_sha1_16(ho, hi, tprdpStaticSalt);
	sec_hash_to_string(hash, sizeof(hash), ho, sizeof(ho));

	/* write licence to {sha1}.cal.new, then atomically
	   rename to {sha1}.cal */
	snprintf(path, PATH_MAX, "%s" TPRDP_STORE "/%s.cal", home, hash);
	path[sizeof(path) - 1] = '\0';

	snprintf(tmppath, PATH_MAX, "%s.new", path);
	path[sizeof(path) - 1] = '\0';

	fd = open(tmppath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd == -1)
	{
		logger(Core, Error, "save_license(), open() failed: %s", strerror(errno));
		return;
	}

	if (write(fd, data, length) != length)
	{
		logger(Core, Error, "save_license(), write() failed: %s", strerror(errno));
		unlink(tmppath);
	}
	else if (rename(tmppath, path) == -1)
	{
		logger(Core, Error, "save_license(), rename() failed: %s", strerror(errno));
		unlink(tmppath);
	}

	close(fd);
}

/* create tprdp ui */
void
rd_create_ui()
{
	if (!ui_have_window())
	{
		/* create a window if we don't have one initialized */
		if (!ui_create_window(tpSessionWidth, tpSessionHeight))
			exit(EX_OSERR);
	}
	else
	{
		/* reset clipping if we already have a window */
		ui_reset_clip();
	}
}

/* TODO: Replace with recursive mkdir */
RD_BOOL rd_certcache_mkdir(void)
{
	char *home;
	char certcache_dir[PATH_MAX];

	home = getenv("HOME");

	if (home == NULL)
		return False;

	snprintf(certcache_dir, sizeof(certcache_dir) - 1, "%s/%s", home, ".local");

	if ((mkdir(certcache_dir, S_IRWXU) == -1) && errno != EEXIST)
	{
		logger(Core, Error, "%s: mkdir() failed: %s", __func__, strerror(errno));
		return False;
	}

	snprintf(certcache_dir, sizeof(certcache_dir) - 1, "%s/%s", home, ".local/share");

	if ((mkdir(certcache_dir, S_IRWXU) == -1) && errno != EEXIST)
	{
		logger(Core, Error, "%s: mkdir() failed: %s", __func__, strerror(errno));
		return False;
	}

	snprintf(certcache_dir, sizeof(certcache_dir) - 1, "%s/%s", home, ".local/share/tprdp");

	if ((mkdir(certcache_dir, S_IRWXU) == -1) && errno != EEXIST)
	{
		logger(Core, Error, "%s: mkdir() failed: %s", __func__, strerror(errno));
		return False;
	}

	snprintf(certcache_dir, sizeof(certcache_dir) - 1, "%s/%s", home, ".local/share/tprdp/certs");

	if ((mkdir(certcache_dir, S_IRWXU) == -1) && errno != EEXIST)
	{
		logger(Core, Error, "%s: mkdir() failed: %s", __func__, strerror(errno));
		return False;
	}

	return True;
}

/* Create the bitmap cache directory */
RD_BOOL
rd_pstcache_mkdir(void)
{
	char *home;
	char bmpcache_dir[256];

	home = getenv("HOME");

	if (home == NULL)
		return False;

	sprintf(bmpcache_dir, "%s/%s", home, ".tprdp");

	if ((mkdir(bmpcache_dir, S_IRWXU) == -1) && errno != EEXIST)
	{
		logger(Core, Error, "rd_pstcache_mkdir(), mkdir() failed: %s", strerror(errno));
		return False;
	}

	sprintf(bmpcache_dir, "%s/%s", home, ".tprdp/cache");

	if ((mkdir(bmpcache_dir, S_IRWXU) == -1) && errno != EEXIST)
	{
		logger(Core, Error, "rd_pstcache_mkdir(), mkdir() failed: %s", strerror(errno));
		return False;
	}

	return True;
}

/* open a file in the .tprdp directory */
int
rd_open_file(char *filename)
{
	char *home;
	char fn[256];
	int fd;

	home = getenv("HOME");
	if (home == NULL)
		return -1;
	sprintf(fn, "%s/.tprdp/%s", home, filename);
	fd = open(fn, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1)
		logger(Core, Error, "rd_open_file(), open() failed: %s", strerror(errno));

	return fd;
}

/* close file */
void
rd_close_file(int fd)
{
	close(fd);
}

/* read from file*/
int
rd_read_file(int fd, void *ptr, int len)
{
	return read(fd, ptr, len);
}

/* write to file */
int
rd_write_file(int fd, void *ptr, int len)
{
	return write(fd, ptr, len);
}

/* move file pointer */
int
rd_lseek_file(int fd, int offset)
{
	return lseek(fd, offset, SEEK_SET);
}

/* do a write lock on a file */
RD_BOOL
rd_lock_file(int fd, int start, int len)
{
	struct flock lock;

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = start;
	lock.l_len = len;
	if (fcntl(fd, F_SETLK, &lock) == -1)
		return False;
	return True;
}
