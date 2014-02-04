
var old_selector=null;
var old_station=null;
var selected_programme=null;

var current_info="";
var flash_obj="";

var last_url;
var last_title;
var last_caption;

var state=0;

// direct links for Flash-averse devices
var useFlash;

var audio = $("<audio>").attr("id", "player")
                        .attr("preload", "auto");

function change(sourceUrl)
{
    var audio = $("player");      
    $("#ogg_src").attr("src", sourceUrl);
    /****************/
    audio[0].pause();
    audio[0].load();//suspends and restores all audio element
    audio[0].play();
    /****************/
}

function createPlayer(url,title,caption)
{
	last_url = url;
	last_title = title;
	last_caption = caption;

	var flashvars =
	{
		url: url,
		codec: "mp3",
		lang: "auto",
		volume: "100",
		buffering: "5",
		title: title,
		autoplay: "true",
		backgroundColor: "#fff",
		tracking: "false",
		jsevents: "true",
		skin: "player-config.xml"
	};
	var params = {
	  menu: "false",
	  bgcolor: "#fff"
	};
	var attributes = {
	  allowfullscreen: "true"
	};

	if(url!=null)
	{
		swfobject.embedSWF("ffmp3.swf", "flashcontent", "0px", "0px", "9","expressInstall.swf", flashvars, params, attributes,callbackFn);
	}
	
		current_info=caption;
}

function Point(x, y)
{
	this.x = x;
	this.y = y;
}

// get size (height) of this string
function getTextSize(str,size,width,weight)
{
	var element = document.getElementById("size_test");
	element.innerHTML=str;
	element.style.fontSize=size;
	element.style.width=width+'px';
	element.style.fontWeight=weight;
	
	// now measure the size of the div
	var height = element.clientHeight;
	var width = element.clientWidth;	
	
	return new Point(width, height);
}

// truncate and ellipsize some text given its max dimensions
function truncateText(str, size, maxWidth, maxHeight, weight)
{	
	// simple truncate - doesn't attempt to ellipsize on word boundaries	
	var point = getTextSize(str,size,maxWidth,weight);
	
	if(point.x>maxWidth || point.y>maxHeight)
	{
		while((point.x>maxWidth || point.y>maxHeight) && str.length>0)
		{
			str = str.substr(0,str.length-1);
			point = getTextSize(str+'……',size,maxWidth,weight);
		}

		str += '…';

	var element = document.getElementById("size_test");
	element.innerHTML=str+' '+point.x+'x'+point.y;

	}
	
	return str;
}

// embedSWF callback
function callbackFn(e)
{
	flash_obj = e.id;
}

