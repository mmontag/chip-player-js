<?php

# This is Bisqwit's generic makediff.php, activated from depfun.mak.
# The same program is used in many different projects to create
# a diff file version history (patches).
#
# makediff.php version 3.0.6

# Copyright (C) 2000,2002 Bisqwit (http://bisqwit.iki.fi/)

# Syntax:

# argv[1]: Newest archive if any
# argv[2]: Archive directory if any
# argv[3]: Disable /WWW/src/arch linking if set

function ShellFix($s)
{
  return "'".str_replace("'", "'\''", $s)."'";
}

if(isset($REMOTE_ADDR))
{
  header('Content-type: text/plain');
  readfile($PHP_SELF);
  exit;
}

error_reporting(E_ALL & ~E_NOTICE);

ob_implicit_flush(true);

if(strlen($argv[2]))
{
  chdir($argv[2]);
  echo "\tcd ", $argv[2], "\n";
}

function calcversion($versionstring)
{
  $k = '.'.str_replace('.', '..', $versionstring).'.';
  $k = preg_replace('/([^0-9])([0-9][0-9a-z][0-9a-z][0-9a-z][^0-9a-z])/', '\1-\2', $k);
  $k = preg_replace('/([^0-9])([0-9][0-9a-z][0-9a-z][^0-9a-z])/', '\1--\2', $k);
  $k = preg_replace('/([^0-9])([0-9][0-9a-z][^0-9a-z])/', '\1---\2', $k);
  $k = preg_replace('/([^0-9])([0-9][^0-9a-z])/', '\1----\2', $k);
  $k = str_replace('.', '', $k);
  $k = str_pad($k, 6*5, '-');
  # Reverse:
  #$k = strtr($k, '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ',
  #               '9876543210ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba');
  return $k;
}


