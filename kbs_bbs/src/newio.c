/*
底层的I/O库。
		KCN重写
*/

#include "bbs.h"
#include <arpa/telnet.h>

#ifdef AIX
#include <sys/select.h>
#endif

/*输入输出缓冲区的大小
  输出缓冲区给个半屏大小足以，
  输入缓冲区一般都利用不上，给
  个128字节吧
*/
#define OBUFSIZE  (1024*2)
#define IBUFSIZE  (128)

#define INPUT_ACTIVE 0
#define INPUT_IDLE 1

extern int temp_numposts;

char outbuffer[OBUFSIZE + 1];
char *outbuf = outbuffer + 1;
int obufsize = 0;

char inbuffer[IBUFSIZE + 1];
char *inbuf = inbuffer + 1;
int ibufsize = 0;

int icurrchar = 0;
int KEY_ESC_arg;

int idle_count = 0;

static time_t old;
static time_t lasttime = 0;

extern int convcode;
extern char *big2gb(char *, int *, int);
extern char *gb2big(char *, int *, int);
extern int ssh_sock;
void oflush()
{
    if (obufsize) {
        if (convcode) {
            char *out;

            out = gb2big(outbuf, &obufsize, 0);
#ifdef SSHBBS
            if (ssh_write(0, out, obufsize) < 0)
#else
            if (write(0, out, obufsize) < 0)
#endif
                abort_bbs(0);
        } else
#ifdef SSHBBS
        if (ssh_write(0, outbuf, obufsize) < 0)
#else
        if (write(0, outbuf, obufsize) < 0)
#endif
            abort_bbs(0);
    }
    obufsize = 0;
}

void ochar(char c)
{
    if (obufsize > OBUFSIZE - 1) {      /* doin a oflush */
        oflush();
    }
    outbuf[obufsize++] = c;
    /*
     * need to change IAC to IAC IAC 
     */
    if (((unsigned char) c) == IAC) {
        if (obufsize > OBUFSIZE - 1) {  /* doin a oflush */
            oflush();
        }
        outbuf[obufsize++] = c;
    }
}

#define ZMODEM_RATE 5000
int ZmodemRateLimit = 1;
int raw_write(int fd, char *buf, int len)
{
    static int lastcounter = 0;
    int nowcounter, i;
    static int bufcounter;
    int retlen=0;
#ifndef NINE_BUILD
    if (ZmodemRateLimit) {
        nowcounter = time(0);
        if (lastcounter == nowcounter) {
            if (bufcounter >= ZMODEM_RATE) {
                sleep(1);
                nowcounter = time(0);
                bufcounter = len;
            } else
                bufcounter += len;
        } else {
            /*
             * time clocked, clear bufcounter 
             */
            bufcounter = len;
        }
        lastcounter = nowcounter;
    }
#endif    
#ifdef SSHBBS
    return ssh_write(fd, buf, len);
#else
    for (i = 0; i < len; i++) {
        int mylen;

        if ((unsigned char) buf[i] == 0xff)
            mylen = write(fd, "\xff\xff", 2);
        else if (buf[i] == 13)
            mylen = write(fd, "\x0d\x00", 2);
        else
            mylen = write(fd, &buf[i], 1);
        if (mylen < 0)
            break;
        retlen += mylen;
    }
    return retlen;
#endif
}

void raw_ochar(char c)
{
    raw_write(0, &c, 1);
}

int raw_read(int fd, char *buf, int len)
{
    int i,j,retlen=0,pp=0;
#ifdef SSHBBS
    return ssh_read(fd, buf, len);
#else
    retlen = read(fd,buf,len);
    for(i=0;i<retlen;i++) {
        if(i>0&&((unsigned char)buf[i-1]==0xff)&&((unsigned char)buf[i]==0xff)) {
            retlen--;
            for(j=i;j<retlen;j++)
                buf[j]=buf[j+1];
            continue;
        }
        if(i>0&&buf[i-1]==0x0d&&buf[i]==0x00) {
            retlen--;
            for(j=i;j<retlen;j++)
                buf[j]=buf[j+1];
            continue;
        }
    }
    return retlen;
#endif
}

void output(s, len)
char *s;
int len;
{
    /*
     * need to change IAC to IAC IAC
     * if(obufsize+len > OBUFSIZE) {
     * #ifdef SSHBBS
     * ssh_write(0,outbuf,obufsize) ;
     * #else
     * write(0,outbuf,obufsize) ;
     * #endif
     * obufsize = 0 ;
     * }
     * memcpy(outbuf+obufsize, s, len) ;
     * obufsize+=len ; 
     */
    int i;

    for (i = 0; i < len; i++)
        ochar(s[i]);
}


int i_newfd = 0;
static void (*flushf) () = NULL;

int i_timeout = 0;
static int i_timeoutusec = 0;
static time_t i_begintimeout;
static void (*i_timeout_func) (void *);
static struct timeval i_to, *i_top = NULL;
static void *timeout_data;

