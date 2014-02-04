
var old_selector=null;
var old_station=null;
var selected_programme=null;

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

//				html +='<div class="spacer"></div>';
				$.each(item.streamInfoArray, function(i,programme)
				{
					var title = programme.programmeDetails.title.replace("'", "\\'");
					var display_title = programme.programmeDetails.title;

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
					html +='<a href="'+programme.stream_url+'">';
					html +='<div class="programme">';
					html +='<div class="location">'+safe_name+' (UTC'+gmt+')</div>';

					html +='<div class="programme_image">';

					if(programme.programmeDetails.title!='N/A' && programme.programmeDetails.pid!=null)
					{
						// type brand/series
						if(programme.programmeDetails.type=='brand')
						{
							html +='<img onerror="this.style.display=\'none\'" border="0" src="http://www.bbc.co.uk/iplayer/images/progbrand/'+programme.programmeDetails.pid+'_86_48.jpg">';
						}
						else
						{
							html +='<img onerror="this.style.display=\'none\'" border="0" src="http://www.bbc.co.uk/iplayer/images/series/'+programme.programmeDetails.pid+'_86_48.jpg">';
						}
					}
					
					html +='<div class="play_icon"><img src="play.png"></div>';
					html +='</div>';

					html +='<div class="title">'+display_title+'</div>';
					html +='<div class="subtitle">'+programme.programmeDetails.subtitle+'</div>';
					html +='<div class="time">'+programme.programmeDetails.display_start+'-'+programme.programmeDetails.display_end+'</div>';
/*
					if(programme.programmeDetails.title!='N/A')
					{
						var url=escape("http://www.bbc.co.uk/programmes/"+programme.programmeDetails.pid);
						var delay = new Number(programme.delay/60);
						
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
						
						var text = escape('I\'m listening to '+programme.programmeDetails.title+' on '+station+', aligned to GMT'+gmt+' with Delayplayer!');

						text = text.replace('+','%2b');
						
						var facebook =
							'https://www.facebook.com/dialog/feed?app_id=353258424717130&'
							+'name='+programme.programmeDetails.title+' - '+programme.programmeDetails.subtitle+'&'
							+ 'caption=delayplayer.com&'
							+ 'description='+text+'&'
							+ 'redirect_uri=http://www.delayplayer.com/demo';
						
						if(programme.programmeDetails.pid!=null)
						{
							facebook +=
							 'link='+url+'&'
							+'picture=http://www.bbc.co.uk/iplayer/images/progbrand/'+programme.programmeDetails.pid+'_178_100.jpg&';
						}
						else
						{
							facebook += 'link=http://www.delayplayer.com/&';
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
*/
					html += '</div>';
					html +='</a>';
				});

//				html +='<div class="spacer"></div>';
				$('#'+item.id).html(html);
				$('#foot').html('Delayplayer &copy;2012<br/><br/>');
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
	selectStation(document.getElementById('default-station'),'radio_one');
});

function setImage(name,src)
{
	document[name].src=src;
}
