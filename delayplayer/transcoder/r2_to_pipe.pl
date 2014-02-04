#!/usr/bin/perl -w 

use strict; 
use IO::Socket; 
use Getopt::Long;
use HTML::Entities;
use POSIX qw( strftime );
use POSIX qw/setuid setsid/;

my $config = {};
my $def_agent = "icecream/1.3";
my $version = "icecream/1.3";
my $accept_header = "audio/mpeg, audio/x-mpegurl, audio/x-scpls, */*";
my $def_timeout = 500;
my $max_sync_count = 3;
my $largest_unknown_buffer = 1024**2;


sub extract_status_code
{
	my ($message) = shift || return undef;

	if ($message !~ /^(.+)\s+(\d+)/) 
	{
		return undef;
	}
	
	return $2;
}

sub select_socket
{
	my ($handle) = shift || return 0;
	my ($timeout) = shift || return 0;
	my ($v) = '';

	vec($v, fileno($handle), 1) = 1;
	return select($v, $v, $v, $timeout / 1000.0);
}

sub recv_chunk
{
	my ($handle) = shift || return undef;
	my ($cnt) = shift || return undef;
	my ($data) = '';
	
	while ($cnt != 0) 
	{
		my ($chunk, $chunksize);
		my ($next_chunk);
		
		$next_chunk = ($cnt > 0) ? $cnt : 1024;

		if (select_socket($handle, $def_timeout) <= 0) 
		{
			# timed out
			print STDERR "Timedout!\n";
			last;
		}
		
		$handle->recv($chunk, $next_chunk);
		$chunksize = length($chunk);		
		if ($chunksize == 0) 
		{
			# error occured, or end of stream
			last;
		}	
		
		$data .= $chunk;
		$cnt -= $chunksize;
		
		# paranoia, what if a bigger chunk is received
		$cnt = 0 unless $cnt > 0;
	}
	
	return $data;
}


sub debug
{
	my $title = shift || return;
	my $additional = shift;

	if ($config->{debug}) 
	{
		print "[ $title ]\n";
		if (defined $additional) 
		{
			my @ar = split("\n", $additional);
			foreach my $s (@ar) 
			{
				print "\t$s\n";
			}
		}
	}
}

# write from the stream until it terminates (hopefully never)
sub loop_anonymous_stream
{
	my ($sock) = shift || return 0;
	my ($context) = {};

	debug("loop_anonymous_stream()");

	my $block_count=0;

	while($sock->connected) 
	{
		my $chunk = recv_chunk($sock, 6000);
		
		if(length($chunk)>0)
		{
			# write data to pipe
			print STDOUT $chunk;
		}
		else
		{
			print STDERR "Disconnected\n";
			return 1;
		}
	}
	
	debug("loop_anonymous_stream ended");
	return 2;
}

sub strip_protocol
{
	my ($url) = shift || return undef;

	if ($url =~ /^\w+:\/\/(.+)$/) 
	{
		return $1;
	}

	return $url;
}

sub slurp_headers
{
	my ($sock) = shift || return undef;
	my ($max_length) = shift || -1;
	my ($data);
	my ($headers) = '';
	
	return "" if ($max_length == 0);
	
	$data = recv_chunk($sock, 1);
	while (defined $data) 
	{
		$headers .= $data;
		last if $headers =~ /\r\n\r\n/;
		
		if ($max_length != -1 && length($headers) >= $max_length) 
		{
			# just enough (we're reading one byte at a time)
			last;
		}
			
		$data = recv_chunk($sock, 1);
	}
	
	return $headers;
}

sub trim
{
	my ($str) = shift || return undef;
	
	$str =~ s/^[\s\t]//g;
	$str =~ s/[\s\t]$//g;
	return $str;
}


sub split_url
{
	my ($url) = shift || return undef;
	my ($host, $port, $path);

print STDERR "url: $url\n";

	$port = undef;
	
	if ($url =~ /^([\d\w\._\-]+)(:\d+)??(\/.*)??$/) 
	{
		$host = $1;
		if (defined $2) 
		{
			# port includes the colon
			$port = substr($2, 1);
		}
		
		$path = $3;
	} 
	else 
	{
		# unparsable
		print "*** UNPARSABLE ***\n";
		return undef;
	}

print STDERR "path $path\n";

	return ($host, $port, $path);	
}
		