void add_io(int fd, int timeout)
{
    i_newfd = fd;
    if (timeout) {
        i_to.tv_sec = timeout;
        i_to.tv_usec = 0;
        i_top = &i_to;
    } else
        i_top = NULL;
}

void add_flush(void (*flushfunc) ())
{
    flushf = flushfunc;
}

void set_alarm(int set_timeout, int set_timeoutusec,void (*timeout_func) (void *), void *data)
{
    i_timeout = set_timeout;
    i_timeoutusec=set_timeoutusec;
    i_begintimeout = time(0);
    i_timeout_func = timeout_func;
    timeout_data = data;
}

int num_in_buf()
{
    return icurrchar - ibufsize ; 
}

int telnet_state = 0;
char lastch;
int naw_col, naw_ln, naw_changed=0;

static int telnet_machine(unsigned char ch)
{
    switch (telnet_state) {
    case 255:                  /* after the first IAC */
        switch (ch) {
        case DO:
        case DONT:
        case WILL:
        case WONT:
            telnet_state = 1;
            break;
        case SB:               /* loop forever looking for the SE */
            telnet_state = 2;
            break;
        case IAC:
            return IAC;
        default:
            telnet_state = 0;   /* other telnet command */
        }
        break;
    case 1:                    /* Get the DO,DONT,WILL,WONT */
        telnet_state = 0;       /* the ch is the telnet option */
        break;
    case 2:                    /* the telnet suboption */
        if (ch == 31)
            telnet_state = 5;   /* wait for windows size */
        else if (ch == IAC)
            telnet_state = 3;   /* wait for SE */
        else
            telnet_state = 4;   /* filter telnet SB data */
        break;
    case 3:                    /* wait for se */
        if (ch == SE) {
            telnet_state = 0;
            if(naw_changed) {
                naw_changed = 0;
                do_naws(naw_ln, naw_col);
            }
        }
        else
            telnet_state = 4;
        break;
    case 4:                    /* telnet SB data */
        if (ch == IAC)
            telnet_state = 3;   /* wait for SE */
        break;
    case 5:
        naw_changed = 1;
        telnet_state = 6;
        break;
    case 6:
        naw_col = ch;
        if (ch == IAC)
            telnet_state = 4;
        else
            telnet_state = 7;
        break;
    case 7:
        if (ch == IAC)
            telnet_state = 4;
        else
            telnet_state = 8;
        break;
    case 8:
        naw_ln = ch;
        if (ch == IAC)
            telnet_state = 4;
        else
            telnet_state = 2;
        break;
    }
    return 0;
}

int filter_telnet(char *s, int *len)
{
    unsigned char *p1, *p2, *pend;
    int newlen;

    newlen = 0;
    for (p1 = (unsigned char *) s, p2 = (unsigned char *) s, pend = (unsigned char *) s + (*len); p1 != pend; p1++) {
        if (telnet_state) {
            int ch = 0;

            ch = telnet_machine(*p1);
            if (ch == IAC) {    /* 两个IAC */
                *p2 = IAC;
                p2++;
                newlen++;
            }
        } else {
            if (*p1 == IAC)
                telnet_state = 255;
            else {
                *p2 = *p1;
                p2++;
                newlen++;
            }
        }
    }
    return (*len = newlen);
}

bool inremsg = false;

struct key_struct *keymem=NULL;
int keymem_total;
int kicked=0;
int incalendar=0;

void ktimeout(void *data)
{
    kicked = 1;
}

