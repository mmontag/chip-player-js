<?php

# This is Bisqwit's generic docmaker.php, activated from depfun.mak.
# The same program is used in many different projects to create
# the README.html file from progdesc.php.
#
# docmaker.php version 1.2.0

# Copyright (C) 2000,2004 Bisqwit (http://iki.fi/bisqwit/)

# Syntax:

# argv[1]: Archive name
# argv[2]: Source file (default: progdesc.php)

# Requires:
#   /usr/local/bin/htmlrecode
#   /WWW/document.php (document formatting module)
#
# From the source file, requires the following:
#   $title
#   $progname

@$archivename = $argv[1];
@$docmodulefn = $argv[2];
$docformatfn = '/WWW/document.php';

if(!$docmodulefn) $docmodulefn = 'progdesc.php';

foreach(array($docmodulefn, $docformatfn) as $fn)
  if(!file_exists($fn))
  {
    print "$fn not found, not making document.\n";
    exit(1);
  }

function shellfix($s){return "'".str_replace("'", "'\''", $s)."'";}

$content_array = file($docmodulefn);
$content = implode('', $content_array);
$fw = fopen('docmaker-temp.php', 'w');
fwrite($fw, preg_replace('/include \'.*;/U', '', $content));
fclose($fw);
include 'docmaker-temp.php';
unlink('docmaker-temp.php');

if(!isset($outset)) $outset='';
if($outset) ob_start();

?>
<html><head><meta http-equiv="Content-type" content="text/html; charset=iso-8859-1">
 <title><?=htmlspecialchars($title)?></title>
 <style type="text/css"><!--
TABLE.toc {border:0px}
A:link,A:visited{text-decoration:none;color:#2A3B83}
A:hover{text-decoration:underline;color:#002040}
A:active{text-decoration:underline;color:#004060;background:#CCD8FF}
TD.toc   {font-size:80%; font-family:Tahoma; text-align:left}
H1       {font-size:250%; font-weight:bold} .level1 {text-align:center}
H2       {font-size:200%; font-weight:bold} .level2 {margin-left:1%}
H3       {font-size:160%; font-weight:bold} .level3 {margin-left:2%}
H4       {font-size:145%; font-weight:bold} .level4 {margin-left:3%}
H5       {font-size:130%; font-weight:bold} .level5 {margin-left:4%}
H6       {font-size:110%; font-weight:bold} .level5 {margin-left:5%}
BODY{background:white;color:black}
CODE{font-family:lucida console,courier new,courier;color:#105000}
PRE.smallerpre{font-family:lucida console,courier new,courier;font-size:80%;color:#500010;margin-left:30px}
SMALL    {font-size:70%}
.nonmail { visibility:hidden;position:absolute; top:0px;left:0px }
.ismail  { font-weight:normal }
--></style></head>
 <body>
  <h1><?=htmlspecialchars($title)?></h1>
  <h2 class=level2> 0. Contents </h2>
  
  This is the documentation of <?=htmlspecialchars($archivename)?>.
<?

$url = 'http://iki.fi/bisqwit/source/'.rawurlencode($progname).'.html';
$k = '
   The official home page of '.htmlspecialchars($progname).'
   is at <a href="'.htmlspecialchars($url).'">'.htmlspecialchars($url).'</a>.<br>
   Check there for new versions.
';
if(isset($git))
{
  $k .=
    '<p>'.
    'Additionally, the most recent source code (bleeding edge) for '.htmlspecialchars($progname).
    ' can also be downloaded by cloning the Git repository'.
    ' by:<ul style="margin-left:3em;margin-top:0px">'.
    '<li><code> git clone <a href="'.htmlspecialchars($git).'">'.htmlspecialchars($git).'</a></code></li>'.
    '<li><code> git checkout origin/release -b release</code></li>'.
    '<li><code> git checkout origin/master  -b master</code></li>'.
    '</ul></p>';
}
$text['download:99999. Downloading'] = $k;

include $docformatfn;

$st1 = stat($docmodulefn);
$st2 = stat('docmaker.php');
?>
 <p align=right><small>Generated from
       <tt><?=$docmodulefn?></tt> (last updated: <?=date('r', $st1[9])?>)<br>
  with <tt>docmaker.php</tt> (last updated: <?=date('r', $st2[9])?>)<br>
  at <?=date('r')?></small>
 </p>
</body>
</html>
<?

if($outset)
{
  $s = ob_get_contents();
  ob_end_clean();
  if(file_exists('/usr/local/bin/htmlrecode'))
  {
    /* Try to ensure browser interpretes japanese characters correctly */
    passthru('echo '.shellfix($s).
             '|/usr/local/bin/htmlrecode -Iiso-8859-1 -O'.$outset.' 2>/dev/null');
  }
  else
    print $s;
}
