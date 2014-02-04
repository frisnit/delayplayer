

# start radio 1
/home/ec2-user/delayplayer/transcoder/stream_to_pipe.pl --service bbc_radio_one    | /usr/local/bin/ffmpeg -i pipe:0 -f mp3 -acodec libmp3lame -b:a 56k -ar 32000 pipe:1 2> /dev/null | /home/ec2-user/delayplayer/transcoder/save_from_pipe.pl --path /home/ec2-user/delayplayer/transcoder/radio_one/  > /dev/null 2>&1 &

# start radio 2
/home/ec2-user/delayplayer/transcoder/r2_to_pipe.pl --service bbc_radio_two | /usr/local/bin/ffmpeg -i pipe:0 -f mp3 -acodec libmp3lame -b:a 56k -ar 32000 pipe:1  2> /dev/null | /home/ec2-user/delayplayer/transcoder/save_from_pipe.pl --path /home/ec2-user/delayplayer/transcoder/radio_two/ > /dev/null 2>&1 &

# start radio 4
/home/ec2-user/delayplayer/transcoder/r4_to_pipe.pl --service bbc_radio_fourfm | /usr/local/bin/ffmpeg -i pipe:0 -f mp3 -acodec libmp3lame -b:a 56k -ar 32000 pipe:1 2> /dev/null | /home/ec2-user/delayplayer/transcoder/save_from_pipe.pl --path /home/ec2-user/delayplayer/transcoder/radio_four/ > /dev/null 2>&1 &