int igetch()
{
    time_t now;
    char c;
    int hasaddio = 1;
    extern int RMSG;


    if ((uinfo.mode == CHAT1 || uinfo.mode == TALK || uinfo.mode == PAGE) && RMSG == true)
        hasaddio = 0;

  igetagain:
    if (ibufsize == icurrchar) {
        fd_set readfds, xds;
        struct timeval to;
        int sr, hifd;

        to.tv_sec = 0;
        to.tv_usec = 0;
        hifd = 1;
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);
        if ((hasaddio && (i_newfd))&&(!inremsg)) {
            FD_SET(i_newfd, &readfds);
            if (hifd <= i_newfd)
                hifd = i_newfd + 1;
        }
	//TODO: igetkey重入问题
        if (scrint&&!inremsg) {
            while (msg_count) {
                inremsg = true;
                msg_count--;
                r_msg();
                refresh();
                inremsg = false;
            }
        }
        if(kicked) return KEY_TIMEOUT;
        sr = select(hifd, &readfds, NULL, NULL, &to);
        if (sr < 0 && errno == EINTR) {
            if (talkrequest)
                return KEY_TALK;
            if (scrint&&!inremsg) {
/*这种msg处理仍然有同步问题，如果while判断完msg_count==0,
 * goto igetagain到select之间，发生了信号，那么，这个还是
 * 会丢失
 */
                while (msg_count) {
                    inremsg = true;
                    msg_count--;
                    r_msg();
                    inremsg = false;
                }
                goto igetagain;
            }
        }
        if (sr < 0 && errno != EINTR)
            abort_bbs(0);
        if (sr == 0) {
            refresh();
            if (flushf)
                (*flushf) ();

            while (1) {
                int alarm_timeout;

                hifd = 1;
                FD_ZERO(&xds);
                FD_SET(0, &xds);
                FD_ZERO(&readfds);
                FD_SET(0, &readfds);
                if ((hasaddio && (i_newfd))&&(!inremsg)) {
                    FD_SET(i_newfd, &readfds);
                    if (hifd <= i_newfd)
                        hifd = i_newfd + 1;
                }
                alarm_timeout = 0;
                if (i_top)
                    to = *i_top;
                else {
                    while ((i_timeout != 0)||(i_timeoutusec!=0)) {
                        to.tv_sec = i_timeout - (time(0) - i_begintimeout);
                        to.tv_usec = i_timeoutusec;
                        if ((to.tv_sec < 0) ||((to.tv_sec==0)&&(i_timeoutusec==0))){
                            i_timeout = 0;
                            i_timeoutusec=0;
                            if (i_timeout_func)
                            	(*i_timeout_func) (timeout_data);
                            else
                            	return KEY_TIMEOUT;
                            if(kicked) return KEY_TIMEOUT;
                            continue;
                        };
                        alarm_timeout = 1;
                        break;
                    };
                    if (!alarm_timeout)
                        to.tv_sec = IDLE_TIMEOUT;
                }
                sr = select(hifd, &readfds, NULL, &xds, &to);
                if (sr < 0 && errno == EINTR) {
                    if (talkrequest)
                        return KEY_TALK;
                }
                if(kicked) return KEY_TIMEOUT;
                if (!inremsg) {
		      int saveerrno=errno;
                    while (msg_count) {
                        inremsg = true;
                        msg_count--;
                        r_msg();
                        refresh();
                        inremsg = false;
                    }
                    if (sr<0&&saveerrno==EINTR)continue;
                }
                if (sr == 0 && alarm_timeout) {
                    i_timeout = 0;
                    i_timeoutusec=0;
                    if (i_timeout_func)
                    	(*i_timeout_func) (timeout_data);
                    else
                    	return KEY_TIMEOUT;
                    continue;
                }
                if (sr >= 0)
                    break;
                if (errno == EINTR)
                    continue;
                else
                    abort_bbs(0);

            }
            if ((sr == 0) && (!i_top))
                abort_bbs(0);
            if (sr == 0)
                return I_TIMEOUT;
            if (FD_ISSET(0, &xds))
                abort_bbs(0);
        }
        if (hasaddio && (i_newfd && FD_ISSET(i_newfd, &readfds)))
            return I_OTHERDATA;
#ifdef SSHBBS
        while ((ibufsize = ssh_read(0, inbuffer + 1, IBUFSIZE)) <= 0) {
#else
        while ((ibufsize = read(0, inbuffer + 1, IBUFSIZE)) <= 0) {
#endif
            if (ibufsize == 0)
                longjmp(byebye, -1);
            if (ibufsize < 0 && errno != EINTR)
                longjmp(byebye, -1);
        }
        if (!filter_telnet(inbuffer + 1, &ibufsize)) {
            icurrchar = 0;
            ibufsize = 0;
            goto igetagain;
        }

        /*
         * add by KCN for GB/BIG5 encode 
         */
        if (convcode) {
            inbuf = big2gb(inbuffer + 1, &ibufsize, 0);
            if (ibufsize == 0) {
                icurrchar = 0;
                goto igetagain;
            }
        } else
            inbuf = inbuffer + 1;
        /*
         * end 
         */
        icurrchar = 0;
        if (ibufsize > IBUFSIZE) {
            ibufsize = 0;
            goto igetagain;
        }
    }

    if (icurrchar >= ibufsize) {
        ibufsize = icurrchar;
        goto igetagain;
    }
    if (((inbuf[icurrchar] == '\n') && (lastch == '\r'))
        || ((inbuf[icurrchar] == '\r') && (lastch == '\n'))) {
        lastch = 0;
        goto igetagain;
    }

    else if (icurrchar != ibufsize) {
        if (((inbuf[icurrchar] == '\n') && (inbuf[icurrchar + 1] == '\r'))
            || ((inbuf[icurrchar] == '\r') && (inbuf[icurrchar + 1] == '\n'))) {
            icurrchar++;
            lastch = 0;
        }
    } else
        lastch = inbuf[icurrchar];

    idle_count = 0;
    c = inbuf[icurrchar];

    switch (c) {
    case Ctrl('L'):
        redoscr();
        icurrchar++;
#ifdef NINE_BUILD
	now = time(0);
	uinfo.freshtime = now;
	if (now - old > 60) {
	   UPDATE_UTMP(freshtime, uinfo);
	   old = now;
	}   
#endif
        goto igetagain;
    case Ctrl('Z'):
        if(scrint&&uinfo.mode!=NEW&&uinfo.mode!=LOGIN&&uinfo.mode!=BBSNET &&!inremsg) {
            icurrchar++;
            inremsg = true;
            r_msg();
            inremsg = false;
            goto igetagain;
        }
        break;
    default:
        break;
    }
    icurrchar++;
    while ((icurrchar != ibufsize) && (inbuf[icurrchar] == 0))
        icurrchar++;
    now = time(0);
    /*---	Ctrl-T disabled as anti-idle key	period	2000-12-05	---*/
#ifndef NINE_BUILD
    if (Ctrl('T') != c)
#endif	    
        uinfo.freshtime = now;
    /*
     * add by KCN , decrease temp_numposts 
     */
    if (lasttime + 60 * 60 * 8 < now) {
        lasttime = now;
        if (temp_numposts > 0)
            temp_numposts--;
    }
    if (now - old > 60) {
        UPDATE_UTMP(freshtime, uinfo);
        old = now;
    }
    return c;
}

