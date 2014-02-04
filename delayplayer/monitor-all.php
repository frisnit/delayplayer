<?php

/*

monitor delayplayer stream ingest (make sure new files are being generated)
monitor delayplayer shoutcast endpoints (make sure we can connect to endpoints)

*/


date_default_timezone_set('UTC');

	header("Expires: Mon, 26 Jul 1990 05:00:00 GMT");
	header("Last-Modified: " . gmdate("D, d M Y H:i:s") . " GMT");
	header("Cache-Control: no-store, no-cache, must-revalidate");
	header("Cache-Control: post-check=0, pre-check=0", false);
	header("Pragma: no-cache");
	
	
	
$data = @file_get_contents("/home/ec2-user/delayplayer/monitor_state.json");

if($data==null)
{
	echo "Creating config\r\n";
	
	$data['one'] = "";
	$data['two'] = "";
	$data['four'] = "";
}
else
{	
	$data = json_decode($data,true);
}

$one	= getLastFileName("/home/ec2-user/delayplayer/transcoder/radio_one/");
$two	= getLastFileName("/home/ec2-user/delayplayer/transcoder/radio_two/");
$four	= getLastFileName("/home/ec2-user/delayplayer/transcoder/radio_four/");

$result = array();

$result['one'] = compareFiles($one, $data['one']);
$result['two'] = compareFiles($two, $data['two']);
$result['four'] = compareFiles($four, $data['four']);

//print_r($result);

file_put_contents("/home/ec2-user/delayplayer/html/monitor.json",json_encode($result));

// save state
$data['one'] = $one;
$data['two'] = $two;
$data['four'] = $four;

//print_r($data);

file_put_contents("/home/ec2-user/delayplayer/monitor_state.json",json_encode($data));
	
	
	
	
?>

<html>
<head>
<title>Delayplayer monitoring</title>
</head>
<body>

<?php

$data = @file_get_contents("/home/ec2-user/delayplayer/monitor_state.json");

if($data==null)
{
?>

<h2>No data</h2>

<?php
}
else
{	
	$data = json_decode($data,true);
?>

<h1><?php echo date('D, d M Y H:i:s');?></h1>

<h2>Stream ingest</h2>
<h3>Radio One <?php showStatus($data['one'],'stream_to_pipe.pl')?></h3>
<h3>Radio Two <?php showStatus($data['two'],'r2_to_pipe.pl')?></h3>
<h3>Radio Four <?php showStatus($data['four'],'r4_to_pipe.pl')?></h3>

<h2>Shoutcast streams</h2>
<h3>Radio One EDT <?php testUrl("http://www.delayplayer.com:8000/radio_one/edt.mp3")?></h3>
<h3>Radio One PDT <?php testUrl("http://www.delayplayer.com:8000/radio_one/pdt.mp3")?></h3>
<h3>Radio One AU EDT <?php testUrl("http://www.delayplayer.com:8000/radio_one/au_edt.mp3")?></h3>
<h3>Radio One HKT <?php testUrl("http://www.delayplayer.com:8000/radio_one/hkt.mp3")?></h3>
<hr/>

<h3>Radio Two EDT <?php testUrl("http://www.delayplayer.com:8000/radio_two/edt.mp3")?></h3>
<h3>Radio Two PDT <?php testUrl("http://www.delayplayer.com:8000/radio_two/pdt.mp3")?></h3>
<h3>Radio Two AU EDT <?php testUrl("http://www.delayplayer.com:8000/radio_two/au_edt.mp3")?></h3>
<h3>Radio Two HKT <?php testUrl("http://www.delayplayer.com:8000/radio_two/hkt.mp3")?></h3>
<hr/>

<h3>Radio Four EDT <?php testUrl("http://www.delayplayer.com:8000/radio_four/edt.mp3")?></h3>
<h3>Radio Four PDT <?php testUrl("http://www.delayplayer.com:8000/radio_four/pdt.mp3")?></h3>
<h3>Radio Four AU EDT <?php testUrl("http://www.delayplayer.com:8000/radio_four/au_edt.mp3")?></h3>
<h3>Radio Four HKT <?php testUrl("http://www.delayplayer.com:8000/radio_four/hkt.mp3")?></h3>
<hr/>

<?php


}

?>

</body>
</html>

<?php

function testUrl($url)
{
	$result = get_headers($url);
	
	showStatus(is_array($result) && strstr($result[0],"200")!==false);
}

function showStatus($status,$process=null)
{
	echo "<img src=\"";
	
	if($status==true)
		echo './good.gif';
	else
	{
		if($process!=null)
		{
			// kill stalled ingest process
			exec('kill $(pgrep '.$process.')');
			exec('kill $(pgrep '.$process.')');
		}
		echo './bad.gif';
	}
	echo "\">";
}

function compareFiles($new, $old)
{
	if($new != $old)
	{
		return true;
	}
	else
	{
		return false;
	}
}

function getLastFileName($dir)
{

//echo $dir."\r\n";

	$lastMod = 0;
	$lastModFile = '';
	foreach (scandir($dir) as $entry)
	{
		if(is_file($dir.$entry) && filectime($dir.$entry) > $lastMod)
		{
			$lastMod = filectime($dir.$entry);
			$lastModFile = $entry;
		}
	}

//echo $lastModFile."\r\n";

	
	return $lastModFile;
}