function update()
{
	$.getJSON("http://cdn.frisnit.com/nowplaying.php?jsoncallback=?",
	function(response)
	{
		if(typeof(response) != 'undefined')
		{
			$.each(response, function(i,item)
			{
				var html = "";
				var station = item.name;

				html +='<div class="spacer"></div>';

				$.each(item.streamInfoArray, function(i,programme)
				{
					var title = programme.programmeDetails.title.replace(/'/g, "\\'");
					var display_title = programme.programmeDetails.title;

					display_title = truncateText(display_title, 15, 178, 36, 900);

/*
					if(display_title.length>30)
					{
						display_title = display_title.substr(0,25);
						display_title = display_title + "...";
					}
*/

					var delay = new Number(programme.delay/60);
				
					delay += item.source_utc_offset/60;
					
					var gmt;
					if(delay>12)
					{
						// east
						gmt = '+'+(24-delay);
					}
					else
					{
						// west 
						gmt = "-"+delay;
					}

					var safe_name = programme.name.replace("'", "\\'");
					// _86_48.jpg
					html +='<div class="programme">';
					html +='<div class="location">'+safe_name+' (UTC'+gmt+')</div>';

					if(useFlash)
					{
						html +='<a href="#" onclick="createPlayer(\''+programme.stream_url+'\',\''+title+'\',\'Now playing: '+station+' ('+safe_name+')\');">';
					}
					else
					{
						html +='<a href="'+programme.stream_url+'" target="playerFrame" >';
					}
					
					html +='<div class="programme_image">';

					var imagebase;

					// type brand/series
					if(programme.programmeDetails.type=='brand')
					{
						imagebase ='http://www.bbc.co.uk/iplayer/images/progbrand/'+programme.programmeDetails.pid;
					}
					else
					{
						imagebase ='http://www.bbc.co.uk/iplayer/images/series/'+programme.programmeDetails.pid;
					}

					if(programme.programmeDetails.title!='N/A' && programme.programmeDetails.pid!=null)
					{
						html +='<img onerror="this.style.display=\'none\'" border="0" src="'+imagebase+'_178_100.jpg">';
					}

					html +='<div class="play_icon"><img src="play.png"></div>';
					html +='</div>';
					html +='</a>';

					html +='<div class="title">'+display_title+'</div>';

					var display_subtitle = truncateText(programme.programmeDetails.subtitle, 10, 178, 30, 'normal');
					html +='<div class="subtitle">'+display_subtitle+'</div>';

					html +='<div class="time">'+programme.programmeDetails.display_start+'-'+programme.programmeDetails.display_end+'</div>';

					if(programme.programmeDetails.title!='N/A')
					{
						var url=escape("http://www.bbc.co.uk/programmes/"+programme.programmeDetails.pid);
					
						var text = escape('I\'m listening to '+programme.programmeDetails.title+' on '+station+', aligned to UTC'+gmt+' with Delayplayer!');

						text = text.replace('+','%2b');
						
						var facebook =
							'https://www.facebook.com/dialog/feed?app_id=353258424717130&'
							+'name='+programme.programmeDetails.title+' - '+programme.programmeDetails.subtitle+'&'
							+ 'caption=delayplayer.com&'
							+ 'description='+text+'&'
							+ 'redirect_uri=http://www.delayplayer.com/';
						
						if(programme.programmeDetails.pid!=null)
						{
							facebook +=
							 '&link='+url+'&'
							+'picture='+imagebase+'_178_100.jpg&';
						}
						else
						{
							facebook += '&link=http://www.delayplayer.com/&';
						}
						
						var twitter =
						'https://twitter.com/intent/tweet?'
						+'url='+url+'&'
						+'via=delayplayer&'
						+'&text='+text;
						
						html +='<div class="social">&nbsp;';
						html +='<a href="'+facebook+'" target="_blank"><img src="facebook-small.png"></a>&nbsp;&nbsp;&nbsp;&nbsp;';
						html +='<a href="'+twitter+'" target="_blank"><img src="twitter-small.png"></a>';
						html +='</div>';

					}

					html += '</div>';
				});

				html +='<div class="spacer"></div>';
				$('#'+item.id).html(html);
			});
		}
	});
}
var refresh = setInterval(function(){update();}, 120000);

function selectStation(selector,station)
{
	if(old_station==station)
	{
		return;
	}
	
	// select new selection
	selector.className = "tab-selected";
	document.getElementById(station).style.visibility='visible'; 

	// deselect old selection
	if(old_station!=null && old_selector!=null)
	{
		old_selector.className = "tab";
		document.getElementById(old_station).style.visibility='hidden'; 
	}
	
	old_selector=selector;
	old_station=station;
}

$(document).ready(function()
{
	update();
	
	useFlash = navigator.userAgent.match(/iPad/g)==null;
	
	//alert(useFlash);
	
	// enable Flash player
	if(useFlash)
	{
		createPlayer(null,'Select your timezone','Select channel and timezone then click to play');
	}
	else
	{
		$("#flashContainer").css("visibility", "hidden");
	}
	
	selectStation(document.getElementById('default-station'),'radio_one');
});

function setImage(name,src)
{
	document[name].src=src;
}

function stop()
{
	setImage('transport','playerskin/play.gif');
	document.getElementById("info").innerHTML="Stopped";
}

function transportClick()
{
	if(state==0)
	{
		createPlayer(last_url,last_title,last_caption);
		state=1;
	}
	else
	{
		setImage('transport','playerskin/play.gif');
		document.getElementById("info").innerHTML="Stopped";
		swfobject.removeSWF("flashcontent");
		$('#player-container').prepend("<div id='flashcontent'></div>");
		state=0;
	}
}

function ffmp3Callback(event,param)
{
	switch(event)
	{
		case "buffering":
			setImage('transport','./spinner.gif');
			document.getElementById("info").innerHTML="Buffering...";
			state=1;
			break;
		case "play":
			setImage('transport','playerskin/stop.gif');
			document.getElementById("info").innerHTML=current_info;
			state=1;
			break;
		case "stop":
			setImage('transport','playerskin/play.gif');
			document.getElementById("info").innerHTML="Stopped";
			state=0;
			break;
	}
}