int* keybuffer;
int keybuffer_count=0;
int skip_key=0;

int igetkey()
{
    int mode;
    int ch, last, llast;
    int ret=0;

    if(keybuffer_count) {
        ret = *keybuffer;
        keybuffer++;
        keybuffer_count--;
        return ret;
    }
    mode = last = llast = 0;
//    if (ibufsize == icurrchar)
//        refresh();
    while (1) {
        ch = igetch();
        if(kicked) return KEY_TIMEOUT;

        if(check_calltime()){
			mode = 0;
			continue;
		}

#ifdef SMTH
	if (scrint&&ch==Ctrl('V')) {
            if (currentuser&&!HAS_PERM(currentuser,PERM_DENYRELAX))
            exec_mbem("@mod:service/libdict.so#dict_main");
            continue;
        }
#endif
        if (scrint&&(ch == KEY_TALK) && talkrequest) {
            if (uinfo.mode != CHAT1 && uinfo.mode != CHAT2 && uinfo.mode != CHAT3 && uinfo.mode != CHAT4 && uinfo.mode != TALK && uinfo.mode != PAGE) {
                talkreply();
                return KEY_REFRESH;
            } else
                return KEY_TALK;
        }
        if (mode == 0) {
            if (ch == KEY_ESC) {
                if(ibufsize==icurrchar) {
                    if(uinfo.mode!=POSTING&&uinfo.mode!=SMAIL&&uinfo.mode!=EDITUFILE&&uinfo.mode!=EDITSFILE&&
                        uinfo.mode!=NOTEPAD&&uinfo.mode!=EDIT&&uinfo.mode!=EDITANN&&uinfo.mode!=RMAIL&&
                        uinfo.mode!=CALENEDIT&&uinfo.mode!=CSIE_ANNOUNCE)
                        return ch;
                }
                mode = 1;
            }
            else {
                ret = ch;
                break;      /* Normal Key */
            }
        } else if (mode == 1) { /* Escape sequence */
            if (ch == '[' || ch == 'O')
                mode = 2;
            else if (ch == '1' || ch == '4')
                mode = 3;
            else {
                KEY_ESC_arg = ch;
                return KEY_ESC;
            }
        } else if (mode == 2) { /* Cursor key */
            if (ch >= 'A' && ch <= 'D') {
               ret = KEY_UP + (ch - 'A');
               break;
            }
            else if (ch >= 'P' && ch <= 'S') {
                ret = KEY_F1+ch-'P';
                break;
            }
            else if (ch >= '1' && ch <= '6')
                mode = 3;
            else {
                ret = ch;
                break;
            }
        } else if (mode == 3) { /* Ins Del Home End PgUp PgDn */
            if (ch == '~') {
                ret = KEY_HOME + (last - '1');
                break;
            }
            else if (ch >= '0' && ch <= '9')
                mode = 4;
            else {
                ret = ch;
                break;
            }
        } else if (mode == 4) {
            if (ch == '~') {
                int k=(llast-'1')*10+(last-'1');
                if(k<=3) ret = KEY_F1+k;
                else ret = KEY_F1+k-1;
        	if (scrint&&ret==KEY_F10&&!incalendar) {
        	      mode=0;
                    if (currentuser&&!HAS_PERM(currentuser,PERM_DENYRELAX))
                    exec_mbem("@mod:service/libcalendar.so#calendar_main");
                    continue;
                }
                break;
            }
            else {
                ret = ch;
                break;
            }
        }
        llast = last;
        last = ch;
    }

    if(scrint&&keymem_total&&!skip_key) {
        int i,j,k,p;
        for(i=0;i<keymem_total;i++) {
            p=!keymem[i].status[0];
            if(keymem[i].status[0]==-1) continue;
            j=0;
            while(keymem[i].status[j]&&j<10) {
                if(keymem[i].status[j]==uinfo.mode) p=1;
                j++;
            }
            if(p&&ret==keymem[i].key) {
                j=0;
                while(keymem[i].mapped[j]&&j<10) j++;
                if(j==0) continue;
                ret = keymem[i].mapped[0];
                keybuffer_count = j-1;
                keybuffer = keymem[i].mapped+1;
                break;
            }
        }
    }
    
    return ret;
}

