<?php
        
    require("funcs.php");
/*
        $hostname=bbs_sysconf_str("MYSQLHOST");
        $username=bbs_sysconf_str("MYSQLUSER");
        $password=bbs_sysconf_str("MYSQLPASSWORD");
        $dbname=bbs_sysconf_str("MYSQLDATABASE");
*/
$hostname="localhost";
$username="root";
$password="hebiao";
$dbname="SMSmsg";

    if($loginok !=1)
    	html_nologin(); 
    else
	{
		html_init("gb2312");

		if( $_GET["start"] ){
			$startnum = $_GET["start"];
		}else if( $_POST["start"] ){
			$startnum = $_POST["start"];
		}else
			$startnum = 0;
		settype($startnum,"integer");

		if( $_GET["count"] ){
			$count = $_GET["count"];
		}else if( $_POST["count"] ){
			$count = $_POST["count"];
		}else
			$count = 10;
		settype($count,"integer");

		$db = mysql_connect($hostname, $username, $password) or die(mysql_error());
		mysql_select_db($dbname, $db) or die(mysql_error());

		if( $_GET["action"] ){
			$action = $_GET["action"];
		}

		if(isset($action) && $action == "del"){
			if( $_GET["id"] ){
				$id = $_GET["id"];
			}
			else{
				html_error_quit("参数错误1");
			}
			settype($id, "integer");
			if($id < 0)
				html_error_quit("参数错误2");

			$sqlstr = "DELETE FROM smsmsg WHERE userid=\"".$currentuser["userid"]."\" AND id=".$id;
			$result = mysql_query($sqlstr) or die(mysql_error());

		}

		$sqlstr = "SELECT * FROM smsmsg WHERE userid=\"".$currentuser["userid"]."\" ORDER BY readed, timestamp LIMIT ".$startnum.",".$count;

		$result = mysql_query($sqlstr) or die(mysql_error());

		if( ! $result ){
			html_error_quit("没有数据");
		}else{
?>
<body>
<center><p><?php echo BBS_FULL_NAME; ?> -- 短信管理 [用户: <?php echo $currentuser["userid"]; ?>]</p>
<a href="javascript:location=location">刷新</a>
</center>
<hr class=default>
<table border="1" width="613" align="center" cellpadding="0" cellspacing="0">
<tr><td>序号</td><td>对象</td><td>类别</td><td>时间</td><td>内容</td><td></td></tr>
<?php
			$i=0;
			while($row = mysql_fetch_row($result)){
?>
<tr>
<td><?php echo $startnum+$i+1;?></td>
<td><?php echo $row[2];?></td>
<td><?php if( $row[4]==1 ) echo "发"; else echo "收";?></td>
<td><?php echo $row[3];?></td>
<td><pre><?php echo $row[5];?></pre></td>
<td><a href="/bbsrsmsmsg.php?start=<?php if($i==0 && $startnum > 0) echo ($startnum-1); else echo $startnum;?>&count=<?php echo $count;?>&action=del&id=<?php echo $row[0];?>">删除</a></td>
</tr>
<?php
				$i++;
			}
			mysql_free_result($result);
		}
?>
</table>
<center>
<hr class=default>
<a href="javascript:location=location">刷新</a>
<?php
		if( $startnum > 0 ){
?>
<a href="/bbsrsmsmsg.php?start=<?php if($startnum - $count > 0) echo ($startnum-$count); else echo "0";?>&count=<?php echo $count;?>">上一页</a>
<?php	}else{
?>
上一页
<?php
		}

		if( $i >= $count ){
?>
<a href="/bbsrsmsmsg.php?start=<?php echo ($startnum+$count-1);?>&count=<?php echo $count;?>">下一页</a>
<?php	}else{
?>
下一页
<?php
		}
?>
</center>
</body>
<?php
	}
?>
