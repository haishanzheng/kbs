/*
    Pirate Bulletin Board System
    Copyright (C) 1990, Edward Luke, lush@Athena.EE.MsState.EDU
    Eagles Bulletin Board System
    Copyright (C) 1992, Raymond Rocker, rocker@rock.b11.ingr.com
                        Guy Vega, gtvega@seabass.st.usm.edu
                        Dominic Tynes, dbtynes@seabass.st.usm.edu

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "bbs.h"
#include "vote.h"

extern cmpbnames();
extern int page,range;
char *vote_type[] = { "是非", "单选", "复选", "数字", "问答"};
struct votebal currvote;
struct votelimit currlimit;/*Haohmaru.99.11.17.根据板主设的限制条件判断是否让该使用者投票*/
char controlfile[STRLEN],limitfile[STRLEN];
unsigned int result[33];
int vnum;
int voted_flag;
static FILE *sug;

int
cmpvuid(userid, uv)
char    *userid;
struct ballot *uv;
{
    return !strcmp(userid, uv->uid);
}

int
setvoteflag(char* bname,int flag)
{
    int pos;
    struct boardheader fh;

	pos = getboardnum(bname,&fh);
	if (pos) {
	    if(flag==0)
	        fh.flag = fh.flag&~VOTE_FLAG;
	    else
	        fh.flag = fh.flag|VOTE_FLAG;
		set_board(pos,&fh);
	}
}

void
b_report( str )
char *str;
{
    char        buf[ STRLEN ];

    sprintf( buf, "%s %s", currboard, str );
    report( buf );
}

void
makevdir( bname )
char    *bname;
{
    struct stat st;
    char        buf[ STRLEN ];

    sprintf( buf, "vote/%s", bname );
    if( stat( buf, &st ) != 0 )
        mkdir( buf, 0755 );
}

void
setvfile( buf, bname, filename )
char    *buf, *bname, *filename;
{
    sprintf( buf, "vote/%s/%s", bname, filename );
}

void
setcontrolfile()
{
    setvfile(controlfile,currboard,"control");
}

int
b_notes_edit()
{
    char        buf[ STRLEN ];
    char ans[4];
    int         aborted;

    if( !chk_currBM(currBM,currentuser))
    {
        return 0 ;
    }
    clear();
    makevdir( currboard );
    setvfile( buf, currboard, "notes" );
    getdata(1,0,"(E)编辑 (D)删除 本讨论区的备忘录? [E]: ",ans,2,DOECHO,NULL,YEA);
    if (ans[0] == 'D' || ans[0] == 'd')
    {
        move(2,0);
        if(askyn("真的要删除本讨论区的备忘录",0))
        {
            move(3,0);
            prints("备忘录已经删除...\n");
            pressanykey();
            unlink(buf);
            aborted=1;
        }else
            aborted=-1;
    }else
        aborted = vedit( buf, NA );
    if( aborted ==-1) {
        pressreturn();
    } else
    {
        setvfile( buf, currboard, "noterec" );
        unlink(buf);
    }

    return FULLUPDATE;
}

int
b_jury_edit()   /* stephen 2001.11.1: 编辑版面仲裁名单 */
{
    char        buf[ STRLEN ];
    char ans[4];
    int         aborted;

    if(!(HAS_PERM(currentuser,PERM_JURY) && HAS_PERM(currentuser,PERM_BOARDS) || HAS_PERM(currentuser,PERM_SYSOP)))
    {
        return 0 ;
    }
    clear();
    makevdir(currboard);
    setvfile( buf, currboard, "jury" );
    getdata(1,0,"(E)编辑 (D)删除 本讨论区仲裁委员名单? (C)取消 [C]: ",ans,2,DOECHO,NULL,YEA);
    if (ans[0] == 'D' || ans[0] == 'd')
    {
        move(2,0);
        if(askyn("真的要删除本讨论区仲裁委员名单",0))
        {
            move(3,0);
            prints("仲裁委员名单已经删除...\n");
            pressanykey();
            unlink(buf);
            aborted=111;
        }else
            aborted=-1;
    }
    else if (ans[0] =='E' || ans[0]=='e')
        aborted = vedit( buf, NA );
    else 
    {    
	prints("取消");
	aborted=-1;
    }
    if( aborted ==-1) {
        pressreturn();
    } else
    {
	char secu[STRLEN];
	if (aborted==111)
	{  sprintf(secu, "删除 %s 版的仲裁委员名单",currboard);
	   securityreport(secu, NULL);
	   postfile(buf, "JuryMail", secu, 2);
	}
	else 
	{  sprintf(secu, "修改 %s 版的仲裁委员名单",currboard);
	/*securityreport(secu, NULL);*/
	   postfile(buf, "syssecurity", secu, 2);
	   postfile(buf, "JuryMail", secu, 2);
	}
        setvfile( buf, currboard, "juryrec" );
        unlink(buf);
    }

    return FULLUPDATE;
}