$openfiles = Array();
$archtmpdir = 'archives.tmp';
$subtmpdir = 'subarch.tmp';
$istmp=0;
function GoTmp()
{
  global $istmp, $archtmpdir;
  if($istmp)return;
  $istmp=1;
  if(@mkdir($archtmpdir, 0700))print "\tmkdir $archtmpdir\n";
  print "\tcd $archtmpdir\n";
  chdir($archtmpdir);
}
function IsInTmp() { global $istmp; return $istmp; }
function UnGoTmp()
{
  global $istmp, $archtmpdir;
  if(!$istmp)return;
  $istmp=0;
  print "\tcd ..\n";
  chdir('..');
}
function Open($files, $keeplist=array())
{
  global $openfiles;
  global $archtmpdir, $subtmpdir;

  /* Open any of the given files */
  foreach($files as $fn)
  {
    // If any of the files is already open, return true.
    $puh = $openfiles[$fn];
    if($puh) return $puh['dir'];
  }

  #echo count($openfiles), " files open now...\n";
  if(count($openfiles) >= 2)
  {
    $oldest=''; $oldesttime=999999999;
    foreach($openfiles as $fn => $puh)
    {
      $keep = 0;
      foreach($keeplist as $keepfn) if($fn == $keepfn) { $keep=1; break; }
      if($keep) continue;
      if($puh['time'] < $oldesttime) { $oldesttime = $puh['time']; $oldest = $fn; }
    }
    if($oldest)
    {
      Close($oldest);
    }
  }
  
  $pick = '';
  foreach($files as $fn)
    if(preg_match('@\.tar\.gz$@', $fn))
    {
      $pick = $fn;
      break;
    }

  if(!$pick)
  {
    reset($files);
    list($dummy, $pick) = each($files);
  }
  
  GoTmp();
  
  @mkdir($subtmpdir, 0700);
  chdir($subtmpdir);
  
  if(preg_match('@\.tar\.gz$@', $pick))
  {
    print "\ttar xfz ../".shellfix($pick)."\n";
    exec('tar xfz ../../'.shellfix($pick));
  }
  else
  {
    print "\tbzip2 -d < ../".shellfix($pick)."| tar xf -\n";
    exec('bzip2 -d < ../../'.shellfix($pick).'| tar xf -');
  }
  $thisdir = exec('echo *');
  exec('mv * ../');
  chdir('..');

  global $timeind;
  $openfiles[$pick] = array('dir' => $thisdir, 'time' => ++$timeind);
  
  return $thisdir;
}
function Close($fn)
{
  global $openfiles;
  global $archtmpdir;
  
  $puh = $openfiles[$fn];
  if(!$puh) return;
  
  $prefix = IsInTmp() ? '' : $archtmpdir.'/';
  
  $cmd = 'rm -rf '.shellfix($prefix.$puh['dir']);
  print "\t$cmd\n";
  exec($cmd);
  unset($openfiles[$fn]);
}
function CloseAll()
{
  global $openfiles;
  global $archtmpdir;
  
  UnGoTmp();
  
  $openfiles = Array();
  $cmd = 'rm -rf '.shellfix($archtmpdir);
  print "\t$cmd\n";
  exec($cmd);
}
function MakeDiff($dir1, $dir2, $patchname)
{
  GoTmp();
  #print "## Building $patchname from $dir1 and $dir2\n";
  #passthru('ls -al');
  
  /*
    Before doing the diff, we should do the following:
      - Remove all symlinks
      - Remove all duplicate hardlinks
  */
  
  /* Gather up inode numbers. */
  
  $data1 = FindInodes($dir1);
  $data2 = FindInodes($dir2);
  
  $era1 = Array();
  $era2 = Array();
  
  foreach($data1['if'] as $ino => $fils)
  {
    if(count($fils) > 1)
    {
      $bestcommoncount = 0;
      $bestcommon = $fn;
      $group2 = array();
      foreach($fils as $fn)
      {
        $ino2 = $data2['fi'][$fn];
        if(!isset($ino2))continue;
        
        $fils2 = $data2['if'][$ino2];
        
        $common = array_intersect($fils, $fils2);
        if(count($common) > $bestcommoncount)
        {
          $bestcommoncount = count($common);
          $bestcommon = $fn;
          $group2 = $fils2;
        }
      }
      $common = array_intersect($fils, $group2);
      
      // Leave one file so that diff works
      reset($common);
      list($dummy, $fn) = each($common);
      unset($common[$dummy]);
      
      foreach($common as $fn)
      {
        $era1[] = $fn;
        $era2[] = $fn;
      }
    }
  }
  
  if(count($era1))
  {
    chdir($dir1); print "\tcd $dir1\n";
    $cmd = 'rm -f';
    foreach($era1 as $fn)$cmd .= ' '.shellfix($fn);
    print "\t$cmd\n";
    exec($cmd);
    chdir('..'); print "\tcd ..\n";
  }
  if(count($era2))
  {
    chdir($dir2); print "\tcd $dir2\n";
    $cmd = 'rm -f';
    foreach($era1 as $fn)$cmd .= ' '.shellfix($fn);
    print "\t$cmd\n";
    exec($cmd);
    chdir('..'); print "\tcd ..\n";
  }
  
  $cmd = 'LC_ALL=C LANG=C diff -NaHudr '.shellfix($dir1) . ' ' . shellfix($dir2).' > '.shellfix($patchname);
  print "\t$cmd\n";
  exec($cmd);
}
function FindInodes($directory)
{
  for($try = 0; $try < 10; $try++)
  {
    $fp = @opendir($directory);
    if($fp) break;
    print "OPENDIR $directory failed (cwd=".getcwd()."), retrying\n";
    sleep(1);
  }
  if($try == 10)
  {
    print "OPENDIR $directory failed (cwd=".getcwd().")!\n";
    exit;
  }
  
  $inofil = array();
  $filino = array();
  
  while(($fn = readdir($fp)))
  {
    if($fn=='.' || $fn=='..')continue;
    
    if($directory != '.')
      $fn = $directory.'/'.$fn;
    
    $st = stat($fn);
    $ino = $st[0].':'.$st[1];
    
    $inofil[$ino][] = $fn;
    $filino[$fn] = $ino;
    
    if(is_dir($fn))
    {
      $sub = FindInodes($fn);
      $filino = $filino + $sub['fi'];
      
      foreach($sub['if'] as $ino => $fil)
      {
        $tgt = &$inofil[$ino];
        if(is_array($tgt))
          $tgt = array_merge($tgt, $fil);
        else
          $tgt = $fil;
      }
      unset($tgt);
    }
  }
  closedir($fp);
  return array('if' => $inofil, 'fi' => $filino);
}
function MakePatch($progname, $v1, $v2, $paks1, $paks2)
{
  // print "Make patch for $progname $v1 - $v2\n";
  // Available packages for prog1: print_r($paks1);
  // Available packages for prog2: print_r($paks2);
  
  $v1 = preg_replace('/(-----)*$/', '', $v1);
  $v2 = preg_replace('/(-----)*$/', '', $v2);
  
  $v1string = preg_replace('/\.$/', '', preg_replace('|(.....)|e', '(str_replace("-","","$1"))."."', $v1));
  $v2string = preg_replace('/\.$/', '', preg_replace('|(.....)|e', '(str_replace("-","","$1"))."."', $v2));
  
  $files1 = Array();
  foreach($paks1 as $ext)
    $files1[] = $progname . '-' . $v1string . '.' . $ext;

  $files2 = Array();
  foreach($paks2 as $ext)
    $files2[] = $progname . '-' . $v2string . '.' . $ext;
  
  $keeplist = array_merge($files1, $files2);
  $dir1 = Open($files1, $keeplist);
  $dir2 = Open($files2, $keeplist);
  
  $patchname = "patch-$progname-$v1string-$v2string";
  
  MakeDiff($dir1, $dir2, $patchname);
  
  GoTmp();
  
  $cmd = "gzip -9 ".shellfix($patchname);
  print "\t$cmd\n";
  exec($cmd);
  
  $cmd = "gzip -d < ".shellfix($patchname). ".gz | bzip2 -9 > ".shellfix($patchname).".bz2";
  print "\t$cmd\n";
  exec($cmd);
  
  $cmd = "mv -f ".shellfix($patchname).".{gz,bz2} ../";
  print "\t$cmd\n";
  exec($cmd);
  
  UnGoTmp();

  $cmd = "touch -r".shellfix($files2[0])." ".shellfix($patchname).".{gz,bz2}";
  print "\t$cmd\n";
  exec($cmd);

  $cmd = "chown --reference ".shellfix($files2[0])." ".shellfix($patchname).".{gz,bz2}";
  print "\t$cmd\n";
  exec($cmd);

  global $argv;
  if(!$argv[3])
  {
    $cmd = 'ln -f '.shellfix($patchname).'.{gz,bz2} /WWW/src/arch/';
    print "\t$cmd\n";
    exec($cmd);
  }
}