int getdata(int line, int col, char *prompt, char *buf, int len, int echo, void *nouse, int clearlabel)
{
    int ch, clen = 0, curr = 0, x, y;
    char tmp[STRLEN];
    extern int scr_cols;
    extern int RMSG;

    if (clearlabel == true) {
        buf[0] = 0;
    }
    if (scrint)
    move(line, col);
    if (prompt)
        prints("%s", prompt);
/*    y = line;*/
    if (scrint)
    getyx(&y, &x);
/*    col += (prompt == NULL) ? 0 : num_noans_chr(prompt);
//    x = col;*/
    clen = strlen(buf);
    curr = (clen >= len) ? len - 1 : clen;
    buf[curr] = '\0';
    prints("%s", buf);

    if (!scrint) {
        while ((ch = igetkey()) != '\r') {
            /*
             * TODO: add KEY_REFRESH support 
             */
	    if (ch == '\n')
                break;
            if (ch == '\177' || ch == Ctrl('H')) {
                if (clen == 0) {
                    continue;
                }
                clen--;
                ochar(Ctrl('H'));
                ochar(' ');
                ochar(Ctrl('H'));
                oflush();
                continue;
            }
            if (!isprint2(ch)) {
                continue;
            }
            if (clen >= len - 1) {
                continue;
            }
            buf[clen++] = ch;
            /*
             * move(line, col + clen);  Leeward 98.02.23  -- removed by wwj 2001/5/8 
             */
            if (echo)
                ochar(ch);
            else
                ochar('*');
        }
        buf[clen] = '\0';
        prints("\n");
        oflush();
        return clen;
    }
    if (!echo) {
        clrtoeol();
        while ((ch = igetkey()) != '\r') {
	    if (ch == '\n')
                break;
            if (ch == '\177' || ch == Ctrl('H')) {
                int y,x;
                if (clen == 0)
                    continue;
                clen--;
                getyx(&y,&x);
                move(y,x-1);
                outc(' ');
                move(y,x-1);
                continue;
            }
            if (!isprint2(ch)||clen>=len-1)
                continue;
            buf[clen++] = ch;
            outc('*');
        }
        buf[clen] = '\0';
        prints("\n");
        return clen;
    }
    clrtoeol();
    while (1) {
        ch = igetkey();
        /*
         * TODO: add KEY_REFRESH support ???
         */

        if(kicked) return 0;
        if (true == RMSG && (KEY_UP == ch || KEY_DOWN == ch))
            return -ch;         /* Leeward 98.07.30 supporting msgX */
        if (uinfo.mode == KILLER && (!buf[0]) && (ch==KEY_UP||ch==KEY_DOWN||ch==KEY_PGUP||ch==KEY_PGDN||ch>=Ctrl('S')&&ch<=Ctrl('W')))
            return -ch;
#ifdef NINE_BUILD
	if (true == RMSG && ch == Ctrl('Z') && clen == 0) break;
#endif
        if (ch == '\n' || ch == '\r')
            break;
#ifdef CHINESE_CHARACTER
        if (ch == Ctrl('R')) {
			currentuser->userdefine = currentuser->userdefine ^ DEF_CHCHAR;
        	continue;
        }
#endif        	
        if (ch == '\177' || ch == Ctrl('H')) {
            if (curr == 0) {
                continue;
            }
            strcpy(tmp, &buf[curr]);
            buf[--curr] = '\0';
#ifdef CHINESE_CHARACTER
			if (DEFINE(currentuser, DEF_CHCHAR)) {
				int i,j=0;
				for(i=0;i<curr;i++)
					if(j) j=0;
					else if(buf[i]<0) j=1;
				if(j) {
					buf[--curr] = '\0';
					clen--;
				}
			}
#endif
            (void) strcat(buf, tmp);
            clen--;
            move(y, x);
            prints("%s", buf);
            clrtoeol();
            move(y, x + curr);
            continue;
        }

        if(ch == KEY_ESC) {
            curr = 0;
            clen = 0;
            buf[0] = 0;
            move(y, x);
            clrtoeol();
            continue;
        }
        
        if (ch == KEY_DEL) {
            if (curr >= clen) {
                curr = clen;
                continue;
            }
            strcpy(tmp, &buf[curr + 1]);
#ifdef CHINESE_CHARACTER
			if (DEFINE(currentuser, DEF_CHCHAR)) {
				int i,j=0;
				for(i=0;i<curr+1;i++)
					if(j) j=0;
					else if(buf[i]<0) j=1;
				if(j) {
		            strcpy(tmp, &buf[curr + 2]);
					clen--;
				}
			}
#endif
            buf[curr] = '\0';
            (void) strcat(buf, tmp);
            clen--;
            move(y, x);
            prints("%s", buf);
            clrtoeol();
            move(y, x + curr);
            continue;
        }
        if (ch == KEY_LEFT) {
            if (curr == 0) {
                continue;
            }
            curr--;
#ifdef CHINESE_CHARACTER
			if (DEFINE(currentuser, DEF_CHCHAR)) {
				int i,j=0;
				for(i=0;i<curr;i++)
					if(j) j=0;
					else if(buf[i]<0) j=1;
				if(j) {
					curr--;
				}
			}
#endif
            move(y, x + curr);
            continue;
        }
        if (ch == Ctrl('E') || ch == KEY_END) {
            curr = clen;
            move(y, x + curr);
            continue;
        }
        if (ch == Ctrl('A') || ch == KEY_HOME) {
            curr = 0;
            move(y, x + curr);
            continue;
        }
        if (ch == KEY_RIGHT) {
            if (curr >= clen) {
                curr = clen;
                continue;
            }
            curr++;
#ifdef CHINESE_CHARACTER
			if (DEFINE(currentuser, DEF_CHCHAR)) {
				int i,j=0;
				for(i=0;i<curr;i++)
					if(j) j=0;
					else if(buf[i]<0) j=1;
				if(j) {
					curr++;
				}
			}
#endif
            move(y, x + curr);
            continue;
        }
        if (!isprint2(ch)) {
            continue;
        }

        if (x + clen >= scr_cols || clen >= len - 1) {
            continue;
        }

        if (!buf[curr]) {
            buf[curr + 1] = '\0';
            buf[curr] = ch;
            outc(ch);
        } else {
            /*
             * strncpy(tmp, &buf[curr], len);
             * buf[curr] = ch;
             * buf[curr + 1] = '\0';
             * strncat(buf, tmp, len - curr);
             */
            int i;

            for (i = len - 2; i >= curr; i--)
                buf[i + 1] = buf[i];
            buf[curr] = ch;
            move(y, x + curr);
            outs(buf + curr);
            move(y, x + curr + 1);
        }
        curr++;
        clen++;
        /*
         * move(y, x);
         * prints("%s", buf);
         * move(y, x + curr);
         */
    }
    buf[clen] = '\0';
    if (echo) {
        move(y, x);
        prints("%s", buf);
    }
    prints("\n");
    return clen;
}