int
b_suckinfile( fp, fname )
FILE *fp;
char *fname;
{
    char inbuf[256];
    FILE *sfp;
    if ( ( sfp = fopen( fname, "r" ) ) == NULL )
        return -1;
    while ( fgets( inbuf, sizeof( inbuf ), sfp ) != NULL )
        fputs( inbuf, fp );
    fclose( sfp );
    return 0;
}

/*Add by SmallPig*/
int
catnotepad( fp, fname )
FILE *fp;
char *fname;
{
    char inbuf[256];
    FILE *sfp;
    int count;

    count=0;
    if ( ( sfp = fopen( fname, "r" ) ) == NULL )
    {
        fprintf(fp,"[31m[41m⊙┴———————————————————————————————————┴⊙[m\n\n");
        return -1;
    }
    while ( fgets( inbuf, sizeof( inbuf ), sfp ) != NULL )
    {
        if(count!=0)
            fputs( inbuf, fp );
        else
            count++;
    }
    fclose( sfp );
    return 0;
}

int
vote_close()
{
    time_t closetime;

    closetime=currvote.opendate+currvote.maxdays*86400;
    if(closetime<=time(NULL))
    {
        mk_result(vnum);
    }
}

int
b_close(fh)
struct boardheader *fh;
{
    int end;

    strcpy(currboard,fh->filename);
    setcontrolfile();
    end=get_num_records(controlfile,sizeof(currvote));
    for(vnum=end;vnum>=1;vnum--)
    {
        get_record(controlfile,&currvote,sizeof(currvote),vnum);
        vote_close();
    }
    return;
}

int
b_closepolls()
{
    struct stat st;
    struct boardheader fh;
    FILE        *cfp;
    char        buf[80];
    time_t      now;
    int         i,end;

    now = time( NULL );
    strcpy( buf, "vote/lastpolling" );
    if( stat( buf, &st ) != -1 && st.st_mtime > now - 3600 ) {
        return 0;
    }
    move( t_lines-1, 0 );
    prints( "对不起，系统关闭投票中，请稍候..." );
    refresh();
    if( (cfp = fopen( buf, "w" )) == NULL )
    {
        report("lastpoll write error");
        return 0;
    }
    fprintf( cfp, "%s", ctime( &now ) );
    strcpy(buf,currboard);
    fclose( cfp );

    apply_boards(b_close);
    strcpy(currboard,buf);
    return 0;
}


int
count_result(struct ballot *ptr,char* arg)
{
    int     i;

    if (ptr == NULL)
    {
        if(sug!=NULL) {
            fclose(sug);
            sug = NULL;
        }
        return 0;
    }
    if(ptr->msg[0][0]!='\0')
    {
        if(currvote.type==VOTE_ASKING)
        {
            fprintf(sug,"%s 的作答如下：\n",ptr->uid);
        }
        else
            fprintf(sug,"%s 的建议如下：\n",ptr->uid);
        for(i=0;i<3;i++)
            fprintf(sug,"%s\n",ptr->msg[i]);
    }
    result[32]++;
    if(currvote.type==VOTE_ASKING)
    {
        return 0;
    }
    if(currvote.type!=VOTE_VALUE)
    {
        for (i = 0; i < 32; i++)
        {
            if ((ptr->voted >> i) & 1)
                (result[i])++;
        }

    }else
    {
        result[31]+=ptr->voted;
        result[(ptr->voted*10)/(currvote.maxtkt+1)]++;
    }
    return 0;
}

get_result_title()
{
    char buf[STRLEN];

    if(currlimit.numlogins || currlimit.numposts || currlimit.stay || currlimit.
            day){
        fprintf( sug, "⊙ 此次投票的允许资格为:\n");
        fprintf( sug, "1. 上站次数需大于 %d 次 .\n",currlimit.numlogins);
        fprintf( sug, "2. 文章数目需大于 %d 篇.\n",currlimit.numposts);
        fprintf( sug, "3. 上站总时数需大于 %d 小时.\n",currlimit.stay);
        fprintf( sug, "4. 上站物理时间需大于 %d 天.\n",currlimit.day);
    }
    fprintf( sug, "⊙ 投票开启於：%.24s  类别：%s\n", ctime( &currvote.opendate )
             ,vote_type[currvote.type-1]);
    fprintf( sug, "⊙ 主题：%s\n",currvote.title);
    if(currvote.type==VOTE_VALUE)
        fprintf( sug, "⊙ 此次投票的值不可超过：%d\n\n" ,currvote.maxtkt);
    fprintf( sug, "⊙ 票选题目描述：\n\n" );
    sprintf( buf, "vote/%s/desc.%d",currboard,currvote.opendate );
    b_suckinfile( sug, buf );
}

