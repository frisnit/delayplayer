delayplayer
===========

A collection of scripts to delay live streaming radio services for synchronising with different time zones. This is very beta.

See http://www.frisnit.com/2012/03/12/delayplayer-radio-4-synced-to-your-timezone/ for full details

The system consists of two main parts:

Ingest
------
>This connects to the remote AAC audio streams, transcodes them to MP3 using ffmpeg and saves them as timestamped files  
>in chunks of about ten minutes duration.

Playout
-------
> An Icecast server operating in relay mode maintains a number of streams endpoint for each station and timezone. Each of these connects to an instance of a lightweight server that streams the timestamped chunks from the appropriate point in the past.

There are a number of other supporting systems:

Now playing
-----------
> The front page HTML is generated from the radio schedule by a cron job that fires off every minute or so

Monitoring
----------
> The system status is monitored by checking the injest files are being created and that the Icecast endpoints are still available. This is performed by a simple .php script run from cron every five minutes