int multi_getdata(int line, int col, int maxcol, char *prompt, char *buf, int len, int maxline, int clearlabel)
{
    int ch, clen = 0, curr = 0, x, y, startx, starty, now, i, j, k, i0, chk, cursorx, cursory;
    char savebuffer[25][LINELEN*3];
    char tmp[STRLEN];
    extern int RMSG;

    if (clearlabel == true) {
        buf[0] = 0;
    }
    move(line, col);
    if (prompt)
        prints("%s", prompt);
    getyx(&starty, &startx);
    now = strlen(buf);
    for(i=0;i<=24;i++)
        saveline(i, 0, savebuffer[i]);

    while (1) {
        y = starty; x = startx;
        move(y, x);
        chk = 0;
        if(now==0) {
            cursory = y;
            cursorx = x;
        }
        for(i=0; i<strlen(buf); i++) {
            if(chk) chk=0;
            else if(buf[i]<0) chk=1;
            if(chk&&x>=maxcol) x++;
            if(buf[i]!=13&&buf[i]!=10) {
                if(x>maxcol) {
                    clrtoeol();
                    x = col;
                    y++;
                    move(y, x);
                }
                prints("%c", buf[i]);
                x++;
            }
            else {
                clrtoeol();
                x = col;
                y++;
                move(y, x);
            }
            if(i==now-1) {
                cursory = y;
                cursorx = x;
            }
        }
        clrtoeol();
        move(cursory, cursorx);
        ch = igetkey();
        if (ch == '\n' || ch == '\r')
            break;
        for(i=starty;i<=y;i++)
            saveline(i, 1, savebuffer[i]);
        if (true == RMSG && (KEY_UP == ch || KEY_DOWN == ch) && (!buf[0]))
            return -ch;
#ifdef NINE_BUILD
        if (RMSG && (ch == Ctrl('Z')) && (!buf[0]))
            return -ch;
#endif
#ifdef CHINESE_CHARACTER
        if (ch == Ctrl('R')) {
		currentuser->userdefine = currentuser->userdefine ^ DEF_CHCHAR;
        	continue;
        }
#endif        	
        switch(ch) {
            case Ctrl('Q'):
                if(y-starty+1<maxline) {
                    for(i=strlen(buf)+1;i>now;i--)
                        buf[i]=buf[i-1];
                    buf[now++]='\n';
                }
                break;
            case KEY_UP:
                if(cursory>starty) {
                    y = starty; x = startx;
                    chk = 0;
                    if(y==cursory-1&&x<=cursorx)
                        now=0;
                    for(i=0; i<strlen(buf); i++) {
                        if(chk) chk=0;
                        else if(buf[i]<0) chk=1;
                        if(chk&&x>=maxcol) x++;
                        if(buf[i]!=13&&buf[i]!=10) {
                            if(x>maxcol) {
                                x = col;
                                y++;
                            }
                            x++;
                        }
                        else {
                            x = col;
                            y++;
                        }
#ifdef CHINESE_CHARACTER
                        if (!DEFINE(currentuser, DEF_CHCHAR)||!chk)
#endif
                        if(y==cursory-1&&x<=cursorx)
                            now=i+1;
                    }
                }
                break;
            case KEY_DOWN:
                if(cursory<y) {
                    y = starty; x = startx;
                    chk = 0;
                    if(y==cursory+1&&x<=cursorx)
                        now=0;
                    for(i=0; i<strlen(buf); i++) {
                        if(chk) chk=0;
                        else if(buf[i]<0) chk=1;
                        if(chk&&x>=maxcol) x++;
                        if(buf[i]!=13&&buf[i]!=10) {
                            if(x>maxcol) {
                                x = col;
                                y++;
                            }
                            x++;
                        }
                        else {
                            x = col;
                            y++;
                        }
#ifdef CHINESE_CHARACTER
                        if (!DEFINE(currentuser, DEF_CHCHAR)||!chk)
#endif
                        if(y==cursory+1&&x<=cursorx)
                            now=i+1;
                    }
                }
                break;
            case '\177':
            case Ctrl('H'):
                if(now>0) {
                    for(i=now-1;i<strlen(buf);i++)
                        buf[i]=buf[i+1];
                    now--;
#ifdef CHINESE_CHARACTER
                    if (DEFINE(currentuser, DEF_CHCHAR)) {
                        chk = 0;
                        for(i=0;i<now;i++) {
                            if(chk) chk=0;
                            else if(buf[i]<0) chk=1;
                        }
                        if(chk) {
                            for(i=now-1;i<strlen(buf);i++)
                                buf[i]=buf[i+1];
                            now--;
                        }
                    }
#endif
                }
                break;
            case KEY_DEL:
                if(now<strlen(buf)) {
#ifdef CHINESE_CHARACTER
                    if (DEFINE(currentuser, DEF_CHCHAR)) {
                        chk = 0;
                        for(i=0;i<now+1;i++) {
                            if(chk) chk=0;
                            else if(buf[i]<0) chk=1;
                        }
                        if(chk)
                            for(i=now;i<strlen(buf);i++)
                                buf[i]=buf[i+1];
                    }
#endif
                    for(i=now;i<strlen(buf);i++)
                        buf[i]=buf[i+1];
                }
                break;
            case KEY_LEFT:
                if(now>0) {
                    now--;
#ifdef CHINESE_CHARACTER
                    if (DEFINE(currentuser, DEF_CHCHAR)) {
                        chk = 0;
                        for(i=0;i<now;i++) {
                            if(chk) chk=0;
                            else if(buf[i]<0) chk=1;
                        }
                        if(chk) now--;
                    }
#endif
                }
                break;
            case KEY_RIGHT:
                if(now<strlen(buf)) {
                    now++;
#ifdef CHINESE_CHARACTER
                    if (DEFINE(currentuser, DEF_CHCHAR)) {
                        chk = 0;
                        for(i=0;i<now;i++) {
                            if(chk) chk=0;
                            else if(buf[i]<0) chk=1;
                        }
                        if(chk) now++;
                    }
#endif
                }
                break;
            case KEY_HOME:
            case Ctrl('A'):
                now--;
                while(now>=0&&buf[now]!='\n'&&buf[now]!='\r') now--;
                now++;
                break;
            case KEY_END:
            case Ctrl('E'):
                while(now<strlen(buf)&&buf[now]!='\n'&&buf[now]!='\r') now++;
                break;
            case KEY_PGUP:
                now=0;
                break;
            case KEY_PGDN:
                now = strlen(buf);
                break;
            case Ctrl('Y'):
                i0 = strlen(buf);
                i=now-1;
                while(i>=0&&buf[i]!='\n'&&buf[i]!='\r') i--;
                i++;
                if(!buf[i]) break;
                j=now;
                while(j<i0-1&&buf[j]!='\n'&&buf[j]!='\r') j++;
                if(j>=i0-1) j=i0-1;
                j=j-i+1;
                if(j<0) j=0;
                for(k=0;k<i0-i-j+1;k++)
                    buf[i+k]=buf[i+j+k];

                y = starty; x = startx;
                chk = 0;
                if(y==cursory&&x<=cursorx)
                    now=0;
                for(i=0; i<strlen(buf); i++) {
                    if(chk) chk=0;
                    else if(buf[i]<0) chk=1;
                    if(chk&&x>=maxcol) x++;
                    if(buf[i]!=13&&buf[i]!=10) {
                        if(x>maxcol) {
                            x = col;
                            y++;
                        }
                        x++;
                    }
                    else {
                        x = col;
                        y++;
                    }
#ifdef CHINESE_CHARACTER
                    if (!DEFINE(currentuser, DEF_CHCHAR)||!chk)
#endif
                    if(y==cursory&&x<=cursorx)
                        now=i+1;
                }

                if(now>strlen(buf)) now=strlen(buf);
                break;
            default:
                if(isprint2(ch)&&strlen(buf)<len-1) {
                    for(i=strlen(buf)+1;i>now;i--)
                        buf[i]=buf[i-1];
                    buf[now++]=ch;
                    y = starty; x = startx;
                    chk = 0;
                    for(i=0; i<strlen(buf); i++) {
                        if(chk) chk=0;
                        else if(buf[i]<0) chk=1;
                        if(chk&&x>=maxcol) x++;
                        if(buf[i]!=13&&buf[i]!=10) {
                            if(x>maxcol) {
                                x = col;
                                y++;
                            }
                            x++;
                        }
                        else {
                            x = col;
                            y++;
                        }
                    }
                    if(y-starty+1>maxline) {
                        for(i=now-1;i<strlen(buf);i++)
                            buf[i]=buf[i+1];
                        now--;
                    }
                }
                break;
        }
    }

    return y-starty+1;
}

