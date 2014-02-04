<?php

/*

create the JSON that drives the front page 'now playing' display

*/

$output_file = "/var/www/html/nowplaying.json";

date_default_timezone_set('Europe/London');
$tm = gettimeofday();

$delta = $tm['minuteswest'];

// get config JSON
$raw = file_get_contents('/home/ec2-user/delayplayer/stations.json');
$stations = json_decode($raw);

$json_data = array();

foreach($stations->stations as $station)
{
	$station_info = new StationInfo();
	$station_info->name = $station->name;
	$station_info->id = $station->id;
	$station_info->logo = $station->logo;
	$station_info->source_utc_offset = $delta;

	$raw = file_get_contents($station->feed.'yesterday.json', NULL, NULL, 0, 65536);
	$data = json_decode($raw);
	$yesterday = getProgrammeDetails($data);

	$raw = file_get_contents($station->feed.'today.json', NULL, NULL, 0, 65535);
	$data = json_decode($raw);
	$today = getProgrammeDetails($data);

	$schedule = array_merge($yesterday, $today);

	$nowplaying = array();

	foreach($station->zones as $zone)
	{
		$stream = new StreamInfo();
		$stream->name = $zone->name;
		$stream->delay = $zone->offset;
		$stream->stream_url = $zone->stream;
		$obj = getNowPlaying($zone->offset,$schedule);
		
		if($obj==null)
		{
			echo "getNowPlaying returned null";
			exit;
		}

		$stream->programmeDetails=$obj;

		array_push($station_info->streamInfoArray,$stream);
	}
	array_push($json_data,$station_info);
}

print_r($json_data);

// write json file to www directory for client AJAX to pick up
$data = json_encode($json_data);
file_put_contents($output_file,$data);

exit;

function getNowPlaying($offset,$schedule)
{
	// now for each offset, get the current programme
	$time = time() - ($offset*60);

	$programme = getProgrammeFromEpoch($time,$schedule);

	if($programme==null)
	{
		return null;
	}

	return $programme;
}

function getProgrammeFromEpoch($time,$schedule)
{
	$debug="";
	
	// return first programme that our time falls between
	// (assume programmes don't overlap)
	foreach($schedule as $item)
	{
		$debug+=$item->title." time: ".$time." - ";
		$debug+=$item->start."-".$item->end."\n";

		if($time >= $item->start && $time < $item->end)
		{
			return $item;
		}
	}
	
	echo $debug;
	
	return null;
}

function getProgrammeDetails($schedule)
{
	$broadcasts = $schedule->schedule->day->broadcasts;
	$details = array();

	foreach($broadcasts as $broadcast)
	{
		$programme = $broadcast->programme;
		$detail = ProgrammeDetails::programmeDetailsFromProgramme($broadcast);
		array_push($details,$detail);
	}

	return $details;
}


class StationInfo
{
	public $name;
	public $logo;
	public $id;
	public $streamInfoArray;
	public $source_utc_offset;

    public function __construct()
    {
		$this->name="";
		$this->streamInfoArray=array();
		$this->id="";
	}
}

// a delayed stream
class StreamInfo
{
	public $name;
	public $stream_url;
	public $delay;
	public $programmeDetails;
}

class ProgrammeDetails
{
	public $title;
	public $pid;
	public $type;
	public $subtitle;
	public $start;
	public $end;
	public $duration;

	public $display_start;
	public $display_end;
	public $display_duration;

    private function __construct()
    {
		$this->title	= "";
		$this->pid	= "";
		$this->type	= "";
		$this->subtitle	= "";
		$this->start	= 0;
		$this->end		= 0;		
		$this->duration	= 0;
		$this->display_start="";
		$this->display_end="";
		$this->display_duration="";
    }

	public static function emptyProgrammeDetails()
	{
		return new ProgrammeDetails();		
	}
	
	public static function programmeDetailsFromProgramme($broadcast)
	{
		$programme = $broadcast->programme;

		$obj = new ProgrammeDetails();		
		$obj->title	= $programme->display_titles->title;
		$obj->pid	= $programme->programme->pid;
		$obj->subtitle	= $programme->display_titles->subtitle;
		$obj->type	= $programme->programme->type;

		if($obj->subtitle=="")
		{
			// stop the programme block from collapsing
			$obj->subtitle="&nbsp;";
		}
		
		$time = strtotime($broadcast->start);
		$obj->start	= $time;

		$obj->end	= strtotime($broadcast->end);//$obj->start + $programme->duration;

		$obj->display_start		= date("H:i", $time);
		$obj->display_end		= date("H:i", $obj->end);
		$obj->display_duration	= date("H:i", $programme->duration);
		$obj->duration = $programme->duration;

		return $obj;
	}
	
	public function dump()
	{
		print $this->title."<br/>";
		print "&nbsp;&nbsp;&nbsp;&nbsp;Start ".$this->start."<br/>";
		print "&nbsp;&nbsp;&nbsp;&nbsp;End ".$this->end."<br/>";
		print "&nbsp;&nbsp;&nbsp;&nbsp;Description ".$this->subtitle."<br/>";
		print "&nbsp;&nbsp;&nbsp;&nbsp;".$this->display_start."<br/>";
		print "&nbsp;&nbsp;&nbsp;&nbsp;".$this->display_end."<br/>";
		print "&nbsp;&nbsp;&nbsp;&nbsp;".$this->display_duration."<br/>";
		print "&nbsp;&nbsp;&nbsp;&nbsp;Duration ".$this->duration."<br/>";
		print "<br/>";
	}
}
