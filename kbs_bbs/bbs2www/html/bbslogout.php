<?php
	/**
	 * This file handles user logout.
	 * $Id$
	 */
  require("funcs.php");
  if ($loginok!=1) {
?>
<html>
<meta http-equiv="Content-Type" content="text/html; charset=gb2312">
<link rel="stylesheet" type="text/css" href="/bbs.css">
没有登陆<br><br>
<a href="/index.html">快速返回</a>
</html>
<?php
 } else {
	bbs_wwwlogoff();
    setcookie("UTMPKEY","",time()-3600,"/");
    setcookie("UTMPNUM","",time()-3600,"/");
    setcookie("UTMPUSERID","",time()-3600,"/");
    setcookie("LOGINTIME","",time()-3600,"/");
    setcookie("WWWPARAMS","",time()-3600,"/");

	header("Location: /index.html");
   }
?>