int lock_scr()
{                               /* Leeward 98.02.22 */
    char passbuf[STRLEN];

    if (!strcmp(currentuser->userid, "guest"))
        return 1;

    modify_user_mode(LOCKSCREEN);
    clear();
    /*
     * lock_monitor(); 
     */
    while (1) {
        move(19, 32);
        clrtobot();
        prints("[1m[32mBBS " NAME_BBS_CHINESE "站[m");
        move(21, 0);
        clrtobot();
        getdata(21, 0, "屏幕现在已经锁定，要解除锁定，请输入密码：", passbuf, 39, NOECHO, NULL, true);
        move(22, 32);
        if (!checkpasswd2(passbuf, currentuser)) {
            prints("[1m[31m密码输入错误...[m\n");
            pressanykey();
        } else {
            prints("[1m[31m屏幕现在已经解除锁定[m\n");
            /*
             * pressanykey(); 
             */
            break;
        }
    }
    return 0;
}

void printdash(char *mesg)
{
    char buf[80], *ptr;
    int len;

    memset(buf, '=', 79);
    buf[79] = '\0';
    if (mesg != NULL) {
        len = strlen(mesg);
        if (len > 76)
            len = 76;
        ptr = &buf[40 - len / 2];
        ptr[-1] = ' ';
        ptr[len] = ' ';
        strncpy(ptr, mesg, len);
    }
    prints("%s\n", buf);
}