int
mk_result(num)
int num;
{
    char fname[STRLEN],nname[STRLEN];
    char sugname[STRLEN];
    char title[STRLEN];
    int i;
    unsigned int total=0;

    setcontrolfile();
    sprintf(fname,"vote/%s/flag.%d",currboard,currvote.opendate);
    count_result(NULL,0);
    sprintf(sugname,"vote/%s/tmp.%d",currboard,getpid());
    if((sug=fopen(sugname,"w"))==NULL)
    {
        report("open vote tmp file error");
        prints("Error: 结束投票错误...\n");
        pressanykey();
    }
    (void)memset(result, 0, sizeof(result));
    if(apply_record(fname,count_result,sizeof(struct ballot),0)==-1)
    {
        report("Vote apply flag error");
    }
    fprintf( sug, "[44m[36m——————————————┤使用者%s├——————————————[m\n\n\n",(currvote.type!=VOTE_ASKING)?"建议或意见":"此次的作答");
    fclose(sug);
    sug = NULL;
    sprintf(nname,"vote/%s/results",currboard);
    if((sug=fopen(nname,"w"))==NULL)
    {
        report("open vote newresult file error");
        prints("Error: 结束投票错误...\n");
    }

    /*    fprintf( sug, "** 投票开启於：[1m%.24s[m  类别：[1m%s[m\n", ctime( &currvote.opendate )
                ,vote_type[currvote.type-1]);
        fprintf( sug, "** 主题：[1m%s[m\n",currvote.title);
        if(currvote.type==VOTE_VALUE)
            fprintf( sug, "** 此次投票的值不可超过：[1m%d[m\n\n" ,currvote.maxtkt);
        fprintf( sug, "** 票选题目描述：\n\n" );
        sprintf( buf, "vote/%s/desc.%d",currboard,currvote.opendate );
        b_suckinfile( sug, buf );*/
    get_result_title();

    fprintf( sug, "** 投票结果:\n\n" );
    if(currvote.type==VOTE_VALUE)
    {
        total=result[32];
        for(i=0;i<10;i++)
        {
            fprintf(sug, "[1m  %4d[m 到 [1m%4d[m 之间有 [1m%4d[m 票  约占 [1m%d%%[m\n",
                    (i*currvote.maxtkt)/10+((i==0)?0:1),((i+1)*currvote.maxtkt)/10,result[i]
                    ,(result[i]*100)/((total<=0)?1:total));
        }
        fprintf(sug, "此次投票结果平均值是: [1m%d[m\n",result[31]/((total<=0)?1:total));
    }else if(currvote.type==VOTE_ASKING)
    {
        total=result[32];
    }else
    {
        for(i=0;i<currvote.totalitems;i++)
        {
            total += result[i];
        }
        for(i=0;i<currvote.totalitems;i++)
        {
            /*            fprintf(sug, "(%c) %-40s  %4d 票  约占 [1m%d%%[m\n",'A'+i,
                     currvote.items[i], result[i] , (result[i]*100)/((total<=0)?1:total)); 
            */
            fprintf(sug, "(%c) %-40s  %4d 票  约占 [1m%d%%[m\n",'A'+i,
                    currvote.items[i], result[i] , (result[i]*100)/((result[32]<=0)?1:result[32]));

        }
    }
    fprintf( sug, "\n投票总人数 = [1m%d[m 人\n", result[32] );
    fprintf( sug, "投票总票数 =[1m %d[m 票\n\n", total );
    fprintf( sug, "[44m[36m——————————————┤使用者%s├——————————————[m\n\n\n",(currvote.type!=VOTE_ASKING)?"建议或意见":"此次的作答");
    b_suckinfile( sug, sugname);
    unlink(sugname);
    fclose(sug);
    sug = NULL;

    sprintf(title,"[公告] %s 板的投票结果",currboard);
    mail_file(nname,currvote.userid,title);
    if (strcmp(currboard,"sys_discuss") &&  strcmp(currboard, "Jury"))/*如果是sys_discuss版,投票结果不进vote版,Haohmaru,98.9.4 add Jury by stephen 2001.10.31*/
    {
        postfile(nname,"vote",title,1);
    }
    postfile(nname,currboard,title,1);
    dele_vote(num);
    return ;
}
int
check_result(num)
int num;
{
    char fname[STRLEN],nname[STRLEN];
    char sugname[STRLEN];
    char title[STRLEN];
    int i;
    unsigned int total=0;

    setcontrolfile();
    sprintf(fname,"vote/%s/flag.%d",currboard,currvote.opendate);
    count_result(NULL,0);
    sprintf(sugname,"vote/%s/tmp.%d",currboard,getpid());
    if((sug=fopen(sugname,"w"))==NULL)
    {
        report("open vote tmp file error");
        prints("Error: 检查投票错误...\n");
        pressanykey();
    }
    (void)memset(result, 0, sizeof(result));
    if(apply_record(fname,count_result,sizeof(struct ballot),0)==-1)
    {
        report("Vote apply flag error");
    }
    fprintf( sug, "[44m[36m——————————————┤使用者%s├——————————————[m\n\n\n",(currvote.type!=VOTE_ASKING)?"建议或意见":"此次的作答");
    fclose(sug);
    sug = NULL;
    sprintf(nname,"vote/%s/results",currboard);
    if((sug=fopen(nname,"w"))==NULL)
    {
        report("open vote newresult file error");
        prints("Error: 结束投票错误...\n");
    }

    get_result_title();

    fprintf( sug, "** 投票结果:\n\n" );
    if(currvote.type==VOTE_VALUE)
    {
        total=result[32];
        for(i=0;i<10;i++)
        {
            fprintf(sug, "[1m  %4d[m 到 [1m%4d[m 之间有 [1m%4d[m 票  约占 [1m%d%%[m\n",
                    (i*currvote.maxtkt)/10+((i==0)?0:1),((i+1)*currvote.maxtkt)/10,result[i]
                    ,(result[i]*100)/((total<=0)?1:total));
        }
        fprintf(sug, "此次投票结果平均值是: [1m%d[m\n",result[31]/((total<=0)?1:total));
    }else if(currvote.type==VOTE_ASKING)
    {
        total=result[32];
    }else
    {
        for(i=0;i<currvote.totalitems;i++)
        {
            total += result[i];
        }
        for(i=0;i<currvote.totalitems;i++)
        {
            fprintf(sug, "(%c) %-40s  %4d 票  约占 [1m%d%%[m\n",'A'+i,
                    currvote.items[i], result[i] , (result[i]*100)/((result[32]<=0)?1:result[32]));

        }
    }
    fprintf( sug, "\n投票总人数 = [1m%d[m 人\n", result[32] );
    fprintf( sug, "投票总票数 =[1m %d[m 票\n\n", total );
    fprintf( sug, "[44m[36m——————————————┤使用者%s├——————————————[m\n\n\n",(currvote.type!=VOTE_ASKING)?"建议或意见":"此次的作答");
    b_suckinfile( sug, sugname);
    unlink(sugname);
    fclose(sug);
    sug = NULL;

    sprintf(title,"[检查] %s 板的投票结果",currboard);
    mail_file(nname,currentuser->userid,title);

    unlink(nname);

    return ;
}