$progs = array();
$fp = opendir('.');
$f = array();
while(($fn = readdir($fp)))
{
  if(preg_match('@\.sh\.(gz|bz2)$@', $fn))continue;
  if(preg_match('@^patch-.*-[0-9].*-[0-9].*\...*@', $fn))
  {
    preg_match(
      '/^patch-(.*(?:-opt)?)-([0-9][0-9.a-z-]*)-([0-9][0-9.a-z-]*)\.([a-z0-9]+)$/', $fn, $tab);
    // tab[0] = fn
    // tab[1] = progname
    // tab[2] = old version
    // tab[3] = new version
    // tab[4] = compression type (gz, bz2)

    $progname = $tab[1];
    $version1 = calcversion($tab[2]);
    $version2 = calcversion($tab[3]);
    $archtype = $tab[4];

    # print "patch prog {$tab[1]} vers1 {$tab[2]} vers2 {$tab[3]} comp {$tab[4]}\n";

    $progs[$progname]['p'][$version1][$version2][$archtype] = $archtype;
  }
  else
  {
    preg_match('/(.*(?:-opt)?)-((?!-opt)[0-9][0-9.a-z-]*)\.(tar\.[a-z0-9]+|zip|rar)$/', $fn, $tab);
    // tab[0] = fn
    // tab[1] = progname
    // tab[2] = version
    // tab[3] = archive type (tar.gz, tar.bz2, zip, rar)
    
    #print "$fn:\n";
    #print_r($tab);
    
    $progname = $tab[1];
    $version  = calcversion($tab[2]);
    $archtype = $tab[3];
    
    if($archtype != 'zip' && $archtype != 'rar')
    {
      # print "prog {$tab[1]} vers {$tab[2]} comp {$tab[3]}\n";
      
      $progs[$progname]['v'][$version][$archtype] = $archtype;
    }
  }
}
closedir($fp);

$argv[1] = preg_replace('@^[^-]*(?:-opt)?-([0-9])@', '\1', $argv[1]);
$wantversion = strlen($argv[1]) ? calcversion($argv[1]) : '';

foreach($progs as $progname => $data)
{
  $versions = $data['v'];
  ksort($versions);
  
  $wantpatches = Array();
  
  $verstabs = Array();
  for($c=1; $c<=6; $c++)
  {
    foreach($versions as $version => $paks)
    {
      $k = substr($version, 0, $c*5);
      $verstabs[$c][$k] = $k;
    }
    asort($verstabs[$c]);
  }

  $lastversio = '';
  foreach($versions as $version => $paks)
  {
    if($lastversio)
    {
      $did = Array($lastversio => 1);
      
      $wantpatches[$lastversio][$version] = 1;
      
      for($c=1; $c<=5; ++$c)
      {
        $prev = '';
        $k = substr($version, 0, $c*5);
        
        $test = str_pad($k, 5*6, '-');
        if($test != $version
        && isset($versions[$test])) continue;
        foreach($verstabs[$c] as $k2)
        {
          if($k2 == $k)break;
          $prev = $k2;
        }
        
        if($prev)
        {
          $prev = str_pad($prev, 5*6, '-');
          if(isset($versions[$prev]) && !$did[$prev])
          {
            $did[$prev] = 1;
            
            // Extra
            $wantpatches[$prev][$version] = 1;
          }
        }
      }
    }
    $lastversio = $version;
  }
  
  foreach($wantpatches as $version1 => $v2tab)
  {
    foreach($v2tab as $version2 => $dummy)
    {
      if(strlen($wantversion))
      {
        if($version2 != $wantversion)
        {
          continue;
        }
      }
      else if(isset($progs[$progname]['p'][$version1][$version2]))
      {
        continue;
      }
      
      MakePatch($progname, $version1, $version2,
                $versions[$version1],
                $versions[$version2]);
    }
  }
}
print_r($thisdir);
CloseAll();
UnGoTmp();