void bell()
{
    /*
     * change by KCN 1999.09.08    fprintf(stderr,"%c",Ctrl('G')) ; 
     */
    char sound;

    sound = Ctrl('G');
    output(&sound, 1);

}

int pressreturn()
{
    extern int showansi;
    char buf[3];

    showansi = 1;
    move(t_lines - 1, 0);
    prints("\x1b[m");
    clrtoeol();
    getdata(t_lines - 1, 0, "                              \x1b[33m请按 ◆\x1b[36mEnter\x1b[33m◆ 继续\x1b[m", buf, 2, NOECHO, NULL, true);
    move(t_lines - 1, 0);
    clrtoeol();
    return 0;
}

int askyn(str, defa)
char str[STRLEN];
int defa;
{
    int x, y;
    char realstr[STRLEN * 2];
    char ans[6];

    sprintf(realstr, "%s (Y/N)? [%c]: ", str, (defa) ? 'Y' : 'N');
    getyx(&x, &y);
    getdata(x, y, realstr, ans, 3, DOECHO, NULL, true);
    if (ans[0] == 'Y' || ans[0] == 'y')
        return 1;
    else if (ans[0] == 'N' || ans[0] == 'n')
        return 0;
    return defa;
}

int pressanykey()
{
    extern int showansi;

    showansi = 1;
    move(t_lines - 1, 0);
    prints("\x1b[m");
    clrtoeol();
    prints("                                \x1b[5;1;33m按任何键继续 ..\x1b[m");
    igetkey();
    move(t_lines - 1, 0);
    clrtoeol();
    return 0;
}