sub split_protocol
{
	my ($url) = shift || return undef;

	if ($url =~ /^(\w+):\/\//) 
	{
		return $1;
	}

	return undef;
}

sub start_stream
{
	my ($host, $port, $path);
	my ($sock, $headers);
	my ($status);
	my ($stream_data);

#	do{
		my $full_location = get_stream_url();
		print STDERR $full_location."\n";

		if(split_protocol($full_location) ne "http") 
		{
			print STDERR "error: not an http location $full_location\n";
			return 0;
		}

		my $location = strip_protocol($full_location);
			
		# parse location
		($host, $port, $path) = split_url($location);
	
		# parsing errors?
		if (! defined $host) 
		{
			print STDERR "error parsing url $location\n";
			return 0;
		}

		$port = 80 unless defined $port;
		$path = "/" unless defined $path;

		do
		{
			$sock = IO::Socket::INET->new(PeerAddr => $host, PeerPort => $port, Proto => 'tcp', Timeout => 10);
			if (! defined $sock) 
			{
				print STDERR "error connecting to $host:$port\n";
				return 0;
			}

			my $agent = $config->{'user-agent'};
			my $request = "GET $path HTTP/1.0\r\n" .
				"Host: $host:$port\r\n" .
				"Accept: ${accept_header}\r\n" .
				"Icy-MetaData:0\r\n" .
				"User-Agent:$agent\r\n" .
				"\r\n";

			debug("sending request to server", $request);
			print $sock $request;

			$headers = slurp_headers($sock);
			if (! defined $headers) 
			{
				print STDERR "error retreiving response from server\n";
				return 0;
			}

			debug("data retreived from server", $headers);

			$status = extract_status_code($headers);

			debug("status code ".$status);

			if (! defined $status) 
			{
				print STDERR "error parsing server response (use --debug)\n";
				return 0;
			}

			elsif ($status == 302) 
			{
				# relocated
				$location = get_302_location($headers);
			}

			elsif ($status == 400) 
			{
				# server full
				print STDERR "error: server is full (use --debug for complete response)\n";
				return 0;
			}

			elsif ($status != 200) 
			{
				# nothing works fine these days
				print STDERR "error: server error $status (use --debug for complete response)\n";
				return 0;
			}

		} while ($status != 200);

		# read stream and dump to disk
		debug("calling loop_anonymous_stream" );

	#}while(loop_anonymous_stream($sock)==0);

	loop_anonymous_stream($sock);

	debug("loop_anonymous_stream returned" );

	return 1;
}

sub get_stream_url
{
	my $location="http";
	my $host="open.live.bbc.co.uk";
	my $port="80";
#	my $path="/mediaselector/4/mtis/stream/".$config->{'service'};
	my $path="/mediaselector/5/select/version/2.0/vpid/".$config->{'service'}."/mediaset/http-icy-aac-lc-a";

	my $sock = IO::Socket::INET->new(PeerAddr => $host, PeerPort => $port, Proto => 'tcp', Timeout => 10);
	if (! defined $sock) 
	{
		print STDERR "error connecting to $host:$port\n";
		return 0;
	}

	my $agent = $config->{'user-agent'};
	my $request = "GET $path HTTP/1.0\r\n" .
		"Host: $host:$port\r\n" .
		"Accept: ${accept_header}\r\n" .
		"User-Agent:none\r\n" .
		"\r\n";

	debug("sending request to server", $request);
	print $sock $request;

	my $headers = slurp_headers($sock);
	my $status = extract_status_code($headers);

	my $chunk = recv_chunk($sock, 100000);

	print STDERR "Requesting playlist - response length: ".length($chunk)."\n";

#	print STDERR $chunk;

#	if($chunk =~ /icy_uk_stream_aac_low_live(.*?)\/>/) 
#	if($chunk =~ /icy_uk_stream_aac_mob_live(.*?)\/>/) 
#	if($chunk =~ /_stream_aac_high_live(.*?)\/>/) 
	{
#		$chunk = $1;
#		print STDERR $chunk."\n";
#		if($chunk =~ /<connection application.*?href="(.*?)"/) 
		if($chunk =~ /href="(.*?)"/)
		{			
			print STDERR $1."\n";
			my $url = decode_entities($1);
#			print STDERR $1."\n";
			return $url;
		}
	}

	die "Can't find stream URL";

	return "";
	
}

# check if already running
my $is_running = `ps -A | grep -c r2_to_pipe 2>&1`;

if($is_running>1)
{
	print STDERR "Streamer R2 is already running\n";
#        die "Streamer is already running\n";

	exit;
}


#        print "Running as daemon\n";
#
 #       #chdir '/';
  #      umask 0;
   #     open STDIN, '/dev/null';
    #    open STDERR, '>/dev/null';
     #   defined(my $pid = fork);
      #  exit if $pid;
       # setsid;





my (%options) = ();

GetOptions(\%options, "--user-agent=s", "--service=s");

$config->{'user-agent'} = (defined $options{'user-agent'}) ? $options{'user-agent'} : ${def_agent}; 
$config->{'service'} = (defined $options{'service'}) ? $options{'service'} : die "Define service id with --service"; 


$config->{debug}=0;

print STDERR "Starting...\n";

#my $full_location = get_stream_url();
#print STDERR $full_location."\n";
start_stream();
print STDERR "Finished.\n";
exit;