int
get_vitems(bal)
struct votebal *bal;
{
    int num;
    char buf[STRLEN];

    move(3,0);
    prints( "请依序输入可选择项, 按 ENTER 完成设定.\n" );
    num = 0;
    for (num = 0; num < 32; num++)
    {
        sprintf( buf, "%c) ", num + 'A' );
        getdata( (num%16) + 4 , (num / 16) * 40 , buf, bal->items[num], 36, DOECHO, NULL,YEA);
        if (strlen(bal->items[num]) == 0)
        {
            if (num != 0)
                break;
            num = -1;
        }

    }
    bal->totalitems=num;
    return num;
}

int
vote_maintain( bname )
char    *bname;
{
    char buf[STRLEN*2];
    struct votebal *ball=&currvote;
    struct votelimit *v_limit=&currlimit;
    int aborted;

    setcontrolfile();
    if( !HAS_PERM(currentuser, PERM_SYSOP ))
        if (!HAS_PERM(currentuser,PERM_OVOTE))
            if( !chk_currBM(currBM,currentuser))
            {
                return 0 ;
            }
    stand_title( "开启投票箱" );
    makevdir( bname );
    for(;;)
    {
        getdata(2,0,"(1)是非, (2)单选, (3)复选, (4)数值 (5)问答 (6)取消 ? : "
                ,genbuf,2,DOECHO,NULL,YEA);
        genbuf[0] -= '0' ;
        if(genbuf[0] == 6)
        {
            prints( "取消此次投票\n" );
            sleep(1);
            return FULLUPDATE;
        }
        if(genbuf[0] < 1 || genbuf[0] > 5)
            continue;
        ball->type = (int)genbuf[0] ;
        break;
    }
    ball->opendate=time(NULL);
    prints("按任何开始编辑此次 [投票的描述]: \n");
    igetch();
    setvfile( genbuf, bname, "desc" );
    sprintf( buf ,"%s.%d",genbuf,ball->opendate);
    aborted = vedit( buf, NA );
    if ( aborted ) {
        clear();
        prints( "取消此次投票\n" );
        pressreturn();
        return FULLUPDATE;
    }

    clear();
    getdata( 0,0,"此次投票所须天数 (不可０天): ",buf, 4, DOECHO, NULL ,YEA);

    if ( *buf == '\n' || atoi(buf) == 0 || *buf == '\0' )
        strcpy( buf, "1" );

    ball->maxdays = atoi(buf);
    for(;;)
    {
        getdata(1,0,"投票箱的标题: ",ball->title, 61 ,DOECHO, NULL, YEA);
        if(strlen(ball->title)>0)
            break;
        bell();
    }
    switch(ball->type)
    {
    case VOTE_YN :
        ball->maxtkt = 0;
        strcpy(ball->items[0],"赞成  （是的）");
        strcpy(ball->items[1],"不赞成（不是）");
        strcpy(ball->items[2],"没意见（不清楚）");
        ball->maxtkt = 1;
        ball->totalitems=3;
        break;
    case VOTE_SINGLE :
        get_vitems(ball);
        ball->maxtkt = 1;
        break;
    case VOTE_MULTI:
        get_vitems(ball) ;
        for(;;) {
            getdata(21,0,"一个人最多几票? [1]: ",buf,5,DOECHO,NULL,YEA);
            ball->maxtkt = atoi(buf);
            if(ball->maxtkt <= 0) ball->maxtkt = 1;
            if(ball->maxtkt >ball->totalitems)
                continue;
            break;
        }
        break;
    case VOTE_VALUE:
        for(;;) {
            getdata(3,0,"输入数值最大不得超过 [100] : ",buf,7,DOECHO,NULL,YEA);
            ball->maxtkt = atoi(buf);
            if(ball->maxtkt <= 0) ball->maxtkt = 100;
            break;
        }
        break;
    case VOTE_ASKING:
        /*                    getdata(3,0,"此问答题作答行数之限制 :",buf,3,DOECHO,NULL,YEA) ;
                            ball->maxtkt = atof(buf) ;
                            if(ball->maxtkt <= 0) ball->maxtkt = 10;*/
        ball->maxtkt = 0;
        currvote.totalitems=0;
        break;
    }
    setvoteflag(currboard,1);
    clear();
    /*Haohmaru.99.11.17.根据投票管理员设的限制条件判断是否让该使用者投票*/
    if(HAS_PERM(currentuser,PERM_OVOTE)||HAS_PERM(currentuser,PERM_SYSOP)||HAS_PERM(currentuser,PERM_JURY))
    {
        getdata(1,0,"是否对投票资格进行限制(Y/N) [Y]:",buf,3,DOECHO,NULL,YEA) ;
        if ( buf[0] != 'N' && buf[0] != 'n' )
        {
            getdata(2,0,"请输入对上站次数的限制(0为没有限制),上站次数大于:",buf,5,DOECHO,NULL,YEA) ;
            v_limit->numlogins = atoi(buf);
            getdata(3,0,"请输入对文章数目的限制(0为没有限制),文章数目大于:",buf,5,DOECHO,NULL,YEA) ;
            v_limit->numposts = atoi(buf);
            getdata(4,0,"请输入对上站总时数的限制(0为没有限制),上站总时数大于(小时):",buf,5,DOECHO,NULL,YEA) ;
            v_limit->stay = atoi(buf);
            getdata(5,0,"请输入对上站物理时间的限制(0为没有限制),上站物理时间大于(天):",buf,5,DOECHO,NULL,YEA) ;
            v_limit->day = atoi(buf);
        }
        else
        {
            v_limit->numlogins = 0;
            v_limit->numposts = 0;
            v_limit->stay = 0;
            v_limit->day = 0;
        }
    }
    else
    {
        v_limit->numlogins = 0;
        v_limit->numposts = 0;
        v_limit->stay = 0;
        v_limit->day = 0;
    }
    clear();
    sprintf(limitfile,"vote/%s/limit.%d",currboard,ball->opendate);
    if(append_record(limitfile, v_limit, sizeof(struct votelimit))==-1)
    {
        prints("发生严重的错误，无法写入限制文件，请通告站长");
        b_report( "Append limit file Error!!" );
    }

    /*Haohmaru.99.10.26.add below 8 lines*/
    getdata(1,0,"确定开启投票?[Y] :",buf,3,DOECHO,NULL,YEA) ;
    if ( buf[0] == 'N' || buf[0] == 'n' )
    {
        clear();
        prints( "取消此次投票\n" );
        unlink(limitfile);
        pressreturn();
        return FULLUPDATE;
    }
    strcpy(ball->userid,currentuser->userid);
    if(append_record(controlfile, ball, sizeof(*ball))==-1)
    {
        prints("发生严重的错误，无法开启投票，请通告站长");
        b_report( "Append Control file Error!!" );
    }
    else
    {
        char votename[STRLEN];
        int i;

        b_report( "OPEN VOTE" );
        prints("投票箱已经开启了！\n");
        range++;
        sprintf(votename,"tmp/votetmp.%d",getpid());
        if( (sug=fopen(votename,"w")) !=NULL)
        {
            sprintf(buf,"[通知] %s 举办投票：%s",currboard,ball->title);
            get_result_title();
            if(ball->type!=VOTE_ASKING&&ball->type!=VOTE_VALUE)
            {
                fprintf(sug,"\n【选项如下】\n");
                for(i=0;i<ball->totalitems;i++)
                {
                    fprintf(sug, "([1m%c[m) %-40s\n",'A'+i, ball->items[i]);
                }
            }
            fclose(sug);
            sug = NULL;
            if(!strcmp(currboard,"sys_discuss") || !strcmp(currboard, "Jury"))/*sys_discuss的投票不进vote版,Haohmaru,98.9.4, add Jury by stephen 2001.10.31*/
            {
                postfile(votename,currboard,buf,1);}
            else{
                postfile(votename,"vote",buf,1);
            }
            unlink(votename);
        }
    }
    pressreturn();
    return FULLUPDATE;
}

int
vote_flag( bname, val , mode)
char    *bname, val;
int mode;
{
    char        buf[ STRLEN ], flag;
    int         fd, num, size;

    num = usernum - 1;
    switch(mode)
    {
    case 2:
        sprintf(buf,"Welcome.rec");/*进站的 Welcome 画面*/
        break;
    case 1:
        setvfile( buf, bname, "noterec" );/*讨论区备忘录的旗标*/
        break;
    default:
        return -1;
    }
    if(num>=MAXUSERS)
    {
        report("Vote Flag, Out of User Numbers");
        return -1;
    }
    if( (fd = open( buf, O_RDWR | O_CREAT, 0600 )) == -1 ) {
        char buf[STRLEN];

        sprintf(buf,"%s Flag file open Error.",bname);
        report(buf);
        return -1;
    }
    flock(fd,LOCK_EX);
    size = (int)lseek( fd, 0, SEEK_END );
    memset( buf, 0, sizeof( buf ) );
    while( size <= num ) {
        write( fd, buf, sizeof( buf ) );
        size += sizeof( buf );
    }
    lseek( fd, num, SEEK_SET );
    read( fd, &flag, 1 );
    if( (flag == 0 && val != 0) ) {
        lseek( fd, num, SEEK_SET );
        write( fd, &val, 1 );
    }
    flock(fd,LOCK_UN);
    close( fd );
    return flag;
}


int
vote_check(bits)
int     bits;
{
    int     i,
    count;

    for (i = count = 0; i < 32; i++)
    {
        if ((bits >> i) & 1)
            count++;
    }
    return count;
}

int
showvoteitems( pbits, i, flag)
unsigned int  pbits;
int     i,flag;
{
    char        buf[ STRLEN ];
    int count;

    if(flag==YEA)
    {
        count= vote_check(pbits);
        if(count > currvote.maxtkt)
            return NA;
        move(2,0);
        clrtoeol();
        prints("你已经投了 %d 票",count);
    }
    sprintf( buf, "%c.%2.2s%-36.36s", 'A' + i,
             ((pbits >> i) & 1 ? "◎" : "  "),currvote.items[i]);
    move( i+6-(( i>15)? 16:0) , 0+(( i>15)? 40:0) );
    prints( buf );
    refresh();
    return YEA;
}

void
show_voteing_title()
{
    time_t closedate;
    char buf[STRLEN];

    if(currvote.type!=VOTE_VALUE&&currvote.type!=VOTE_ASKING)
        sprintf(buf,"可投票数: %d 票",currvote.maxtkt);
    else
        buf[0]='\0';
    closedate=currvote.opendate+currvote.maxdays*86400;
    prints( "投票将结束於: %24s  %s  %s\n",
            ctime(&closedate),buf,(voted_flag)?"(修改前次投票)":"");
    prints( "投票主题是: [1m%-50s[m类型: [1m%s[m \n",currvote.title,vote_type[currvote.type-1]);
}

int
getsug(uv)
struct ballot *uv;
{
    int i,line;

    move(0, 0);
    clrtobot();
    if(currvote.type==VOTE_ASKING)
    {
        show_voteing_title();
        line=3;
        prints("请填入您的作答(三行):\n");
    }
    else
    {
        line=1;
        prints("请填入您宝贵的意见(三行):\n");
    }
    move(line , 0);
    for (i = 0; i < 3; i++)
    {
        prints(": %s\n",uv->msg[i]);
    }
    for (i = 0; i < 3; i++)
    {
        getdata(line + i, 0, ": ", uv->msg[i], STRLEN-2, DOECHO, NULL,NA);
        if (uv->msg[i][0] == '\0')
            break;
    }
    return i;
}


int
multivote(uv)
struct ballot *uv;
{
    unsigned int i;

    i=uv->voted;
    move(0,0);
    show_voteing_title();
    uv->voted = setperms(uv->voted,"选票",currvote.totalitems,showvoteitems);
    if(uv->voted==i)
        return -1;
    return 1;
}

int
valuevote(uv)
struct ballot *uv;
{
    unsigned int chs;
    char buf[10];

    chs=uv->voted;
    move(0,0);
    show_voteing_title();
    prints( "此次作答的值不能超过 [1m%d[m",currvote.maxtkt);
    if(uv->voted!=0)
        sprintf(buf,"%d",uv->voted);
    else
        memset(buf,0,sizeof(buf));
    do
    {
        getdata(3,0,"请输入一个值? [0]: ",buf,5,DOECHO,NULL,NA);
        uv->voted=abs(atoi(buf));
    }while((int)uv->voted>currvote.maxtkt && buf[0]!='\n' && buf[0]!='\0');
    if( buf[0]=='\n' || buf[0]=='\0' ||uv->voted==chs)
        return -1;
    return 1;
}

int
user_vote( num )
int num;
{
    char fname[STRLEN],bname[STRLEN];
    char buf[STRLEN];
    struct ballot uservote,tmpbal;
    struct votelimit userlimit;
    int votevalue;
    int aborted=NA,pos;

    move(t_lines-2,0);
    get_record(controlfile,&currvote,sizeof(struct votebal),num);
    sprintf(fname,"vote/%s/flag.%d",currboard,currvote.opendate);
    if ((pos = search_record(fname, &uservote, sizeof(uservote), cmpvuid,
                             currentuser->userid)) <= 0)
    {
        (void)memset(&uservote, 0, sizeof(uservote));
        voted_flag=NA;
    }else
    {
        voted_flag=YEA;
    }
    strcpy(uservote.uid,currentuser->userid);
    sprintf(bname,"desc.%d",currvote.opendate);
    setvfile( buf, currboard, bname );
    ansimore( buf, YEA );
    move( 0, 0 );

    /*Haohmaru.99.11.17.根据板主设的限制条件判断是否让该使用者投票*/
    clear();
    userlimit.numlogins = 0;
    userlimit.numposts = 0;
    userlimit.stay = 0;
    userlimit.day = 0;
    sprintf(limitfile,"vote/%s/limit.%d",currboard,currvote.opendate);
    get_record(limitfile,&userlimit,sizeof(struct votelimit),1);
    if ((currvote.type<=0)||(currvote.type>5)) currvote.type=1;
    if( (currentuser->numposts < userlimit.numposts || currentuser->numlogins < userlimit.numlogins
            || currentuser->stay < userlimit.stay*60*60 || (time(NULL) - currentuser->firstlogin) < userlimit.day*24*60*60) )
    {
        prints("对不起,你不满足板主规定的此次投票所需条件,无法参加投票,谢谢参与,下次再见! :)");
        pressanykey();
        return ;
    }

    clrtobot();
    switch(currvote.type)
    {
case VOTE_SINGLE: case VOTE_MULTI: case VOTE_YN:
        votevalue=multivote(&uservote);
        if(votevalue==-1)
            aborted=YEA;
        break;
    case VOTE_VALUE:
        votevalue=valuevote(&uservote);
        if(votevalue==-1)
            aborted=YEA;
        break;
    case VOTE_ASKING:
        uservote.voted=0;
        aborted=!getsug(&uservote);
        break;
    }
    clear();
    if(aborted==YEA)
    {
        prints("保留 【%s】原来的的投票。\n",currvote.title);
    }else
    {
        if(currvote.type!=VOTE_ASKING)
            getsug(&uservote);
        pos = search_record(fname, &tmpbal, sizeof(tmpbal), cmpvuid,
                            currentuser->userid);
        if (pos)
        {
            substitute_record(fname, &uservote, sizeof(uservote),pos);
        }
        else if (append_record(fname, &uservote, sizeof(uservote)) == -1)
        {
            move(2, 0);
            clrtoeol();
            prints("投票失败! 请通知站长参加那一个选项投票\n");
            pressreturn();
        }
        prints("\n已经帮你(□)投入票箱中...\n");
    }
    pressanykey();
    return ;
}

voteexp()
{
    clrtoeol();
    prints("[44m编号 开启投票箱者 开启日 %-40s类别 天数 人数[m\n","投票主题");
}

int
printvote(struct  votebal *ent,char* arg)
{
    static int i ;
    struct ballot uservote;
    char buf[STRLEN+80],*date;
    char flagname[STRLEN];
    int  num_voted;

    if(ent==NULL)
    {
        move(2,0);
        voteexp();
        i = 0 ;
        return 0;
    }
    i++;
    if(i>page+19||i>range)
        return QUIT;
    else if(i <= page)
        return 0;
    sprintf(buf,"flag.%d",ent->opendate);
    setvfile(flagname,currboard,buf);
    if (search_record(flagname, &uservote, sizeof(uservote), cmpvuid,
                      currentuser->userid)<= 0)
    {
        voted_flag=NA;
    }
    else
        voted_flag=YEA;
    num_voted=get_num_records(flagname,sizeof(struct ballot));
    date=ctime(&ent->opendate)+4;
    if ((ent->type<=0)||(ent->type>5)) ent->type=1;
    sprintf(buf," %s%3d %-12.12s %-6.6s %-40.40s%-4.4s %3d %4d[m\n",(voted_flag==NA)?"[1m":"",i,ent->userid,
            date,ent->title,vote_type[ent->type-1],ent->maxdays,num_voted);
/*
    sprintf(buf," %s%3d %-12.12s %-6.6s %-40.40s%-4.4s %3d  %4d[m\n",(voted_flag==NA)?"[1m":"",i,ent->userid,
            date,ent->title,vote_type[ent->type-1],ent->maxdays,num_voted);
*/
    prints("%s",buf);
    return 0;
}

int
dele_vote(num)
int num;
{
    char    buf[STRLEN];

    sprintf(buf,"vote/%s/flag.%d",currboard,currvote.opendate);
    unlink(buf);
    sprintf(buf,"vote/%s/desc.%d",currboard,currvote.opendate);
    unlink(buf);
    sprintf(buf,"vote/%s/limit.%d",currboard,currvote.opendate);/*Haohmaru.99.11.18*/
    unlink(buf);
    if(delete_record(controlfile,sizeof(currvote),num)==-1)
    {
        prints("发生错误，请通知站长....");
        pressanykey();
    }
    range--;
    if(get_num_records(controlfile,sizeof(currvote))==0)
    {
        setvoteflag(currboard,0);
    }
}

int
vote_results( bname )
char    *bname;
{
    char buf[STRLEN];

    setvfile( buf, bname, "results" );
    if ( ansimore( buf, YEA ) == -1 ) {
        move(3,0);
        prints("目前没有任何投票的结果。\n");
        clrtobot();
        pressreturn();
    } else clear();
    return FULLUPDATE;
}

int
b_vote_maintain()
{
    return vote_maintain( currboard );
}

void
vote_title()
{

    docmdtitle("[投票箱列表]",
               "[←,e] 离开 [h] 求助 [→,r <cr>] 进行投票 [↑,↓] 上,下选择 [1m高亮度[m表示尚未投票");
    update_endline();
}

int
vote_key(ch,allnum,pagenum)
int ch;
int allnum,pagenum;
{
    int deal=0,ans;
    char buf[STRLEN];

    switch(ch)
    {
case 'v': case 'V':
case '\n': case '\r':
case 'r': case KEY_RIGHT:
        user_vote(allnum+1);
        deal=1;
        break;
    case 'R':
        vote_results(currboard);
        deal=1;
        break;
case 'H': case 'h':
        show_help( "help/votehelp" );
        deal=1;
        break;
case 'A': case 'a':
        if(!chk_currBM(currBM,currentuser))
            return YEA;
        vote_maintain(currboard);
        deal=1;
        break;
case 'O': case 'o':
        if(!chk_currBM(currBM,currentuser))
            return YEA;
        if (!strcmp(currboard,"Birthday"))/*Haohmaru.99.3.29.应生日板前板主的要求而设,结束于Dec 28 20:52:29 2000*/
            break;
        clear();
        deal=1;
        get_record(controlfile,&currvote,sizeof(struct votebal),allnum+1);
        prints("[31m警告!![m\n");
        prints("投票箱标题：[1m%s[m\n", currvote.title);
        ans = askyn("你确定要提早结束这个投票吗",0);
        if (ans != 1)
        {
            move(2, 0);
            prints("取消删除行动\n");
            pressreturn();
            clear();
            break;
        }
        mk_result(allnum+1);
        sprintf(buf,"提早结束投票 %s",currvote.title);
        securityreport(buf,NULL);
        report(buf);
        break;
    case '@':
        if(!HAS_PERM(currentuser,PERM_SYSOP))
            return YEA;
        clear();
        deal=1;
        get_record(controlfile,&currvote,sizeof(struct votebal),allnum+1);
        prints("检查投票：^[[1m%s^[[m\n", currvote.title);
        check_result(allnum+1);
        break;

case 'D': case 'd':
        if (!chk_currBM(currBM,currentuser))
        {
            return 1;
        }
        if (!strcmp(currboard,"Birthday"))/*Haohmaru.99.3.29.结束于Dec 28 20:52:29 2000*/
            break;

        deal=1;
        get_record(controlfile,&currvote,sizeof(struct votebal),allnum+1);
        clear();
        prints("[31m警告!![m\n");
        prints("投票箱标题：[1m%s[m\n", currvote.title);
        ans = askyn("你确定要强制关闭这个投票吗",0);

        if (ans != 1)
        {
            move(2, 0);
            prints("取消删除行动\n");
            pressreturn();
            clear();
            break;
        }
        sprintf(buf,"强制关闭投票 %s",currvote.title);
        securityreport(buf,NULL);
        report(buf);
        dele_vote(allnum+1);
        break;
    default:
        return 0;
    }
    if(deal)
    {
        Show_Votes();
        vote_title();
    }
    return 1;
}

Show_Votes()
{

    move(3,0);
    clrtobot();
    printvote(NULL,0) ;
    setcontrolfile();
    if(apply_record(controlfile,printvote,sizeof(struct votebal),0) == -1) {
        prints("错误，没有投票箱开启....") ;
        pressreturn() ;
        return 0;
    }
    clrtobot();
    return 0;
}

int
b_vote()
{
    int num_of_vote;
    int voting;

    if (!HAS_PERM(currentuser,PERM_LOGINOK))
        return 0; /* Leeward 98.05.15 */

    setcontrolfile();
    num_of_vote=get_num_records(controlfile,sizeof(struct votebal));
    if(num_of_vote==0)
    {
        move(3,0);
        clrtobot();
        prints("抱歉, 目前并没有任何投票举行。\n");
        pressreturn();
        setvoteflag(currboard,0);
        return FULLUPDATE;
    }
    setlistrange(num_of_vote);
    clear();
    voting=choose(NA,0,vote_title,vote_key,Show_Votes,user_vote);
    clear();
    return /*user_vote( currboard )*/FULLUPDATE;
}

int
b_results()
{
    return vote_results( currboard );
}

int
m_vote()
{
    char buf[STRLEN];
    strcpy(buf,currboard);
    strcpy(currboard,DEFAULTBOARD);
    modify_user_mode( ADMIN );
    vote_maintain( DEFAULTBOARD );
    strcpy(currboard,buf);
    return ;
}

int
x_vote()
{
    char buf[STRLEN];
    modify_user_mode( XMENU );
    strcpy(buf,currboard);
    strcpy(currboard,DEFAULTBOARD);
    b_vote();
    strcpy(currboard,buf);
    return ;
}

int
x_results()
{
    modify_user_mode( XMENU );
    return vote_results( DEFAULTBOARD );
}

